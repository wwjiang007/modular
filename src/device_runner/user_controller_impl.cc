// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/device_runner/user_controller_impl.h"

#include <memory>
#include <utility>

#include "application/lib/app/connect.h"
#include "apps/modular/lib/fidl/array_to_string.h"

namespace modular {

namespace {

constexpr char kUserRunnerUri[] = "file:///system/apps/user_runner";

}  // namespace

UserControllerImpl::UserControllerImpl(
    std::shared_ptr<app::ApplicationContext> app_context,
    const std::string& device_name,
    const AppConfig& user_shell,
    const AppConfig& story_shell,
    fidl::InterfaceHandle<auth::TokenProviderFactory> token_provider_factory,
    const std::string& user_id,
    fidl::InterfaceHandle<ledger::LedgerRepository> ledger_repository,
    fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
    fidl::InterfaceRequest<UserController> user_controller_request,
    DoneCallback done)
    : user_context_impl_(this),
      user_context_binding_(&user_context_impl_),
      user_controller_binding_(this, std::move(user_controller_request)),
      done_(done) {
  // 1. Launch UserRunner in the current environment.
  auto launch_info = app::ApplicationLaunchInfo::New();
  launch_info->url = kUserRunnerUri;
  app::ServiceProviderPtr services;
  launch_info->services = services.NewRequest();
  app_context->launcher()->CreateApplication(
      std::move(launch_info), user_runner_controller_.NewRequest());

  // 2. Initialize the UserRunner service.
  UserRunnerFactoryPtr user_runner_factory;
  app::ConnectToService(services.get(), user_runner_factory.NewRequest());
  user_runner_factory->Create(
      user_id, device_name, user_shell.Clone(), story_shell.Clone(),
      std::move(token_provider_factory), std::move(ledger_repository),
      user_context_binding_.NewBinding(), std::move(view_owner_request),
      user_runner_.NewRequest());
}

// |UserController|
void UserControllerImpl::Logout(const LogoutCallback& done) {
  FTL_LOG(INFO) << "UserController::Logout()";
  logout_response_callbacks_.push_back(done);
  if (logout_response_callbacks_.size() > 1) {
    return;
  }

  // This should prevent us from receiving any further requests.
  user_controller_binding_.Unbind();
  user_context_binding_.Unbind();

  user_runner_->Terminate([this] {
    for (const auto& done : logout_response_callbacks_) {
      done();
    }
    // We announce |OnLogout| only at point just before deleting ourselves,
    // so we can avoid any race conditions that may be triggered by |Shutdown|
    // (which in-turn will call this |Logout| since we have not completed yet).
    user_watchers_.ForAllPtrs(
        [](UserWatcher* watcher) { watcher->OnLogout(); });
    done_(this);
  });
}

// |UserController|
void UserControllerImpl::Watch(fidl::InterfaceHandle<UserWatcher> watcher) {
  user_watchers_.AddInterfacePtr(UserWatcherPtr::Create(std::move(watcher)));
}

// |UserContext|
void UserContextImpl::Logout() {
  FTL_LOG(INFO) << "UserContext::Logout()";
  controller_->Logout([] {});
}

}  // namespace modular
