// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The Story service is the context in which a story executes. It
// starts modules and provides them with a handle to itself, so they
// can start more modules. It also serves as the factory for Link
// instances, which are used to share data between modules.

#ifndef APPS_MODULAR_SRC_STORY_RUNNER_STORY_IMPL_H_
#define APPS_MODULAR_SRC_STORY_RUNNER_STORY_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "application/services/application_controller.fidl.h"
#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/lib/fidl/scope.h"
#include "apps/modular/services/component/component_context.fidl.h"
#include "apps/modular/services/module/module_controller.fidl.h"
#include "apps/modular/services/module/module_data.fidl.h"
#include "apps/modular/services/module/module.fidl.h"
#include "apps/modular/services/story/per_device_story_info.fidl.h"
#include "apps/modular/services/story/story_controller.fidl.h"
#include "apps/modular/services/story/story_data.fidl.h"
#include "apps/modular/services/story/story_shell.fidl.h"
#include "apps/mozart/services/views/view_token.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_handle.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/fidl/cpp/bindings/interface_ptr_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/fidl/cpp/bindings/struct_ptr.h"
#include "lib/ftl/macros.h"

namespace modular {

class LinkImpl;
class ModuleControllerImpl;
class ModuleContextImpl;
class StoryProviderImpl;
class StoryStorageImpl;

constexpr char kRootLink[] = "root";
constexpr char kRootModuleName[] = "root";

// The story runner, which holds all the links and runs all the modules as well
// as the story shell. It also implements the StoryController service to give
// clients control over the story.
class StoryImpl : StoryController, StoryContext, ModuleWatcher {
 public:
  StoryImpl(const fidl::String& story_id,
            ledger::PagePtr story_page,
            StoryProviderImpl* story_provider_impl);
  ~StoryImpl() override;

  // Called by ModuleContextImpl.
  void GetLinkPath(const LinkPathPtr& link_path,
                   fidl::InterfaceRequest<Link> request);

  // Called by ModuleContextImpl and StartModuleInShell().
  //
  // Returns the module instance id so StartModuleInShell() can pass it to the
  // StoryShell.
  void StartModule(
      const fidl::Array<fidl::String>& parent_module_path,
      const fidl::String& module_name,
      const fidl::String& query,
      const fidl::String& link_name,
      fidl::InterfaceHandle<app::ServiceProvider> outgoing_services,
      fidl::InterfaceRequest<app::ServiceProvider> incoming_services,
      fidl::InterfaceRequest<ModuleController> module_controller,
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner,
      std::function<void(uint32_t)> done);

  // Called by ModuleContextImpl.
  void StartModuleInShell(
      const fidl::Array<fidl::String>& parent_module_path,
      const fidl::String& module_name,
      const fidl::String& query,
      const fidl::String& link_name,
      fidl::InterfaceHandle<app::ServiceProvider> outgoing_services,
      fidl::InterfaceRequest<app::ServiceProvider> incoming_services,
      fidl::InterfaceRequest<ModuleController> module_controller,
      const uint64_t parent_id,
      const fidl::String& view_type);

  // Called by ModuleContextImpl.
  const fidl::String& GetStoryId() const;

  // Called by StoryProviderImpl.
  StoryState GetStoryState() const;

  // Called by ModuleControllerImpl.
  //
  // Releases ownership of |controller|, which deletes itself after return.
  void ReleaseModule(ModuleControllerImpl* controller);

  // Called by StoryProviderImpl.
  void Connect(fidl::InterfaceRequest<StoryController> request);

  // Called by StoryProviderImpl.
  bool IsRunning();

  // Called by StoryProviderImpl.
  //
  // A variant of Stop() that stops the story because the story is being
  // deleted. The StoryImpl instance is deleted by StoryProviderImpl and the
  // story data are deleted from the ledger once the done callback is invoked.
  //
  // No further operations invoked after this one are executed. (The Operation
  // accomplishes this by not calling Done() and instead invoking its callback
  // directly from Run(), such that the OperationQueue stays blocked on it until
  // it gets deleted.)
  void StopForDelete(const std::function<void()>& done);

  // Called by StoryProviderImpl.
  void AddForCreate(const fidl::String& module_name,
                    const fidl::String& module_url,
                    const fidl::String& link_name,
                    const fidl::String& link_json,
                    const std::function<void()>& done);

 private:
  // |StoryController|
  void GetInfo(const GetInfoCallback& callback) override;
  void SetInfoExtra(const fidl::String& name,
                    const fidl::String& value,
                    const SetInfoExtraCallback& callback) override;
  void Start(fidl::InterfaceRequest<mozart::ViewOwner> request) override;
  void GetLink(const fidl::String& name,
                    fidl::InterfaceRequest<Link> request) override;
  void Stop(const StopCallback& callback) override;
  void Watch(fidl::InterfaceHandle<StoryWatcher> watcher) override;
  void AddModule(const fidl::String& module_name,
                 const fidl::String& url,
                 const fidl::String& link_name) override;
  void GetModules(const GetModulesCallback& callback) override;

  // Phases of Start() broken out into separate methods.
  void StartStoryShell(fidl::InterfaceRequest<mozart::ViewOwner> request);
  void StartRootModule(const fidl::String& module_name,
                       const fidl::String& url,
                       const fidl::String& link_name);

  // |ModuleWatcher|
  void OnStateChange(ModuleState new_state) override;

  // Misc internal helpers.
  void NotifyStateChange();
  void DisposeLink(LinkImpl* link);

  // The ID of the story, its state and the context to obtain it from
  // and persist it to.
  const fidl::String story_id_;
  // This is the canonical source for state. The value in the ledger is just
  // a write-behind copy of this value.
  StoryState state_{StoryState::INITIAL};
  StoryProviderImpl* const story_provider_impl_;
  ledger::PagePtr story_page_;
  std::unique_ptr<StoryStorageImpl> story_storage_impl_;

  // The scope in which the modules within this story run.
  Scope story_scope_;

  // Implements the primary service provided here: StoryController.
  fidl::BindingSet<StoryController> bindings_;
  fidl::InterfacePtrSet<StoryWatcher> watchers_;

  // Everything for the story shell. Relationships between modules are
  // conveyed to the story shell using their instance IDs.
  app::ApplicationControllerPtr story_shell_controller_;
  StoryShellPtr story_shell_;
  fidl::Binding<StoryContext> story_context_binding_;
  uint64_t next_module_instance_id_{1};

  // Needed to hold on to a running story. They get reset on Stop().
  fidl::BindingSet<ModuleWatcher> module_watcher_bindings_;

  // The first ingredient of a story: Modules. For each Module in the
  // Story, there is one Connection to it.
  struct Connection {
    std::unique_ptr<ModuleContextImpl> module_context_impl;
    std::unique_ptr<ModuleControllerImpl> module_controller_impl;
  };
  std::vector<Connection> connections_;

  // The second ingredient of a story: Links. They connect Modules.
  std::vector<std::unique_ptr<LinkImpl>> links_;

  // A dummy service that allows applications that can run both as
  // modules in a story and standalone from the shell to determine
  // whether they are in a story. See story_marker.fidl for more
  // details.
  class StoryMarkerImpl;
  std::unique_ptr<StoryMarkerImpl> story_marker_impl_;

  // Asynchronous operations are sequenced in a queue.
  OperationQueue operation_queue_;

  // Operations implemented here.
  class AddModuleCall;
  class AddForCreateCall;
  class StartCall;
  class StartModuleCall;
  class StopCall;
  class DeleteCall;
  class GetModulesCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(StoryImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_STORY_RUNNER_STORY_IMPL_H_
