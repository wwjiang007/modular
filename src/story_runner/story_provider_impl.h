// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_STORY_RUNNER_STORY_PROVIDER_IMPL_H_
#define APPS_MODULAR_SRC_STORY_RUNNER_STORY_PROVIDER_IMPL_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/maxwell/services/user/user_intelligence_provider.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/lib/fidl/page_client.h"
#include "apps/modular/lib/fidl/scope.h"
#include "apps/modular/services/config/config.fidl.h"
#include "apps/modular/services/story/story_data.fidl.h"
#include "apps/modular/services/story/story_provider.fidl.h"
#include "apps/modular/src/agent_runner/agent_runner.h"
#include "apps/modular/src/component/component_context_impl.h"
#include "apps/modular/src/component/message_queue_manager.h"
#include "apps/modular/src/story_runner/story_storage_impl.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/fidl/cpp/bindings/interface_ptr_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/fidl/cpp/bindings/string.h"
#include "lib/ftl/macros.h"

namespace modular {
class Resolver;
class StoryImpl;

class StoryProviderImpl : StoryProvider, PageClient {
 public:
  StoryProviderImpl(
      const Scope* user_scope,
      const std::string& device_id,
      ledger::Ledger* ledger,
      ledger::Page* root_page,
      AppConfigPtr story_shell,
      const ComponentContextInfo& component_context_info,
      maxwell::UserIntelligenceProvider* user_intelligence_provider);

  ~StoryProviderImpl() override;

  void Connect(fidl::InterfaceRequest<StoryProvider> request);

  // Called by StoryImpl.
  //
  // When the binding set of StoryImpl becomes empty, it asks its owner to
  // delete it.
  void PurgeController(const std::string& story_id);

  // Called by StoryImpl.
  const Scope* user_scope() const { return user_scope_; }

  // The device ID for this user/device.
  const std::string device_id() const { return device_id_; }

  // Called by StoryImpl.
  const ComponentContextInfo& component_context_info() {
    return component_context_info_;
  }

  // Called by StoryImpl.
  maxwell::UserIntelligenceProvider* user_intelligence_provider() {
    return user_intelligence_provider_;
  }

  // Called by StoryImpl.
  const AppConfig& story_shell() const { return *story_shell_; }

  // Called by StoryImpl.
  void SetStoryInfoExtra(const fidl::String& story_id,
                         const fidl::String& name,
                         const fidl::String& value,
                         const std::function<void()>& callback);

  // |StoryProvider|, also used by StoryImpl.
  void GetStoryInfo(const fidl::String& story_id,
                    const GetStoryInfoCallback& callback) override;

 private:
  using FidlStringMap = fidl::Map<fidl::String, fidl::String>;

  // |StoryProvider|
  void CreateStory(const fidl::String& url,
                   const CreateStoryCallback& callback) override;

  // |StoryProvider|
  void CreateStoryWithInfo(
      const fidl::String& url,
      FidlStringMap extra_info,
      const fidl::String& root_json,
      const CreateStoryWithInfoCallback& callback) override;

  // |StoryProvider|
  void DeleteStory(const fidl::String& story_id,
                   const DeleteStoryCallback& callback) override;

  // |StoryProvider|
  void GetController(const fidl::String& story_id,
                     fidl::InterfaceRequest<StoryController> request) override;

  // |StoryProvider|
  void PreviousStories(const PreviousStoriesCallback& callback) override;

  // |StoryProvider|
  void RunningStories(const RunningStoriesCallback& callback) override;

  // |StoryProvider|
  void Watch(fidl::InterfaceHandle<StoryProviderWatcher> watcher) override;

  // |StoryProvider|
  void Duplicate(fidl::InterfaceRequest<StoryProvider> request) override;

  // |PageClient|
  void OnChange(const std::string& key, const std::string& value) override;

  // |PageClient|
  void OnDelete(const std::string& key) override;

  const Scope* const user_scope_;

  // Unique ID generated for this user/device combination.
  std::string device_id_;

  // Story provider writes story records to the root page, and creates
  // new pages for stories.
  ledger::Ledger* const ledger_;
  ledger::Page* const root_page_;

  // The bindings for this instance.
  fidl::BindingSet<StoryProvider> bindings_;

  AppConfigPtr story_shell_;

  fidl::InterfacePtrSet<StoryProviderWatcher> watchers_;

  std::unordered_map<std::string, std::unique_ptr<StoryImpl>>
      story_controllers_;

  const ComponentContextInfo component_context_info_;

  maxwell::UserIntelligenceProvider* const
      user_intelligence_provider_;  // Not owned.

  // This is a container of all operations that are currently enqueued to run in
  // a FIFO manner. All operations exposed via |StoryProvider| interface are
  // queued here.
  //
  // The advantage of doing this is that if an operation consists of multiple
  // asynchronous calls then no state needs to be maintained for incomplete /
  // pending operations.
  //
  // TODO(mesch): If a story provider operation invokes a story operation that
  // causes the story updating its story info state, that update operation gets
  // scheduled on this queue again, after the current operation. It would be
  // better to be able to schedule such an operation on the story queue because
  // it's a per story operation even if it affects the per story key in the root
  // page, and then the update of story info is bounded by the outer operation.
  OperationQueue operation_queue_;

  // Operations implemented here.
  class GetStoryDataCall;
  class WriteStoryDataCall;
  class MutateStoryDataCall;
  class CreateStoryCall;
  class DeleteStoryCall;
  class GetControllerCall;
  class PreviousStoriesCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(StoryProviderImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_STORY_RUNNER_STORY_PROVIDER_IMPL_H_
