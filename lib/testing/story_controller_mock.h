// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_LIB_TESTING_STORY_CONTROLLER_MOCK_H_
#define APPS_MODULAR_LIB_TESTING_STORY_CONTROLLER_MOCK_H_

#include <string>

#include "apps/modular/services/story/story_controller.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/interface_ptr_set.h"

namespace modular {

class StoryControllerMock : public StoryController {
 public:
  std::string last_added_module() const { return last_added_module_; }

 private:
  // |StoryController|
  void GetInfo(const GetInfoCallback& callback) override {
    auto info = StoryInfo::New();
    info->id = "wow";
    info->url = "wow";
    info->extra.mark_non_null();
    callback(std::move(info));
  }

  // |StoryController|
  void SetInfoExtra(const fidl::String& name,
                    const fidl::String& value,
                    const SetInfoExtraCallback& callback) override {}

  // |StoryController|
  void AddModule(const fidl::String& module_name,
                 const fidl::String& module_url,
                 const fidl::String& link_name) override {
    last_added_module_ = module_url;
  }

  // |StoryController|
  void Start(fidl::InterfaceRequest<mozart::ViewOwner> request) override {}

  // |StoryController|
  void GetLink(fidl::InterfaceRequest<Link> request) override {}

  // |StoryController|
  void GetNamedLink(const fidl::String& name,
                    fidl::InterfaceRequest<Link> request) override {}

  // |StoryController|
  void Stop(const StopCallback& done) override {}

  // |StoryController|
  void Watch(fidl::InterfaceHandle<StoryWatcher> watcher) override {}

  // |StoryController|
  void GetModules(const GetModulesCallback& callback) override {}

  std::string last_added_module_;
};

}  // namespace modular

#endif  // APPS_MODULAR_LIB_TESTING_STORY_PROVIDER_MOCK_H_
