// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the story manager mojo app.

#include <mojo/system/main.h>

#include "apps/modular/story_manager/story_manager.mojom.h"
#include "apps/modular/story_manager/story_provider_state.h"
#include "apps/mozart/services/views/interfaces/view_provider.mojom.h"
#include "apps/mozart/services/views/interfaces/view_token.mojom.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "mojo/public/cpp/application/application_impl_base.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/run_application.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/interfaces/application/application_connector.mojom.h"

namespace modular {

using mojo::ApplicationConnector;
using mojo::ApplicationImplBase;
using mojo::Array;
using mojo::Binding;
using mojo::ConnectionContext;
using mojo::CreateApplicationConnector;
using mojo::DuplicateApplicationConnector;
using mojo::GetProxy;
using mojo::InterfaceHandle;
using mojo::InterfacePtr;
using mojo::InterfacePtrSet;
using mojo::InterfaceRequest;
using mojo::ServiceProvider;
using mojo::ServiceProviderImpl;
using mojo::Shell;
using mojo::StrongBinding;
using mojo::StructPtr;
using mojo::String;

class StoryManagerImpl : public StoryManager {
 public:
  StoryManagerImpl(InterfaceHandle<ApplicationConnector> app_connector,
                   InterfaceRequest<StoryManager> request)
      : app_connector_(InterfacePtr<ApplicationConnector>::Create(
            std::move(app_connector))),
        binding_(this, std::move(request)) {}
  ~StoryManagerImpl() override {}

 private:
  void Launch(StructPtr<ledger::Identity> identity,
              InterfaceRequest<mozart::ViewOwner> view_owner_request,
              const LaunchCallback& callback) override {
    FTL_LOG(INFO) << "StoryManagerImpl::Launch()";

    // Establish connection with Ledger.
    ConnectToService(app_connector_.get(), "mojo:ledger",
                     GetProxy(&ledger_factory_));
    ledger_factory_->GetLedger(
        std::move(identity), ftl::MakeCopyable([
          this, callback, request = std::move(view_owner_request)
        ](ledger::Status status,
                             InterfaceHandle<ledger::Ledger> ledger) mutable {
          if (status != ledger::Status::OK) {
            FTL_LOG(ERROR) << "story-manager's connection to ledger failed.";
            callback.Run(false);
            return;
          }
          callback.Run(true);
          StartUserShell(std::move(ledger), std::move(request));
        }));
  }

  // Run the User shell and provide it the |StoryProvider| interface.
  void StartUserShell(InterfaceHandle<ledger::Ledger> ledger,
                      InterfaceRequest<mozart::ViewOwner> view_owner_request) {
    // First use ViewProvider service to plumb |view_owner_request| and get the
    // associated service provider.
    InterfacePtr<mozart::ViewProvider> view_provider;
    InterfacePtr<ServiceProvider> service_provider;
    ConnectToService(app_connector_.get(), "mojo:dummy_user_shell",
                     GetProxy(&view_provider));
    view_provider->CreateView(std::move(view_owner_request),
                              GetProxy(&service_provider));
    user_shell_ptrs_.AddInterfacePtr(std::move(view_provider));

    // Use this service provider to get |UserShell| interface.
    service_provider->ConnectToService(
        UserShell::Name_, GetProxy(&user_shell_).PassMessagePipe());
    InterfaceHandle<StoryProvider> service;
    new StoryProviderState(
        DuplicateApplicationConnector(app_connector_.get()),
        InterfacePtr<ledger::Ledger>::Create(std::move(ledger)), &service);
    user_shell_->SetStoryProvider(std::move(service));
  }

  InterfacePtr<ApplicationConnector> app_connector_;
  StrongBinding<StoryManager> binding_;
  InterfacePtrSet<mozart::ViewProvider> user_shell_ptrs_;

  InterfacePtr<UserShell> user_shell_;

  InterfacePtr<ledger::LedgerFactory> ledger_factory_;

  FTL_DISALLOW_COPY_AND_ASSIGN(StoryManagerImpl);
};

class StoryManagerApp : public ApplicationImplBase {
 public:
  StoryManagerApp() {}
  ~StoryManagerApp() override {}

 private:
  void OnInitialize() override { FTL_LOG(INFO) << "story-manager init"; }

  bool OnAcceptConnection(ServiceProviderImpl* service_provider_impl) override {
    // Register |StoryManager| implementation.
    service_provider_impl->AddService<StoryManager>(
        [this](const ConnectionContext& connection_context,
               InterfaceRequest<StoryManager> launcher_request) {
          new StoryManagerImpl(CreateApplicationConnector(shell()),
                               std::move(launcher_request));
        });
    return true;
  }

  FTL_DISALLOW_COPY_AND_ASSIGN(StoryManagerApp);
};

}  // namespace modular

MojoResult MojoMain(MojoHandle application_request) {
  modular::StoryManagerApp app;
  return mojo::RunApplication(application_request, &app);
}
