// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_STORY_RUNNER_STORY_STORAGE_IMPL_H_
#define APPS_MODULAR_SRC_STORY_RUNNER_STORY_STORAGE_IMPL_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/lib/fidl/page_client.h"
#include "apps/modular/services/module/module_data.fidl.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"

namespace modular {

class LinkImpl;
  
// A wrapper around a ledger page to store links in a story that runs
// asynchronous operations pertaining to one Story instance in a
// dedicated OperationQueue instance.
class StoryStorageImpl : public PageClient {
 public:
  using DataCallback = std::function<void(const fidl::String&)>;
  using SyncCallback = std::function<void()>;
  using AllModuleDataCallback = std::function<void(fidl::Array<ModuleDataPtr>)>;
  using ModuleDataCallback = std::function<void(ModuleDataPtr)>;

  StoryStorageImpl(ledger::Page* story_page);
  ~StoryStorageImpl() override;

  void ReadLinkData(const LinkPathPtr& link_path,
                    const DataCallback& callback);
  void WriteLinkData(const LinkPathPtr& link_path,
                     const fidl::String& data,
                     const SyncCallback& callback);

  void ReadModuleData(const fidl::Array<fidl::String>& module_path,
                      const ModuleDataCallback& callback);
  void ReadAllModuleData(const AllModuleDataCallback& callback);

  void WriteModuleData(const fidl::Array<fidl::String>& module_path,
                       const fidl::String& module_url,
                       const LinkPathPtr& link_path,
                       const SyncCallback& callback);

  void WatchLink(const LinkPathPtr& link_path, LinkImpl* impl,
		 const DataCallback& watcher);
  void DropWatcher(LinkImpl* impl);
  void Sync(const SyncCallback& callback);

 private:
  // |PageClient|
  void OnChange(const std::string& key, const std::string& value) override;

  // Clients to notify when the value of a given link changes in the
  // ledger page. The first element in the pair is the key for the link path.
  struct WatcherEntry {
    std::string key;
    LinkImpl* impl;  // Not owned.
    DataCallback watcher;
  };
  std::vector<WatcherEntry> watchers_;

  // The ledger page the story data is stored in.
  ledger::Page* const story_page_;

  // All asynchronous operations are sequenced by this operation
  // queue.
  OperationQueue operation_queue_;

  // Operations implemented here.
  class ReadLinkDataCall;
  class WriteLinkDataCall;
  class ReadModuleDataCall;
  class ReadAllModuleDataCall;
  class WriteModuleDataCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(StoryStorageImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_STORY_RUNNER_STORY_STORAGE_IMPL_H_
