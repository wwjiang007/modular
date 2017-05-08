// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_DEVICE_RUNNER_USER_PROVIDER_IMPL_H_
#define APPS_MODULAR_SRC_DEVICE_RUNNER_USER_PROVIDER_IMPL_H_

#include "application/lib/app/application_context.h"
#include "apps/ledger/services/internal/internal.fidl.h"
#include "apps/modular/services/auth/account_provider.fidl.h"
#include "apps/modular/services/config/config.fidl.h"
#include "apps/modular/services/device/user_provider.fidl.h"
#include "apps/modular/src/device_runner/user_controller_impl.h"
#include "apps/mozart/services/views/view_token.fidl.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"

namespace modular {

struct UsersStorage;

class UserProviderImpl : UserProvider {
 public:
  UserProviderImpl(std::shared_ptr<app::ApplicationContext> app_context,
                   const AppConfig& user_shell,
                   const AppConfig& story_shell,
                   ledger::LedgerRepositoryFactoryPtr ledger_repository_factory,
                   bool ledger_repository_for_testing,
                   auth::AccountProviderPtr account_provider);

  void Connect(fidl::InterfaceRequest<UserProvider> request);

  void Teardown(const std::function<void()>& callback);

 private:
  // |UserProvider|
  void Login(
      const fidl::String& account_id,
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
      fidl::InterfaceRequest<UserController> user_controller_request) override;

  // |UserProvider|
  void PreviousUsers(const PreviousUsersCallback& callback) override;

  // |UserProvider|
  void AddUser(auth::IdentityProvider identity_provider,
               const fidl::String& displayname,
               const fidl::String& devicename,
               const fidl::String& servername,
               const AddUserCallback& callback) override;

  bool Parse(const std::string& serialized_users);

  void LoginInternal(
      const std::string& user_id,
      const std::string& device_name,
      const fidl::String& server_name,
      const std::string& local_ledger_path,
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
      fidl::InterfaceRequest<UserController> user_controller_request);

  fidl::BindingSet<UserProvider> bindings_;

  std::shared_ptr<app::ApplicationContext> app_context_;
  const AppConfig& user_shell_;   // Neither owned nor copied.
  const AppConfig& story_shell_;  // Neither owned nor copied.
  ledger::LedgerRepositoryFactoryPtr ledger_repository_factory_;
  const bool ledger_repository_for_testing_;
  auth::AccountProviderPtr account_provider_;

  std::string serialized_users_;
  const modular::UsersStorage* users_storage_ = nullptr;

  std::unordered_map<UserControllerImpl*, std::unique_ptr<UserControllerImpl>>
      user_controllers_;

  FTL_DISALLOW_COPY_AND_ASSIGN(UserProviderImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_DEVICE_RUNNER_USER_PROVIDER_IMPL_H_
