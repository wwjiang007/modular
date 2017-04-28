// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/bootstrap/app.h"

#include <magenta/process.h>
#include <magenta/processargs.h>

#include "application/lib/app/connect.h"
#include "apps/modular/src/bootstrap/params.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"

namespace bootstrap {

App::App(Params* params)
    : application_context_(app::ApplicationContext::CreateFromStartupInfo()),
      env_host_binding_(this) {
  FTL_DCHECK(application_context_);

  // Set up environment for the programs we will run.
  app::ApplicationEnvironmentHostPtr env_host;
  env_host_binding_.Bind(env_host.NewRequest());
  application_context_->environment()->CreateNestedEnvironment(
      std::move(env_host), env_.NewRequest(), env_controller_.NewRequest(),
      params->label());
  env_->GetApplicationLauncher(env_launcher_.NewRequest());

  // Register services.
  for (auto& pair : params->TakeServices())
    RegisterSingleton(pair.first, std::move(pair.second));

  // Ordering note: The impl of CreateNestedEnvironment will resolve the
  // delegating app loader. However, since its call back to the env host won't
  // happen until the next (first) message loop iteration, we'll be set up by
  // then.
  RegisterAppLoaders(params->TakeAppLoaders());

  mx_handle_t request = mx_get_startup_handle(PA_SERVICE_REQUEST);
  if (request != MX_HANDLE_INVALID)
    env_services_.ServeDirectory(mx::channel(request));

  // Launch startup applications.
  for (auto& launch_info : params->TakeApps())
    LaunchApplication(std::move(launch_info));
}

App::~App() {}

void App::RegisterSingleton(std::string service_name,
                            app::ApplicationLaunchInfoPtr launch_info) {
  env_services_.AddServiceForName(
      ftl::MakeCopyable([
        this, service_name, launch_info = std::move(launch_info),
        controller = app::ApplicationControllerPtr()
      ](mx::channel client_handle) mutable {
        FTL_VLOG(2) << "Servicing singleton service request for "
                    << service_name;
        auto it = service_providers_.find(launch_info->url);
        if (it == service_providers_.end()) {
          FTL_VLOG(1) << "Starting singleton " << launch_info->url
                      << " for service " << service_name;
          app::ServiceProviderPtr service_provider;
          auto dup_launch_info = app::ApplicationLaunchInfo::New();
          dup_launch_info->url = launch_info->url;
          dup_launch_info->arguments = launch_info->arguments.Clone();
          dup_launch_info->services = service_provider.NewRequest();
          env_launcher_->CreateApplication(std::move(dup_launch_info),
                                           controller.NewRequest());
          service_provider.set_connection_error_handler(
              [ this, url = launch_info->url, &controller ] {
                FTL_LOG(ERROR) << "Singleton " << url << " died";
                controller.reset();  // kills the singleton application
                service_providers_.erase(url);
              });

          std::tie(it, std::ignore) = service_providers_.emplace(
              launch_info->url, std::move(service_provider));
        }

        it->second->ConnectToService(service_name, std::move(client_handle));
      }),
      service_name);
}

void App::RegisterAppLoaders(Params::ServiceMap app_loaders) {
  app_loader_ = std::make_unique<DelegatingApplicationLoader>(
      std::move(app_loaders), env_launcher_.get(),
      application_context_
          ->ConnectToEnvironmentService<app::ApplicationLoader>());

  env_services_.AddService<app::ApplicationLoader>(
      [this](fidl::InterfaceRequest<app::ApplicationLoader> request) {
        app_loader_bindings_.AddBinding(app_loader_.get(), std::move(request));
      });
}

void App::LaunchApplication(app::ApplicationLaunchInfoPtr launch_info) {
  FTL_LOG(INFO) << "Bootstrapping application " << launch_info->url;
  env_launcher_->CreateApplication(std::move(launch_info), nullptr);
}

void App::GetApplicationEnvironmentServices(
    fidl::InterfaceRequest<app::ServiceProvider> environment_services) {
  env_services_.AddBinding(std::move(environment_services));
}

}  // namespace bootstrap
