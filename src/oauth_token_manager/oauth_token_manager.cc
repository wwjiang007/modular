// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <iostream>
#include <memory>

#include "application/lib/app/application_context.h"
#include "application/lib/app/connect.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/lib/rapidjson/rapidjson.h"
#include "apps/modular/services/auth/account_provider.fidl.h"
#include "apps/modular/services/auth/token_provider.fidl.h"
#include "apps/mozart/services/views/view_provider.fidl.h"
#include "apps/mozart/services/views/view_token.fidl.h"
#include "apps/network/services/network_service.fidl.h"
#include "apps/web_runner/services/web_view.fidl.h"
#include "lib/fidl/cpp/bindings/array.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/fidl/cpp/bindings/string.h"
#include "lib/ftl/files/directory.h"
#include "lib/ftl/files/file.h"
#include "lib/ftl/files/path.h"
#include "lib/ftl/macros.h"
#include "lib/ftl/strings/join_strings.h"
#include "lib/ftl/strings/string_number_conversions.h"
#include "lib/ftl/synchronization/sleep.h"
#include "lib/mtl/socket/strings.h"
#include "lib/mtl/tasks/message_loop.h"
#include "lib/mtl/vmo/strings.h"

namespace modular {
namespace auth {

namespace {

// TODO(alhaad/ukode): Move the following to a configuration file.
// NOTE: We are currently using a single client-id in Fuchsia. This is temporary
// and will change in the future.
constexpr char kClientId[] =
    "1051596886047-kjfjv6tuoluj61n5cedv71ansrj3ggi8.apps."
    "googleusercontent.com";
constexpr char kGoogleOAuthEndpoint[] =
    "https://accounts.google.com/o/oauth2/v2/auth";
constexpr char kRedirectUri[] = "com.google.fuchsia.auth:/oauth2redirect";
constexpr char kTokensFile[] = "/data/modular/device/tokens.db";
constexpr char kWebViewUrl[] =
    "https://fuchsia-build.storage.googleapis.com/apps/web_view/"
    "web_view_b034f1a588a20e87198b83fd37f4ff676b093b6c";

constexpr auto kScopes = {"https://www.googleapis.com/auth/gmail.modify",
                          "https://www.googleapis.com/auth/userinfo.email",
                          "https://www.googleapis.com/auth/userinfo.profile",
                          "https://www.googleapis.com/auth/youtube.readonly",
                          "https://www.googleapis.com/auth/contacts",
                          "https://www.googleapis.com/auth/plus.login"};

// TODO(alhaad/ukode): Don't use a hand-rolled version of this.
std::string UrlEncode(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (std::string::const_iterator i = value.begin(), n = value.end(); i != n;
       ++i) {
    std::string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '=' ||
        c == '&' || c == '+') {
      escaped << c;
      continue;
    }

    // Any other characters are percent-encoded
    escaped << std::uppercase;
    escaped << '%' << std::setw(2) << int((unsigned char)c);
    escaped << std::nouppercase;
  }

  return escaped.str();
}
void Post(const std::string& request_body,
          network::URLLoader* const url_loader,
          const std::function<void()>& success_callback,
          const std::function<void(std::string)> failure_callback,
          const std::function<bool(rapidjson::Document)> set_token_callback) {
  auto encoded_request_body = UrlEncode(request_body);

  mx::vmo data;
  auto result = mtl::VmoFromString(encoded_request_body, &data);
  FTL_DCHECK(result);

  network::URLRequestPtr request(network::URLRequest::New());
  request->url = "https://www.googleapis.com/oauth2/v4/token";
  request->method = "POST";
  request->auto_follow_redirects = true;

  // Content-length header.
  network::HttpHeaderPtr content_length_header = network::HttpHeader::New();
  content_length_header->name = "Content-length";
  uint64_t data_size = encoded_request_body.length();
  content_length_header->value = ftl::NumberToString(data_size);
  request->headers.push_back(std::move(content_length_header));

  // content-type header.
  network::HttpHeaderPtr content_type_header = network::HttpHeader::New();
  content_type_header->name = "content-type";
  content_type_header->value = "application/x-www-form-urlencoded";
  request->headers.push_back(std::move(content_type_header));

  request->body = network::URLBody::New();
  request->body->set_buffer(std::move(data));

  url_loader->Start(std::move(request), [success_callback, failure_callback,
                                         set_token_callback](
                                            network::URLResponsePtr response) {
    if (!response->error.is_null()) {
      failure_callback(
          "Network error! code: " + std::to_string(response->error->code) +
          " description: " + response->error->description.data());
      return;
    }

    if (response->status_code != 200) {
      failure_callback("Status code: " + std::to_string(response->status_code) +
                       " while fetching access token.");
      return;
    }

    if (!response->body.is_null()) {
      FTL_DCHECK(response->body->is_stream());
      std::string response_body;
      // TODO(alhaad/ukode): Use non-blocking variant.
      if (!mtl::BlockingCopyToString(std::move(response->body->get_stream()),
                                     &response_body)) {
        failure_callback("Failed to read from socket.");
        return;
      }

      rapidjson::Document doc;
      doc.Parse(response_body);
      FTL_DCHECK(!doc.HasParseError());
      FTL_DCHECK(set_token_callback(std::move(doc)));
      success_callback();
    }
  });
}

}  // namespace

// Implementation of the OAuth Token Manager app.
class OAuthTokenManagerApp : AccountProvider {
 public:
  OAuthTokenManagerApp();

 private:
  // |AccountProvider|
  void Initialize(
      fidl::InterfaceHandle<AccountProviderContext> provider) override;

  // |AccountProvider|
  void AddAccount(IdentityProvider identity_provider,
                  const fidl::String& display_name,
                  const AddAccountCallback& callback) override;

  // |AccountProvider|
  void GetTokenProviderFactory(
      const fidl::String& account_id,
      fidl::InterfaceRequest<TokenProviderFactory> request) override;

  // Generate a random account id.
  std::string GenerateAccountId();

  void AddRefreshTokenCall(const std::string& account_id,
                           const std::function<void(std::string)>& callback);

  std::shared_ptr<app::ApplicationContext> application_context_;

  AccountProviderContextPtr account_provider_context_;

  fidl::Binding<AccountProvider> binding_;

  class TokenProviderFactoryImpl;
  // account_id -> TokenProviderFactoryImpl
  std::unordered_map<std::string, std::unique_ptr<TokenProviderFactoryImpl>>
      token_provider_factory_impls_;

  rapidjson::Document all_tokens_;

  // We are using operations here not to guard state across asynchronous calls
  // but rather to clean up state after an 'operation' is done.
  OperationCollection operation_collection_;

  class GoogleRefreshCall;
  class GoogleOAuthCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(OAuthTokenManagerApp);
};

class OAuthTokenManagerApp::TokenProviderFactoryImpl : TokenProviderFactory,
                                                       TokenProvider {
 public:
  TokenProviderFactoryImpl(const fidl::String& account_id,
                           OAuthTokenManagerApp* const app,
                           fidl::InterfaceRequest<TokenProviderFactory> request)
      : account_id_(account_id), binding_(this, std::move(request)), app_(app) {
    binding_.set_connection_error_handler(
        [this] { app_->token_provider_factory_impls_.erase(account_id_); });
  }

 private:
  // |TokenProviderFactory|
  void GetTokenProvider(
      const fidl::String& application_url,
      fidl::InterfaceRequest<TokenProvider> request) override {
    // TODO(alhaad/ukode): Current implementation is agnostic about which
    // agent is requesting what token. Fix this.
    token_provider_bindings_.AddBinding(this, std::move(request));
  }

  // TODO(alhaad/ukode): The current implementation always refreshes as access
  // token before returning it. Be smarter about it!
  // |TokenProvider|
  void GetAccessToken(const GetAccessTokenCallback& callback) override {
    if (!app_->all_tokens_.HasMember(account_id_) ||
        !app_->all_tokens_[account_id_].HasMember("refresh_token")) {
      callback(nullptr);
      return;
    }
    app_->AddRefreshTokenCall(account_id_, callback);
  }

  // |TokenProvider|
  // TODO(alhaad/ukode): Id token might have expired, refresh it!
  void GetIdToken(const GetIdTokenCallback& callback) override {
    if (!app_->all_tokens_.HasMember(account_id_) ||
        !app_->all_tokens_[account_id_].HasMember("id_token")) {
      callback(nullptr);
      return;
    }
    callback(app_->all_tokens_[account_id_]["id_token"].GetString());
  }

  // |TokenProvider|
  void GetClientId(const GetClientIdCallback& callback) override {
    callback(kClientId);
  }

  std::string account_id_;
  fidl::Binding<TokenProviderFactory> binding_;
  fidl::BindingSet<TokenProvider> token_provider_bindings_;

  OAuthTokenManagerApp* const app_;

  FTL_DISALLOW_COPY_AND_ASSIGN(TokenProviderFactoryImpl);
};

class OAuthTokenManagerApp::GoogleRefreshCall : Operation<void> {
 public:
  GoogleRefreshCall(OperationContainer* const container,
                    const std::string& account_id,
                    OAuthTokenManagerApp* const app,
                    const std::function<void(fidl::String)>& callback)
      : Operation(container, [] {}),
        account_id_(account_id),
        app_(app),
        callback_(callback) {
    Ready();
  }

 private:
  void Run() override {
    std::string refresh_token =
        app_->all_tokens_[account_id_]["refresh_token"].GetString();
    std::string request_body = "refresh_token=" + refresh_token +
                               "&client_id=" + kClientId +
                               "&grant_type=refresh_token";
    app_->application_context_->ConnectToEnvironmentService(
        network_service_.NewRequest());
    network_service_->CreateURLLoader(url_loader_.NewRequest());

    Post(request_body, url_loader_.get(), [this] { Success(); },
         [this](const std::string error_message) { Failure(error_message); },
         [this](rapidjson::Document doc) { return true; });
  }

  void Success() {
    callback_(app_->all_tokens_[account_id_]["access_token"].GetString());
    Done();
  }

  void Failure(const std::string& error_message) {
    FTL_LOG(ERROR) << error_message;
    callback_(nullptr);
    Done();
  }

  std::string account_id_;
  OAuthTokenManagerApp* const app_;
  const std::function<void(fidl::String)> callback_;

  network::NetworkServicePtr network_service_;
  network::URLLoaderPtr url_loader_;

  FTL_DISALLOW_COPY_AND_ASSIGN(GoogleRefreshCall);
};

// TODO(alhaad): Use variadic template in |Operation|. That way, parameters to
// |callback| can be returned as parameters to |Done()|.
class OAuthTokenManagerApp::GoogleOAuthCall : Operation<void>,
                                              web_view::WebRequestDelegate {
 public:
  GoogleOAuthCall(OperationContainer* const container,
                  AccountPtr account,
                  OAuthTokenManagerApp* const app,
                  const AddAccountCallback& callback)
      : Operation(container, [] {}),
        account_(std::move(account)),
        app_(app),
        callback_(callback) {
    Ready();
  }

 private:
  // |Operation|
  void Run() override {
    auto view_owner = SetupWebView();

    // Set a delegate which will parse incoming URLs for authorization code.
    // TODO(alhaad/ukode): We need to set a timout here in-case we do not get
    // the code.
    web_view::WebRequestDelegatePtr web_request_delegate;
    web_request_delegate_bindings_.AddBinding(
        this, web_request_delegate.NewRequest());
    web_view_->SetWebRequestDelegate(std::move(web_request_delegate));

    std::vector<std::string> scopes(kScopes.begin(), kScopes.end());
    std::string joined_scopes = ftl::JoinStrings(scopes, "+");

    std::string url = kGoogleOAuthEndpoint;
    url += "?scope=" + joined_scopes;
    url += "&response_type=code&redirect_uri=";
    url += kRedirectUri;
    url += "&client_id=";
    url += kClientId;
    web_view_->SetUrl(std::move(url));

    app_->account_provider_context_->GetAuthenticationContext(
        account_->id, auth_context_.NewRequest());
    auth_context_->StartOverlay(std::move(view_owner));
  }

  // |web_view::WebRequestDelegate|
  void WillSendRequest(const fidl::String& incoming_url) override {
    std::string uri = incoming_url.get();
    std::string prefix = kRedirectUri;
    prefix += "?code=";
    auto pos = uri.find(prefix);
    if (pos != 0) {
      return;
    }

    auto code = uri.substr(prefix.size(), std::string::npos);
    // There is a '#' character at the end.
    code.pop_back();
    std::string request_body =
        "code=" + code + "&redirect_uri=" + kRedirectUri +
        "&client_id=" + kClientId + "&grant_type=authorization_code";

    app_->application_context_->ConnectToEnvironmentService(
        network_service_.NewRequest());
    network_service_->CreateURLLoader(url_loader_.NewRequest());

    Post(request_body, url_loader_.get(), [this] { Success(); },
         [this](const std::string error_message) { Failure(error_message); },
         [this](rapidjson::Document doc) { return SetToken(std::move(doc)); });
  }

  bool SetToken(rapidjson::Document tokens) {
    auto& account_id = account_->id;
    auto& allocator = app_->all_tokens_.GetAllocator();

    // Remove any existing tokens for this account_id.
    if (app_->all_tokens_.HasMember(account_id)) {
      app_->all_tokens_.RemoveMember(account_id);
    }

    app_->all_tokens_.AddMember(rapidjson::Value(account_id, allocator), tokens,
                                allocator);

    // Save tokens to disk.
    if (!files::CreateDirectory(files::GetDirectoryName(kTokensFile))) {
      return false;
    }
    auto serialized_tokens = modular::JsonValueToString(app_->all_tokens_);
    if (!files::WriteFile(kTokensFile, serialized_tokens.data(),
                          serialized_tokens.size())) {
      return false;
    }

    return true;
  }
  void Success() {
    callback_(std::move(account_), nullptr);
    auth_context_->StopOverlay();
    Done();
  }

  void Failure(const std::string& error_message) {
    FTL_LOG(ERROR) << error_message;
    callback_(nullptr, error_message);
    auth_context_->StopOverlay();
    Done();
  }

  mozart::ViewOwnerPtr SetupWebView() {
    app::ServiceProviderPtr web_view_services;
    auto web_view_launch_info = app::ApplicationLaunchInfo::New();
    web_view_launch_info->url = kWebViewUrl;
    web_view_launch_info->services = web_view_services.NewRequest();
    app_->application_context_->launcher()->CreateApplication(
        std::move(web_view_launch_info), web_view_controller_.NewRequest());
    web_view_controller_.set_connection_error_handler(
        [this] { Failure("Unable to start webview."); });

    mozart::ViewOwnerPtr view_owner;
    mozart::ViewProviderPtr view_provider;
    ConnectToService(web_view_services.get(), view_provider.NewRequest());
    app::ServiceProviderPtr web_view_moz_services;
    view_provider->CreateView(view_owner.NewRequest(),
                              web_view_moz_services.NewRequest());

    ConnectToService(web_view_moz_services.get(), web_view_.NewRequest());

    return view_owner;
  }

  AccountPtr account_;
  OAuthTokenManagerApp* const app_;
  const AddAccountCallback callback_;

  AuthenticationContextPtr auth_context_;

  web_view::WebViewPtr web_view_;
  app::ApplicationControllerPtr web_view_controller_;

  network::NetworkServicePtr network_service_;
  network::URLLoaderPtr url_loader_;

  fidl::BindingSet<web_view::WebRequestDelegate> web_request_delegate_bindings_;

  FTL_DISALLOW_COPY_AND_ASSIGN(GoogleOAuthCall);
};

OAuthTokenManagerApp::OAuthTokenManagerApp()
    : application_context_(app::ApplicationContext::CreateFromStartupInfo()),
      binding_(this) {
  application_context_->outgoing_services()->AddService<AccountProvider>(
      [this](fidl::InterfaceRequest<AccountProvider> request) {
        binding_.Bind(std::move(request));
      });

  if (files::IsFile(kTokensFile)) {
    std::string serialized_tokens;
    if (!files::ReadFileToString(kTokensFile, &serialized_tokens)) {
      FTL_DCHECK(false) << "Unable to read file " << kTokensFile;
    }
    all_tokens_.Parse(serialized_tokens);
    FTL_DCHECK(!all_tokens_.HasParseError());
  } else {
    // Create an empty DOM.
    all_tokens_.SetObject();
  }
}

void OAuthTokenManagerApp::Initialize(
    fidl::InterfaceHandle<AccountProviderContext> provider) {
  FTL_LOG(INFO) << "OAuthTokenManagerApp::Initialize()";
  account_provider_context_.Bind(std::move(provider));
}

// TODO(alhaad): Check if account id already exists.
std::string OAuthTokenManagerApp::GenerateAccountId() {
  uint32_t random_number;
  size_t random_size;
  mx_status_t status =
      mx_cprng_draw(&random_number, sizeof random_number, &random_size);
  FTL_CHECK(status == NO_ERROR);
  FTL_CHECK(sizeof random_number == random_size);
  return std::to_string(random_number);
}

void OAuthTokenManagerApp::AddAccount(IdentityProvider identity_provider,
                                      const fidl::String& display_name,
                                      const AddAccountCallback& callback) {
  FTL_LOG(INFO) << "OAuthTokenManagerApp::AddAccount()";
  auto account = auth::Account::New();

  account->id = GenerateAccountId();
  account->identity_provider = identity_provider;
  // TODO(alhaad/ukode): Derive |display_name| from user profile instead.
  account->display_name = display_name;

  switch (identity_provider) {
    case IdentityProvider::DEV:
      callback(std::move(account), nullptr);
      return;
    case IdentityProvider::GOOGLE:
      new GoogleOAuthCall(&operation_collection_, std::move(account), this,
                          callback);
      return;
    default:
      callback(nullptr, "Unrecognized Identity Provider");
  }
}

void OAuthTokenManagerApp::GetTokenProviderFactory(
    const fidl::String& account_id,
    fidl::InterfaceRequest<TokenProviderFactory> request) {
  new TokenProviderFactoryImpl(account_id, this, std::move(request));
}

void OAuthTokenManagerApp::AddRefreshTokenCall(
    const std::string& account_id,
    const std::function<void(std::string)>& callback) {
  new GoogleRefreshCall(&operation_collection_, account_id, this, callback);
}

}  // namespace auth
}  // namespace modular

int main(int argc, const char** argv) {
  mtl::MessageLoop loop;
  modular::auth::OAuthTokenManagerApp app;
  loop.Run();
  return 0;
}
