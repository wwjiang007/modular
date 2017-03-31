// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/user_runner/user_runner_impl.h"

#include <string>

#include "application/lib/app/connect.h"
#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/maxwell/services/resolver/resolver.fidl.h"
#include "apps/maxwell/services/suggestion/suggestion_provider.fidl.h"
#include "apps/maxwell/services/user/user_intelligence_provider.fidl.h"
#include "apps/modular/lib/auth/token_provider_impl.h"
#include "apps/modular/lib/fidl/array_to_string.h"
#include "apps/modular/lib/fidl/scope.h"
#include "apps/modular/lib/rapidjson/rapidjson.h"
#include "apps/modular/services/config/config.fidl.h"
#include "apps/modular/services/story/story_provider.fidl.h"
#include "apps/modular/services/user/user_context.fidl.h"
#include "apps/modular/services/user/user_runner.fidl.h"
#include "apps/modular/services/user/user_shell.fidl.h"
#include "apps/modular/src/component/component_context_impl.h"
#include "apps/modular/src/story_runner/link_impl.h"
#include "apps/modular/src/story_runner/story_provider_impl.h"
#include "apps/modular/src/story_runner/story_storage_impl.h"
#include "apps/modular/src/user_runner/focus.h"
#include "apps/mozart/services/views/view_provider.fidl.h"
#include "apps/mozart/services/views/view_token.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "lib/mtl/tasks/message_loop.h"

namespace modular {

namespace {

const char kAppId[] = "modular_user_runner";
const char kMaxwellUrl[] = "file:///system/apps/maxwell";
const char kUserScopeLabelPrefix[] = "user-";

std::string LedgerStatusToString(ledger::Status status) {
  switch (status) {
    case ledger::Status::OK:
      return "OK";
    case ledger::Status::AUTHENTICATION_ERROR:
      return "AUTHENTICATION_ERROR";
    case ledger::Status::PAGE_NOT_FOUND:
      return "PAGE_NOT_FOUND";
    case ledger::Status::KEY_NOT_FOUND:
      return "KEY_NOT_FOUND";
    case ledger::Status::REFERENCE_NOT_FOUND:
      return "REFERENCE_NOT_FOUND";
    case ledger::Status::IO_ERROR:
      return "IO_ERROR";
    case ledger::Status::TRANSACTION_ALREADY_IN_PROGRESS:
      return "TRANSACTION_ALREADY_IN_PROGRESS";
    case ledger::Status::NO_TRANSACTION_IN_PROGRESS:
      return "NO_TRANSACTION_IN_PROGRESS";
    case ledger::Status::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case ledger::Status::UNKNOWN_ERROR:
      return "UNKNOWN_ERROR";
    default:
      return "(unknown error)";
  }
};

}  // namespace

UserRunnerImpl::UserRunnerImpl(
    app::ApplicationEnvironmentPtr application_environment,
    fidl::Array<uint8_t> user_id,
    const fidl::String& device_name,
    AppConfigPtr user_shell,
    AppConfigPtr story_shell,
    const fidl::String& auth_token,
    fidl::InterfaceHandle<ledger::LedgerRepository> ledger_repository,
    fidl::InterfaceHandle<UserContext> user_context,
    fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
    fidl::InterfaceRequest<UserRunner> user_runner_request)
    : binding_(this, std::move(user_runner_request)),
      user_shell_context_binding_(this),
      ledger_repository_(
          ledger::LedgerRepositoryPtr::Create(std::move(ledger_repository))),
      user_scope_(
            std::move(application_environment),
            std::string(kUserScopeLabelPrefix) + to_hex_string(user_id)),
      message_queue_manager_(ledger_repository_.get()),
      token_provider_impl_(auth_token),
      device_name_(device_name),
      focus_handler_(device_name, ledger_repository_.get()) {
  binding_.set_connection_error_handler([this] { delete this; });

  auto resolver_service_provider =
      GetServiceProvider("file:///system/apps/resolver_main");
  user_scope_.AddService<resolver::Resolver>(
      ftl::MakeCopyable([resolver_service_provider =
                         std::move(resolver_service_provider)](
                             fidl::InterfaceRequest<resolver::Resolver>
                             resolver_service_request) {
                          ConnectToService(resolver_service_provider.get(),
                                           std::move(resolver_service_request));
                        }));

  RunUserShell(std::move(user_shell), std::move(view_owner_request));

  ledger::LedgerPtr ledger;
  ledger_repository_->GetLedger(
      to_array(kAppId), ledger.NewRequest(), [](ledger::Status status) {
        FTL_CHECK(status == ledger::Status::OK)
            << "LedgerRepository.GetLedger() failed: "
            << LedgerStatusToString(status);
      });

  // Begin init maxwell.
  //
  // NOTE: There is an awkward service exchange here between
  // UserIntelligenceProvider, AgentRunner and StoryProviderImpl. AgentRunner
  // needs a UserIntelligenceProvider to expose services from Maxwell through
  // its getIntelligenceServices() method. Initializing the Maxwell process
  // (through UserIntelligenceProviderFactory) requires a ComponentContext.
  // ComponentContext requires an AgentRunner, which creates a circular
  // dependency. Because of FIDL late bindings, we can get around this by
  // creating a new InterfaceRequest here (|intelligence_provider_request|),
  // making the InterfacePtr a valid proxy to be passed to AgentRunner and
  // StoryProviderImpl, even though it won't be bound to a real implementation
  // (provided by Maxwell) until later. It works, but it's not a good pattern.
  auto intelligence_provider_request =
      user_intelligence_provider_.NewRequest();
  agent_runner_.reset(new AgentRunner(
      user_scope_.GetLauncher(), &message_queue_manager_,
      ledger_repository_.get(), user_intelligence_provider_.get()));

  maxwell_component_context_impl_.reset(
      new ComponentContextImpl({&message_queue_manager_, agent_runner_.get(),
              ledger_repository_.get()},
        kMaxwellUrl));
  maxwell_component_context_binding_.reset(
      new fidl::Binding<ComponentContext>(
          maxwell_component_context_impl_.get()));
  auto maxwell_services = GetServiceProvider(kMaxwellUrl);
  auto maxwell_factory =
      app::ConnectToService<maxwell::UserIntelligenceProviderFactory>(
          maxwell_services.get());
  fidl::InterfaceHandle<StoryProvider> maxwell_story_provider;
  auto maxwell_story_provider_request = maxwell_story_provider.NewRequest();
  maxwell_factory->GetUserIntelligenceProvider(
      maxwell_component_context_binding_->NewBinding(),
      std::move(maxwell_story_provider), focus_handler_.GetProvider(),
      visible_stories_handler_.GetProvider(),
        std::move(intelligence_provider_request));
  // End init maxwell.

  story_provider_impl_.reset(new StoryProviderImpl(
      &user_scope_, std::move(ledger), device_name, std::move(story_shell),
      {&message_queue_manager_, agent_runner_.get(),
            ledger_repository_.get()},
      user_intelligence_provider_.get()));
  story_provider_impl_->AddBinding(std::move(maxwell_story_provider_request));

  user_scope_.AddService<TokenProvider>(
      [this](fidl::InterfaceRequest<TokenProvider> request) {
        token_provider_impl_.AddBinding(std::move(request));
      });

  user_shell_->Initialize(std::move(user_context),
                          user_shell_context_binding_.NewBinding());
}

UserRunnerImpl::~UserRunnerImpl() = default;

void UserRunnerImpl::Terminate(const TerminateCallback& done) {
  FTL_DCHECK(user_shell_.is_bound());
  FTL_LOG(INFO) << "UserRunner::Terminate()";
      user_shell_->Terminate([this, done] {
          mtl::MessageLoop::GetCurrent()->PostQuitTask();
          done();
          delete this;
          FTL_LOG(INFO) << "UserRunner::Terminate(): deleted";
        });
}

void UserRunnerImpl::GetDeviceName(const GetDeviceNameCallback& callback) {
  callback(device_name_);
}

void UserRunnerImpl::GetStoryProvider(
    fidl::InterfaceRequest<StoryProvider> request) {
  story_provider_impl_->AddBinding(std::move(request));
}

void UserRunnerImpl::GetSuggestionProvider(
    fidl::InterfaceRequest<maxwell::SuggestionProvider> request) {
  user_intelligence_provider_->GetSuggestionProvider(std::move(request));
}

void UserRunnerImpl::GetVisibleStoriesController(
    fidl::InterfaceRequest<VisibleStoriesController> request) {
  visible_stories_handler_.GetController(std::move(request));
}

void UserRunnerImpl::GetFocusController(
    fidl::InterfaceRequest<FocusController> request) {
  focus_handler_.GetController(std::move(request));
}

void UserRunnerImpl::GetFocusProvider(
    fidl::InterfaceRequest<FocusProvider> request) {
  focus_handler_.GetProvider(std::move(request));
}

void UserRunnerImpl::GetLink(fidl::InterfaceRequest<Link> request) {
  if (user_shell_link_) {
    user_shell_link_->Connect(std::move(request));
    return;
  }

  link_storage_.reset(
      new StoryStorageImpl(story_provider_impl_->GetRootPage()));

  user_shell_link_.reset(new LinkImpl(link_storage_.get(), kUserShellKey));
  user_shell_link_->Connect(std::move(request));
}

app::ServiceProviderPtr UserRunnerImpl::GetServiceProvider(AppConfigPtr config) {
  auto launch_info = app::ApplicationLaunchInfo::New();

  app::ServiceProviderPtr services;
  launch_info->services = services.NewRequest();
  launch_info->url = config->url;
  launch_info->arguments = config->args.Clone();

  app::ApplicationControllerPtr ctrl;
  user_scope_.GetLauncher()->CreateApplication(std::move(launch_info),
                                                 ctrl.NewRequest());
  application_controllers_.emplace_back(std::move(ctrl));

  return services;
}

app::ServiceProviderPtr UserRunnerImpl::GetServiceProvider(const std::string& url) {
  AppConfig config;
  config.url = url;
  return GetServiceProvider(config.Clone());
}

void UserRunnerImpl::RunUserShell(
    AppConfigPtr user_shell,
    fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request) {
  auto app_services = GetServiceProvider(std::move(user_shell));

  mozart::ViewProviderPtr view_provider;
  ConnectToService(app_services.get(), view_provider.NewRequest());
  view_provider->CreateView(std::move(view_owner_request), nullptr);

  // Use this service provider to get |UserShell| interface.
  ConnectToService(app_services.get(), user_shell_.NewRequest());
}

}  // namespace modular
