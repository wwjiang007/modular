// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "apps/modular/lib/app/application_context.h"
#include "apps/modular/lib/app/connect.h"
#include "apps/modular/mojo/strong_binding.h"
#include "apps/modular/services/application/application_launcher.fidl.h"
#include "apps/modular/services/application/service_provider.fidl.h"
#include "apps/modular/services/device/device_shell.fidl.h"
#include "apps/modular/services/user/user_runner.fidl.h"
#include "apps/mozart/services/launcher/launcher.fidl.h"
#include "apps/mozart/services/views/view_provider.fidl.h"
#include "apps/mozart/services/views/view_token.fidl.h"
#include "lib/fidl/cpp/bindings/array.h"
#include "lib/fidl/cpp/bindings/interface_handle.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/fidl/cpp/bindings/string.h"
#include "lib/fidl/cpp/bindings/struct_ptr.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "lib/mtl/tasks/message_loop.h"

namespace modular {

using fidl::Array;
using fidl::GetProxy;
using fidl::InterfaceHandle;
using fidl::InterfacePtr;
using fidl::InterfaceRequest;
using fidl::String;
using fidl::StructPtr;

Array<uint8_t> UserIdentityArray(const std::string& username) {
  Array<uint8_t> array = Array<uint8_t>::New(username.length());
  for (size_t i = 0; i < username.length(); i++) {
    array[i] = static_cast<uint8_t>(username[i]);
  }
  return array;
}

class DeviceRunnerImpl : public DeviceRunner {
 public:
  DeviceRunnerImpl(InterfacePtr<ApplicationLauncher> launcher,
                   InterfaceRequest<DeviceRunner> service)
      : launcher_(std::move(launcher)), binding_(this, std::move(service)) {}

  ~DeviceRunnerImpl() override = default;

 private:
  void Login(const String& username,
             InterfaceRequest<mozart::ViewOwner> view_owner_request) override {
    FTL_LOG(INFO) << "DeviceRunnerImpl::Login() " << username;
    auto launch_info = ApplicationLaunchInfo::New();
    launch_info->url = "file:///system/apps/user_runner";
    ServiceProviderPtr services;
    launch_info->services = fidl::GetProxy(&services);
    launcher_->CreateApplication(std::move(launch_info),
                                 GetProxy(&user_runner_controller_));

    ConnectToService(services.get(), fidl::GetProxy(&user_runner_));

    StructPtr<ledger::Identity> identity = ledger::Identity::New();
    identity->user_id = UserIdentityArray(username);
    // |app_id| must not be null so it will pass mojo validation and must not
    // be empty or we'll get ledger::Status::AUTHENTICATION_ERROR when
    // UserRunner calls GetLedger().
    // TODO(jimbe): When there's support from the Ledger, open the user here,
    // then pass down a handle that is restricted from opening other users.
    identity->app_id = Array<uint8_t>::New(1);

    user_runner_->Launch(
        std::move(identity), std::move(view_owner_request), [](bool success) {
          FTL_LOG(INFO) << "DeviceRunnerImpl::Login() UserRunner.Launch()";
        });
  }

  InterfacePtr<ApplicationLauncher> launcher_;
  StrongBinding<DeviceRunner> binding_;

  ApplicationControllerPtr user_runner_controller_;
  // Interface pointer to the |UserRunner| handle exposed by the User Runner.
  // Currently, we maintain a single instance which means that subsequent
  // logins override previous ones.
  InterfacePtr<UserRunner> user_runner_;

  FTL_DISALLOW_COPY_AND_ASSIGN(DeviceRunnerImpl);
};

class DeviceRunnerApp {
 public:
  DeviceRunnerApp() : context_(ApplicationContext::CreateFromStartupInfo()) {
    FTL_LOG(INFO) << "DeviceRunnerApp::DeviceRunnerApp()";

    auto launch_info = ApplicationLaunchInfo::New();
    launch_info->url = "file:///system/apps/dummy_device_shell";
    ServiceProviderPtr services;
    launch_info->services = fidl::GetProxy(&services);
    context_->launcher()->CreateApplication(
        std::move(launch_info), GetProxy(&device_shell_controller_));

    InterfacePtr<mozart::ViewProvider> view_provider;
    ConnectToService(services.get(), fidl::GetProxy(&view_provider));

    InterfaceHandle<mozart::ViewOwner> root_view;
    InterfacePtr<ServiceProvider> device_shell_services;
    view_provider->CreateView(GetProxy(&root_view),
                              GetProxy(&device_shell_services));

    context_->ConnectToEnvironmentService<mozart::Launcher>()->Display(
        std::move(root_view));

    ConnectToService(device_shell_services.get(), GetProxy(&device_shell_));

    InterfacePtr<ApplicationLauncher> launcher;
    context_->environment()->GetApplicationLauncher(GetProxy(&launcher));
    InterfaceHandle<DeviceRunner> device_runner_handle;
    new DeviceRunnerImpl(std::move(launcher), GetProxy(&device_runner_handle));
    device_shell_->SetDeviceRunner(std::move(device_runner_handle));
  }

 private:
  std::unique_ptr<ApplicationContext> context_;
  ApplicationControllerPtr device_shell_controller_;
  InterfacePtr<DeviceShell> device_shell_;
  FTL_DISALLOW_COPY_AND_ASSIGN(DeviceRunnerApp);
};

}  // namespace modular

int main(int argc, const char** argv) {
  FTL_LOG(INFO) << "device runner main";
  mtl::MessageLoop loop;
  modular::DeviceRunnerApp app;
  loop.Run();
  return 0;
}