// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/device_runner/user_provider_impl.h"

#include "apps/modular/src/device_runner/users_generated.h"
#include "lib/ftl/files/directory.h"
#include "lib/ftl/files/file.h"
#include "lib/ftl/files/path.h"
#include "lib/ftl/strings/string_printf.h"

namespace modular {

namespace {

constexpr char kLedgerDataBaseDir[] = "/data/ledger/";
constexpr char kUsersConfigurationFile[] = "/data/modular/device/users-v1.db";

}  // namespace

UserProviderImpl::UserProviderImpl(
    std::shared_ptr<app::ApplicationContext> app_context,
    const AppConfig& user_shell,
    const AppConfig& story_shell,
    ledger::LedgerRepositoryFactoryPtr ledger_repository_factory,
    bool ledger_repository_for_testing,
    auth::AccountProviderPtr account_provider)
    : app_context_(app_context),
      user_shell_(user_shell),
      story_shell_(story_shell),
      ledger_repository_factory_(std::move(ledger_repository_factory)),
      ledger_repository_for_testing_(ledger_repository_for_testing),
      account_provider_(std::move(account_provider)) {
  // There might not be a file of users persisted. If config file doesn't
  // exist, move forward with no previous users.
  // TODO(alhaad): Use JSON instead of flatbuffers for better inspectablity.
  if (files::IsFile(kUsersConfigurationFile)) {
    std::string serialized_users;
    if (!files::ReadFileToString(kUsersConfigurationFile, &serialized_users)) {
      // Unable to read file. Bailing out.
      FTL_LOG(ERROR) << "Unable to read user configuration file at: "
                     << kUsersConfigurationFile;
      return;
    }

    if (!Parse(serialized_users)) {
      return;
    }
  }
}

void UserProviderImpl::Connect(fidl::InterfaceRequest<UserProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

void UserProviderImpl::Teardown(const std::function<void()>& callback) {
  user_controller_impl_->Logout(callback);
}

void UserProviderImpl::Login(
    const fidl::String& account_id,
    fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
    fidl::InterfaceRequest<UserController> user_controller_request) {
  // If requested, run in incognito mode.
  // TODO(alhaad): Revisit clean-up of local ledger state for incognito mode.
  if (account_id.is_null() || account_id == "") {
    FTL_LOG(INFO) << "UserProvider::Login() Incognito mode";
    // When running in incogito mode, we generate a random number. This number
    // serves as user_id, device_name and the filename for ledger repository.
    uint32_t random_number;
    size_t random_size;
    mx_status_t status =
        mx_cprng_draw(&random_number, sizeof random_number, &random_size);
    FTL_CHECK(status == NO_ERROR);
    FTL_CHECK(sizeof random_number == random_size);

    auto user_id = std::to_string(random_number);
    auto ledger_repository_path = kLedgerDataBaseDir + user_id;
    LoginInternal(user_id, user_id, nullptr /* server_name */,
                  ledger_repository_path, std::move(view_owner_request),
                  std::move(user_controller_request));
    return;
  }

  // If not running in incognito mode, a corresponding entry must be present
  // in the users database.
  const UserStorage* found_user = nullptr;
  if (users_storage_) {
    for (const auto* user : *users_storage_->users()) {
      if (user->id()->str() == account_id) {
        found_user = user;
        break;
      }
    }
  }

  // If an entry is not found, we drop the incoming requests on the floor.
  if (!found_user) {
    FTL_LOG(INFO) << "The requested user was not found in the users database"
                  << "It needs to be added first via UserProvider::AddUser().";
    return;
  }

  // Get the LedgerRepository for the user.
  // |user_id| has to be something that is the same across devices. Currently,
  // we take it as input from the user. TODO(alhaad): Infer it from id token.
  std::string user_id = found_user->display_name()->str();
  std::string ledger_repository_path = kLedgerDataBaseDir + user_id;

  if (ledger_repository_for_testing_) {
    unsigned random_number;
    size_t random_size;
    mx_status_t status =
        mx_cprng_draw(&random_number, sizeof random_number, &random_size);
    FTL_CHECK(status == NO_ERROR);
    FTL_CHECK(sizeof random_number == random_size);

    ledger_repository_path +=
        ftl::StringPrintf("_for_testing_%X", random_number);
    FTL_LOG(INFO) << "Using testing ledger repository path: "
                  << ledger_repository_path;
  }

  FTL_LOG(INFO) << "UserProvider::Login() user: " << user_id;
  LoginInternal(user_id, found_user->device_name()->str(),
                found_user->server_name()->str(), ledger_repository_path,
                std::move(view_owner_request),
                std::move(user_controller_request));
}

void UserProviderImpl::PreviousUsers(const PreviousUsersCallback& callback) {
  fidl::Array<auth::AccountPtr> accounts =
      fidl::Array<auth::AccountPtr>::New(0);
  if (users_storage_) {
    for (const auto* user : *users_storage_->users()) {
      auto account = auth::Account::New();
      account->id = user->id()->str();
      switch (user->identity_provider()) {
        case IdentityProvider_DEV:
          account->identity_provider = auth::IdentityProvider::DEV;
          break;
        case IdentityProvider_GOOGLE:
          account->identity_provider = auth::IdentityProvider::GOOGLE;
          break;
        default:
          FTL_DCHECK(false)
              << "Unrecognized IdentityProvider" << user->identity_provider();
      }
      account->display_name = user->display_name()->str();
      accounts.push_back(std::move(account));
    }
  }
  callback(std::move(accounts));
}

void UserProviderImpl::AddUser(auth::IdentityProvider identity_provider,
                               const fidl::String& displayname,
                               const fidl::String& devicename,
                               const fidl::String& servername,
                               const AddUserCallback& callback) {
  account_provider_->AddAccount(
      identity_provider, displayname,
      [this, identity_provider, displayname, devicename, servername, callback](
          auth::AccountPtr account, const fidl::String& error_code) {
        if (account.is_null()) {
          callback(nullptr, error_code);
          return;
        }

        flatbuffers::FlatBufferBuilder builder;
        std::vector<flatbuffers::Offset<modular::UserStorage>> users;

        // Reserialize existing users.
        if (users_storage_) {
          for (const auto* user : *(users_storage_->users())) {
            users.push_back(modular::CreateUserStorage(
                builder, builder.CreateString(user->id()),
                user->identity_provider(),
                builder.CreateString(user->device_name()),
                builder.CreateString(user->server_name())));
          }
        }

        modular::IdentityProvider flatbuffer_identity_provider;
        switch (account->identity_provider) {
          case auth::IdentityProvider::DEV:
            flatbuffer_identity_provider =
                modular::IdentityProvider::IdentityProvider_DEV;
            break;
          case auth::IdentityProvider::GOOGLE:
            flatbuffer_identity_provider =
                modular::IdentityProvider::IdentityProvider_GOOGLE;
            break;
          default:
            FTL_DCHECK(false) << "Unrecongized IDP.";
        }
        users.push_back(modular::CreateUserStorage(
            builder, builder.CreateString(account->id),
            flatbuffer_identity_provider,
            builder.CreateString(account->display_name),
            builder.CreateString(std::move(devicename)),
            builder.CreateString(std::move(servername))));

        builder.Finish(modular::CreateUsersStorage(
            builder, builder.CreateVector(std::move(users))));
        std::string new_serialized_users = std::string(
            reinterpret_cast<const char*>(builder.GetCurrentBufferPointer()),
            builder.GetSize());
        if (!Parse(new_serialized_users)) {
          callback(nullptr, "The user database seems corrupted.");
          return;
        }

        // Save users to disk.
        if (!files::CreateDirectory(
                files::GetDirectoryName(kUsersConfigurationFile))) {
          callback(nullptr, "Unable to create directory.");
          return;
        }
        if (!files::WriteFile(kUsersConfigurationFile,
                              new_serialized_users.data(),
                              new_serialized_users.size())) {
          callback(nullptr, "Unable to write file.");
          return;
        }

        callback(std::move(account), error_code);
      });
}

bool UserProviderImpl::Parse(const std::string& serialized_users) {
  flatbuffers::Verifier verifier(
      reinterpret_cast<const unsigned char*>(serialized_users.data()),
      serialized_users.size());
  if (!modular::VerifyUsersStorageBuffer(verifier)) {
    FTL_LOG(ERROR) << "Unable to verify storage buffer.";
    return false;
  }
  serialized_users_ = std::move(serialized_users);
  users_storage_ = modular::GetUsersStorage(serialized_users_.data());
  return true;
}

void UserProviderImpl::LoginInternal(
    const std::string& user_id,
    const std::string& device_name,
    const fidl::String& server_name,
    const std::string& local_ledger_path,
    fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
    fidl::InterfaceRequest<UserController> user_controller_request) {
  fidl::InterfaceHandle<ledger::LedgerRepository> ledger_repository;
  ledger_repository_factory_->GetRepository(
      local_ledger_path, server_name, ledger_repository.NewRequest(),
      [](ledger::Status status) {
        FTL_DCHECK(status == ledger::Status::OK)
            << "GetRepository failed: " << status;
      });

  // Get token provider factory for this user.
  auth::TokenProviderFactoryPtr token_provider_factory;
  account_provider_->GetTokenProviderFactory(
      user_id, token_provider_factory.NewRequest());

  user_controller_impl_.reset(new UserControllerImpl(
      app_context_, device_name, user_shell_, story_shell_,
      std::move(token_provider_factory), user_id, std::move(ledger_repository),
      std::move(view_owner_request), std::move(user_controller_request),
      [this] { user_controller_impl_.reset(); }));
}

}  // namespace modular
