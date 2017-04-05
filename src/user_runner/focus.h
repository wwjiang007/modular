// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_USER_RUNNER_FOCUS_H_
#define APPS_MODULAR_SRC_USER_RUNNER_FOCUS_H_

#include <string>
#include <vector>

#include "apps/ledger/services/internal/internal.fidl.h"
#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/lib/fidl/page_client.h"
#include "apps/modular/services/user/focus.fidl.h"
#include "lib/fidl/cpp/bindings/array.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/string.h"

// See services/user/focus.fidl for details.

namespace modular {

class FocusHandler : FocusProvider, FocusController, ledger::PageWatcher {
 public:
  FocusHandler(const fidl::String& device_name, ledger::Page* page);
  ~FocusHandler() override;

  void AddProviderBinding(fidl::InterfaceRequest<FocusProvider> request);
  void AddControllerBinding(fidl::InterfaceRequest<FocusController> request);

 private:
  // |FocusProvider|
  void Query(const QueryCallback& callback) override;
  void Watch(fidl::InterfaceHandle<FocusWatcher> watcher) override;
  void Request(const fidl::String& story_id) override;
  void Duplicate(fidl::InterfaceRequest<FocusProvider> request) override;

  // |FocusController|
  void Set(const fidl::String& story_id) override;
  void WatchRequest(
      fidl::InterfaceHandle<FocusRequestWatcher> watcher) override;

  // |ledger::PageWatcher|
  void OnChange(ledger::PageChangePtr page,
                ledger::ResultState result_state,
                const OnChangeCallback& callback) override;

  ledger::Page* const page_;
  PageClient client_;
  fidl::Binding<ledger::PageWatcher> page_watcher_binding_;

  const std::string device_name_;

  fidl::BindingSet<FocusProvider> provider_bindings_;
  fidl::BindingSet<FocusController> controller_bindings_;

  std::vector<FocusWatcherPtr> change_watchers_;
  std::vector<FocusRequestWatcherPtr> request_watchers_;

  // Operations on an instance of this class are sequenced in this
  // operation queue.
  OperationQueue operation_queue_;

  // Operations implemented here.
  class QueryCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(FocusHandler);
};

class VisibleStoriesHandler : VisibleStoriesProvider, VisibleStoriesController {
 public:
  VisibleStoriesHandler();
  ~VisibleStoriesHandler() override;

  void AddProviderBinding(
      fidl::InterfaceRequest<VisibleStoriesProvider> request);
  void AddControllerBinding(
      fidl::InterfaceRequest<VisibleStoriesController> request);

 private:
  // |VisibleStoriesProvider|
  void Query(const QueryCallback& callback) override;
  void Watch(fidl::InterfaceHandle<VisibleStoriesWatcher> watcher) override;
  void Duplicate(
      fidl::InterfaceRequest<VisibleStoriesProvider> request) override;

  // |VisibleStoriesController|
  void Set(fidl::Array<fidl::String> story_ids) override;

  fidl::BindingSet<VisibleStoriesProvider> provider_bindings_;
  fidl::BindingSet<VisibleStoriesController> controller_bindings_;

  std::vector<VisibleStoriesWatcherPtr> change_watchers_;

  fidl::Array<fidl::String> visible_stories_;

  FTL_DISALLOW_COPY_AND_ASSIGN(VisibleStoriesHandler);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_USER_RUNNER_FOCUS_H_
