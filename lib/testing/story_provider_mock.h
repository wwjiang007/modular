// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_LIB_TESTING_STORY_PROVIDER_MOCK_H_
#define APPS_MODULAR_LIB_TESTING_STORY_PROVIDER_MOCK_H_

#include <string>

#include "apps/modular/lib/testing/story_controller_mock.h"
#include "apps/modular/services/story/story_provider.fidl.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_ptr_set.h"

namespace modular {

class StoryProviderMock : public StoryProvider {
 public:
  // Allows notification of watchers.
  void NotifyStoryChanged(modular::StoryInfoPtr story_info) {
    watchers_.ForAllPtrs([&story_info](modular::StoryProviderWatcher* watcher) {
      watcher->OnChange(story_info->Clone());
    });
  }

  modular::StoryControllerMock story_controller() const { return controller_mock_; }
  std::string last_created_story() const { return last_created_story_; }

 private:
  // |StoryProvider|
  void CreateStory(const fidl::String& url,
                   const CreateStoryCallback& callback) override {
    last_created_story_ = url;
    callback("foo");
  }

  // |StoryProvider|
  void CreateStoryWithInfo(
      const fidl::String& url,
      fidl::Map<fidl::String, fidl::String> extra_info,
      const fidl::String& json,
      const CreateStoryWithInfoCallback& callback) override {
    last_created_story_ = url;
    callback("foo");
  }

  // |StoryProvider|
  void Watch(
      fidl::InterfaceHandle<modular::StoryProviderWatcher> watcher) override {
    watchers_.AddInterfacePtr(
        modular::StoryProviderWatcherPtr::Create(std::move(watcher)));
  }

  // |StoryProvider|
  void DeleteStory(const fidl::String& story_id,
                   const DeleteStoryCallback& callback) override {
    callback();
  }

  // |StoryProvider|
  void GetStoryInfo(const fidl::String& story_id,
                    const GetStoryInfoCallback& callback) override {
    callback(nullptr);
  }

  // |StoryProvider|
  void GetController(
      const fidl::String& story_id,
      fidl::InterfaceRequest<modular::StoryController> story) override {
    binding_set_.AddBinding(&controller_mock_, std::move(story));
  }

  // |StoryProvider|
  void PreviousStories(const PreviousStoriesCallback& callback) override {
    callback(fidl::Array<fidl::String>::New(0));
  }

  // |StoryProvider|
  void Duplicate(fidl::InterfaceRequest<StoryProvider> request) override {
    FTL_LOG(FATAL) << "StoryProviderMock::Duplicate() not implemented.";
  }

  std::string last_created_story_;
  modular::StoryControllerMock controller_mock_;
  fidl::BindingSet<modular::StoryController> binding_set_;
  fidl::InterfacePtrSet<modular::StoryProviderWatcher> watchers_;
};

}  // namespace modular

#endif  // APPS_MODULAR_LIB_TESTING_STORY_PROVIDER_MOCK_H_
