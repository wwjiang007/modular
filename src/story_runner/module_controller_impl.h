// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_STORY_RUNNER_MODULE_CONTROLLER_IMPL_H_
#define APPS_MODULAR_SRC_STORY_RUNNER_MODULE_CONTROLLER_IMPL_H_

#include <vector>

#include "application/services/application_launcher.fidl.h"
#include "apps/modular/services/module/module.fidl.h"
#include "apps/modular/services/module/module_controller.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/interface_handle.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/fidl/cpp/bindings/interface_ptr_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/ftl/macros.h"

namespace modular {

class StoryImpl;

// Implements the ModuleController interface, which is given to the
// client that called Story.StartModule(). Exactly one
// ModuleControllerImpl instance is associated with each
// ModuleContextImpl instance.
class ModuleControllerImpl : ModuleController {
 public:
  ModuleControllerImpl(
      StoryImpl* const story_impl,
      const std::string& url,
      app::ApplicationControllerPtr module_application,
      ModulePtr module,
      fidl::InterfaceRequest<ModuleController> module_controller);

  ~ModuleControllerImpl() override;

  // Notifies all watchers of a state change of the module. Also
  // remembers the state to initialize future added watchers.
  void SetState(const ModuleState new_state);

  // Calls Stop() on the module, closes the module handle, notifies
  // watchers, then DisposeModule()s the connection and finally calls
  // done(). Thus, done must not reference anything in
  // ModuleController or the related ModuleContextImpl.
  void TearDown(std::function<void()> done);

 private:
  // |ModuleController|
  void Watch(fidl::InterfaceHandle<ModuleWatcher> watcher) override;
  void Stop(const StopCallback& done) override;

  // Used as connection error callback on the ModuleController
  // binding.
  void OnConnectionError();

  StoryImpl* const story_impl_;
  const std::string url_;
  app::ApplicationControllerPtr module_application_;
  ModulePtr module_;
  fidl::Binding<ModuleController> binding_;
  fidl::InterfacePtrSet<ModuleWatcher> watchers_;
  ModuleState state_{ModuleState::STARTING};

  // Callbacks of TearDown() invocations. If there is one Stop()
  // request pending, a second one is only queued, no second call to
  // Stop() is made.
  std::vector<std::function<void()>> teardown_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ModuleControllerImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_STORY_RUNNER_MODULE_CONTROLLER_IMPL_H_
