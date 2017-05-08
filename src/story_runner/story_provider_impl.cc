// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/story_runner/story_provider_impl.h"

#include <vector>

#include "application/lib/app/connect.h"
#include "apps/modular/lib/fidl/array_to_string.h"
#include "apps/modular/lib/fidl/json_xdr.h"
#include "apps/modular/lib/ledger/storage.h"
#include "apps/modular/lib/rapidjson/rapidjson.h"
#include "apps/modular/src/story_runner/story_impl.h"
#include "lib/fidl/cpp/bindings/array.h"
#include "lib/fidl/cpp/bindings/interface_handle.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/mtl/tasks/message_loop.h"
#include "lib/mtl/vmo/strings.h"

namespace modular {

namespace {

// Serialization and deserialization of StoryData and StoryInfo to and
// from JSON.

void XdrStoryInfo(XdrContext* const xdr, StoryInfo* const data) {
  xdr->Field("url", &data->url);
  xdr->Field("id", &data->id);
  xdr->Field("extra", &data->extra);
}

void XdrStoryData(XdrContext* const xdr, StoryData* const data) {
  xdr->Field("story_info", &data->story_info, XdrStoryInfo);
  xdr->Field("story_page_id", &data->story_page_id);
}

}  // namespace

// Below are helper classes that encapsulate a chain of asynchronous
// operations on the Ledger. Because the operations all return
// something, the handles on which they are invoked need to be kept
// around until the return value arrives. This precludes them to be
// local variables. The next thing that comes to mind is to make them
// fields of the containing object. However, there might be multiple
// such operations going on concurrently in one StoryProviderImpl
// (although right now there are not, because they are all serialized
// in one operation queue), so they cannot be fields of
// StoryProviderImpl. Thus such operations are separate classes.

class StoryProviderImpl::GetStoryDataCall : Operation<StoryDataPtr> {
 public:
  GetStoryDataCall(OperationContainer* const container,
                   ledger::Page* page,
                   const fidl::String& story_id,
                   ResultCall result_call)
      : Operation(container, std::move(result_call)),
        page_(page),
        story_id_(story_id) {
    Ready();
  }

 private:
  void Run() override {
    FlowToken flow(this, &story_data_);

    page_->GetSnapshot(page_snapshot_.NewRequest(), nullptr, nullptr,
                       [this, flow](ledger::Status status) {
                         if (status != ledger::Status::OK) {
                           return;
                         }

                         Cont(flow);
                       });
  }

  void Cont(FlowToken flow) {
    page_snapshot_->Get(
        to_array(MakeStoryKey(story_id_)),
        [this, flow](ledger::Status status, mx::vmo value) {
          if (status != ledger::Status::OK) {
            // It's always OK if the story is not found, all clients
            // handle the null case.
            if (status != ledger::Status::KEY_NOT_FOUND) {
              FTL_LOG(ERROR) << "GetStoryDataCall() " << story_id_
                             << " PageSnapshot.Get() " << status;
            }
            return;
          }

          std::string value_as_string;
          if (!mtl::StringFromVmo(value, &value_as_string)) {
            FTL_LOG(ERROR) << "GetStoryDataCall() " << story_id_
                           << "Unable to extract data.";
            return;
          }

          if (!XdrRead(value_as_string, &story_data_, XdrStoryData)) {
            story_data_.reset();
          }
        });
  };

  ledger::Page* const page_;  // not owned
  ledger::PageSnapshotPtr page_snapshot_;
  const fidl::String story_id_;
  StoryDataPtr story_data_;

  FTL_DISALLOW_COPY_AND_ASSIGN(GetStoryDataCall);
};

class StoryProviderImpl::WriteStoryDataCall : Operation<void> {
 public:
  WriteStoryDataCall(OperationContainer* const container,
                     ledger::Page* const page,
                     StoryDataPtr story_data,
                     ResultCall result_call)
      : Operation(container, std::move(result_call)),
        page_(page),
        story_data_(std::move(story_data)) {
    Ready();
  }

 private:
  void Run() override {
    FTL_DCHECK(!story_data_.is_null());

    std::string json;
    XdrWrite(&json, &story_data_, XdrStoryData);

    page_->PutWithPriority(
        to_array(MakeStoryKey(story_data_->story_info->id)), to_array(json),
        ledger::Priority::EAGER, [this](ledger::Status status) {
          if (status != ledger::Status::OK) {
            const fidl::String& story_id = story_data_->story_info->id;
            FTL_LOG(ERROR) << "WriteStoryDataCall() " << story_id
                           << " Page.PutWithPriority() " << status;
          }

          Done();
        });
  }

  ledger::Page* const page_;  // not owned
  StoryDataPtr story_data_;

  FTL_DISALLOW_COPY_AND_ASSIGN(WriteStoryDataCall);
};

class StoryProviderImpl::MutateStoryDataCall : Operation<void> {
 public:
  MutateStoryDataCall(OperationContainer* const container,
                      ledger::Page* const page,
                      const fidl::String& story_id,
                      std::function<bool(StoryData* story_data)> mutate,
                      ResultCall result_call)
      : Operation(container, std::move(result_call)),
        page_(page),
        story_id_(story_id),
        mutate_(mutate) {
    Ready();
  }

 private:
  void Run() override {
    new GetStoryDataCall(
        &operation_queue_, page_, story_id_, [this](StoryDataPtr story_data) {
          if (!story_data) {
            // If the story doesn't exist, it was deleted and
            // we must not bring it back.
            Done();
            return;
          }
          if (!mutate_(story_data.get())) {
            // If no mutation happened, we're done.
            Done();
            return;
          }

          new WriteStoryDataCall(&operation_queue_, page_,
                                 std::move(story_data), [this] { Done(); });
        });
  }

  ledger::Page* const page_;  // not owned
  const fidl::String story_id_;
  std::function<bool(StoryData* story_data)> mutate_;

  OperationQueue operation_queue_;

  FTL_DISALLOW_COPY_AND_ASSIGN(MutateStoryDataCall);
};

// 1. Create a page for the new story.
// 2. Create a new StoryData structure pointing to this new page and save it
//    to the root page.
// 3. Returns the Story ID of the newly created story.
class StoryProviderImpl::CreateStoryCall : Operation<fidl::String> {
 public:
  CreateStoryCall(OperationContainer* const container,
                  ledger::Ledger* const ledger,
                  ledger::Page* const root_page,
                  StoryProviderImpl* const story_provider_impl,
                  const fidl::String& url,
                  FidlStringMap extra_info,
                  fidl::String root_json,
                  ResultCall result_call)
      : Operation(container, std::move(result_call)),
        ledger_(ledger),
        root_page_(root_page),
        story_provider_impl_(story_provider_impl),
        url_(url),
        extra_info_(std::move(extra_info)),
        root_json_(std::move(root_json)) {
    Ready();
  }

 private:
  void Run() override {
    ledger_->GetPage(
        nullptr, story_page_.NewRequest(), [this](ledger::Status status) {
          if (status != ledger::Status::OK) {
            FTL_LOG(ERROR) << "CreateStoryCall()"
                           << " Ledger.GetPage() " << status;
            Done(std::move(story_id_));
            return;
          }

          story_page_->GetId([this](fidl::Array<uint8_t> id) {
            story_page_id_ = std::move(id);

            // TODO(security), cf. FW-174. This ID is exposed in
            // public services such as
            // StoryProvider.PreviousStories(),
            // StoryController.GetInfo(),
            // ModuleContext.GetStoryId(). We need to ensure this
            // doesn't expose internal information by being a
            // page ID.
            story_id_ = to_hex_string(story_page_id_);

            story_data_ = StoryData::New();
            story_data_->story_page_id = story_page_id_.Clone();
            story_data_->story_info = StoryInfo::New();
            auto* const story_info = story_data_->story_info.get();
            story_info->url = url_;
            story_info->id = story_id_;
            story_info->extra = std::move(extra_info_);
            story_info->extra.mark_non_null();

            new WriteStoryDataCall(&operation_queue_, root_page_,
                                   std::move(story_data_), [this] { Cont(); });
          });
        });
  }

  void Cont() {
    controller_ = std::make_unique<StoryImpl>(story_id_, std::move(story_page_),
                                              story_provider_impl_);

    // We ensure that root data has been written before this operation is
    // done.
    controller_->AddForCreate(kRootModuleName, url_, kRootLink, root_json_,
                              [this] { Done(std::move(story_id_)); });
  }

  ledger::Ledger* const ledger_;                  // not owned
  ledger::Page* const root_page_;                 // not owned
  StoryProviderImpl* const story_provider_impl_;  // not owned
  const fidl::String module_name_;
  const fidl::String url_;
  FidlStringMap extra_info_;
  fidl::String root_json_;

  ledger::PagePtr story_page_;
  StoryDataPtr story_data_;
  std::unique_ptr<StoryImpl> controller_;

  fidl::Array<uint8_t> story_page_id_;
  fidl::String story_id_;  // This is the result of the Operation.

  // Sub operations run in this queue.
  OperationQueue operation_queue_;

  FTL_DISALLOW_COPY_AND_ASSIGN(CreateStoryCall);
};

class StoryProviderImpl::DeleteStoryCall : Operation<void> {
 public:
  using ControllerMap =
      std::unordered_map<std::string, std::unique_ptr<StoryImpl>>;
  using PendingDeletion = std::pair<std::string, DeleteStoryCall*>;

  DeleteStoryCall(OperationContainer* const container,
                  ledger::Page* const page,
                  const fidl::String& story_id,
                  ControllerMap* const story_controllers,
                  const bool already_deleted,
                  ResultCall result_call)
      : Operation(container, std::move(result_call)),
        page_(page),
        story_id_(story_id),
        story_controllers_(story_controllers),
        already_deleted_(already_deleted) {
    Ready();
  }

 private:
  void Run() override {
    // TODO(mesch): If the order of StopForDelete() and deletion from ledger is
    // reversed, we don't need to bother with suppressing writes to the ledger
    // during StopForDelete(), which could be simpler.

    if (already_deleted_) {
      Teardown();

    } else {
      page_->Delete(to_array(MakeStoryKey(story_id_)),
                    [this](ledger::Status status) {
                      // Deleting a key that doesn't exist is OK, not
                      // KEY_NOT_FOUND.
                      if (status != ledger::Status::OK) {
                        FTL_LOG(ERROR) << "DeleteStoryCall() " << story_id_
                                       << " Page.Delete() " << status;
                      }

                      Teardown();
                    });
    }
  }

  void Teardown() {
    auto i = story_controllers_->find(story_id_);
    if (i == story_controllers_->end()) {
      Done();
      return;
    }

    FTL_DCHECK(i->second.get() != nullptr);
    i->second->StopForDelete([this] { Erase(); });
  }

  void Erase() {
    // Here we delete the instance from whose operation a result callback was
    // received. Thus we must assume that the callback returns to a method of
    // the instance. If we delete the instance right here, |this| would be
    // deleted not just for the remainder of this function here, but also for
    // the remainder of all functions above us in the callstack, including
    // functions that run as methods of other objects owned by |this| or
    // provided to |this|.  To avoid such problems, the delete is invoked
    // through the run loop.
    mtl::MessageLoop::GetCurrent()->task_runner()->PostTask([this] {
      story_controllers_->erase(story_id_);
      Done();
    });
  }

 private:
  ledger::Page* const page_;  // not owned
  const fidl::String story_id_;
  ControllerMap* const story_controllers_;
  const bool already_deleted_;  // True if called from OnChange();

  FTL_DISALLOW_COPY_AND_ASSIGN(DeleteStoryCall);
};

// 1. Ensure that the story data in the root page isn't dirty due to a crash
// 2. Retrieve the page specific to this story.
// 3. Return a controller for this story that contains the page pointer.
class StoryProviderImpl::GetControllerCall : Operation<void> {
 public:
  using ControllerMap =
      std::unordered_map<std::string, std::unique_ptr<StoryImpl>>;

  GetControllerCall(OperationContainer* const container,
                    ledger::Ledger* const ledger,
                    ledger::Page* const page,
                    StoryProviderImpl* const story_provider_impl,
                    ControllerMap* const story_controllers,
                    const fidl::String& story_id,
                    fidl::InterfaceRequest<StoryController> request)
      : Operation(container, [] {}),
        ledger_(ledger),
        page_(page),
        story_provider_impl_(story_provider_impl),
        story_controllers_(story_controllers),
        story_id_(story_id),
        request_(std::move(request)) {
    Ready();
  }

 private:
  void Run() override {
    FlowToken flow(this);

    // Use the existing controller, if possible.
    auto i = story_controllers_->find(story_id_);
    if (i != story_controllers_->end()) {
      i->second->Connect(std::move(request_));
      return;
    }

    new GetStoryDataCall(&operation_queue_, page_, story_id_,
                         [this, flow](StoryDataPtr story_data) {
                           if (story_data) {
                             story_data_ = std::move(story_data);
                             Cont1(flow);
                           }
                         });
  }

  void Cont1(FlowToken flow) {
    ledger_->GetPage(
        story_data_->story_page_id.Clone(), story_page_.NewRequest(),
        [this, flow](ledger::Status status) {
          if (status != ledger::Status::OK) {
            FTL_LOG(ERROR) << "GetControllerCall() " << story_id_
                           << " Ledger.GetPage() " << status;
          }
          StoryImpl* const controller = new StoryImpl(
              story_id_, std::move(story_page_), story_provider_impl_);
          controller->Connect(std::move(request_));
          story_controllers_->emplace(story_id_, controller);
        });
  }

  ledger::Ledger* const ledger_;                  // not owned
  ledger::Page* const page_;                      // not owned
  StoryProviderImpl* const story_provider_impl_;  // not owned
  ControllerMap* const story_controllers_;
  const fidl::String story_id_;
  fidl::InterfaceRequest<StoryController> request_;

  StoryDataPtr story_data_;
  ledger::PagePtr story_page_;

  // Sub operations run in this queue.
  OperationQueue operation_queue_;

  FTL_DISALLOW_COPY_AND_ASSIGN(GetControllerCall);
};

class StoryProviderImpl::PreviousStoriesCall
    : Operation<fidl::Array<fidl::String>> {
 public:
  PreviousStoriesCall(OperationContainer* const container,
                      ledger::Page* const page,
                      ResultCall result_call)
      : Operation(container, std::move(result_call)), page_(page) {
    // This resize() has the side effect of marking the array as non-null. Do
    // not remove it because the fidl declaration of this return value does not
    // allow nulls.
    story_ids_.resize(0);

    Ready();
  }

 private:
  void Run() override {
    page_->GetSnapshot(page_snapshot_.NewRequest(), nullptr, nullptr,
                       [this](ledger::Status status) {
                         if (status != ledger::Status::OK) {
                           FTL_LOG(ERROR) << "PreviousStoriesCall() "
                                          << "Page.GetSnapshot() " << status;
                           Done(std::move(story_ids_));
                           return;
                         }

                         Cont1();
                       });
  }

  void Cont1() {
    GetEntries(page_snapshot_.get(), kStoryKeyPrefix, &entries_,
               nullptr /* next_token */, [this](ledger::Status status) {
                 if (status != ledger::Status::OK) {
                   FTL_LOG(ERROR) << "PreviousStoriesCall() "
                                  << "GetEntries() " << status;
                   Done(std::move(story_ids_));
                   return;
                 }

                 Cont2();
               });
  }

  void Cont2() {
    // TODO(mesch): Pagination might be needed here. If the list
    // of entries returned from the Ledger is too large, it might
    // also be too large to return from StoryProvider.

    for (auto& entry : entries_) {
      std::string value_as_string;
      if (!mtl::StringFromVmo(entry->value, &value_as_string)) {
        FTL_LOG(ERROR) << "PreviousStoriesCall() "
                       << "Unable to extract data.";
        Done(nullptr);
        return;
      }

      StoryDataPtr story_data;
      if (!XdrRead(value_as_string, &story_data, XdrStoryData)) {
        Done(nullptr);
        return;
      }

      FTL_DCHECK(!story_data.is_null());

      story_ids_.push_back(story_data->story_info->id);
    }
    Done(std::move(story_ids_));
  }

  ledger::Page* const page_;  // not owned
  ledger::PageSnapshotPtr page_snapshot_;
  std::vector<ledger::EntryPtr> entries_;
  fidl::Array<fidl::String> story_ids_;

  FTL_DISALLOW_COPY_AND_ASSIGN(PreviousStoriesCall);
};

class StoryProviderImpl::TeardownCall : Operation<void> {
 public:
  TeardownCall(OperationContainer* const container,
               StoryProviderImpl* const story_provider_impl,
               ResultCall result_call)
      : Operation(container, std::move(result_call)),
        story_provider_impl_(story_provider_impl) {
    Ready();
  }

 private:
  void Run() override {
    FlowToken flow(this);
    for (auto& it : story_provider_impl_->story_controllers_) {
      // A new instance of |cont| gets creted for each running story. Each
      // |const| has a copy of |flow| which only goes out-of-scope once the
      // story corresponding to |it| stops.
      it.second->StopForTeardown([this, story_id = it.first, flow] {
        // It is okay to erase story_id because story provider binding has been
        // closed and |cont| cannot be invoked asynchronously.
        story_provider_impl_->story_controllers_.erase(story_id);
      });
    }
   }

  StoryProviderImpl* const story_provider_impl_;  // not owned

  FTL_DISALLOW_COPY_AND_ASSIGN(TeardownCall);
};

StoryProviderImpl::StoryProviderImpl(
    const Scope* const user_scope,
    const std::string& device_id,
    ledger::Ledger* const ledger,
    ledger::Page* const root_page,
    AppConfigPtr story_shell,
    const ComponentContextInfo& component_context_info,
    maxwell::UserIntelligenceProvider* const user_intelligence_provider)
    : PageClient("StoryProviderImpl", root_page, kStoryKeyPrefix),
      user_scope_(user_scope),
      device_id_(device_id),
      ledger_(ledger),
      root_page_(root_page),
      story_shell_(std::move(story_shell)),
      component_context_info_(component_context_info),
      user_intelligence_provider_(user_intelligence_provider) {}

StoryProviderImpl::~StoryProviderImpl() = default;

void StoryProviderImpl::Connect(fidl::InterfaceRequest<StoryProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

void StoryProviderImpl::Teardown(const std::function<void()>& callback) {
  // Closing all binding to this instance ensures that no new messages come
  // in, though previous messages need to be processed. The stopping of stories
  // is done on |operation_queue_| since that must strictly happen after all
  // pending messgages have been processed.
  bindings_.CloseAllBindings();
  new TeardownCall(&operation_queue_, this, callback);
}

// |StoryProvider|
void StoryProviderImpl::Watch(
    fidl::InterfaceHandle<StoryProviderWatcher> watcher) {
  watchers_.AddInterfacePtr(
      StoryProviderWatcherPtr::Create(std::move(watcher)));
}

// |StoryProvider|
void StoryProviderImpl::Duplicate(
    fidl::InterfaceRequest<StoryProvider> request) {
  Connect(std::move(request));
}

void StoryProviderImpl::SetStoryInfoExtra(const fidl::String& story_id,
                                          const fidl::String& name,
                                          const fidl::String& value,
                                          const std::function<void()>& done) {
  auto mutate = [name, value](StoryData* const story_data) {
    story_data->story_info->extra[name] = value;
    return true;
  };

  new MutateStoryDataCall(&operation_queue_, root_page_, story_id, mutate,
                          done);
};

// |StoryProvider|
void StoryProviderImpl::CreateStory(const fidl::String& module_url,
                                    const CreateStoryCallback& callback) {
  FTL_LOG(INFO) << "CreateStory() " << module_url;
  new CreateStoryCall(&operation_queue_, ledger_, root_page_, this, module_url,
                      FidlStringMap(), fidl::String(), callback);
}

// |StoryProvider|
void StoryProviderImpl::CreateStoryWithInfo(
    const fidl::String& module_url,
    FidlStringMap extra_info,
    const fidl::String& root_json,
    const CreateStoryWithInfoCallback& callback) {
  FTL_LOG(INFO) << "CreateStoryWithInfo() " << root_json;
  new CreateStoryCall(&operation_queue_, ledger_, root_page_, this, module_url,
                      std::move(extra_info), std::move(root_json), callback);
}

// |StoryProvider|
void StoryProviderImpl::DeleteStory(const fidl::String& story_id,
                                    const DeleteStoryCallback& callback) {
  new DeleteStoryCall(&operation_queue_, root_page_, story_id,
                      &story_controllers_, false /* already_deleted */,
                      callback);
}

// |StoryProvider|
void StoryProviderImpl::GetStoryInfo(const fidl::String& story_id,
                                     const GetStoryInfoCallback& callback) {
  new GetStoryDataCall(
      &operation_queue_, root_page_, story_id,
      [callback](StoryDataPtr story_data) {
        callback(story_data ? std::move(story_data->story_info) : nullptr);
      });
}

// |StoryProvider|
void StoryProviderImpl::GetController(
    const fidl::String& story_id,
    fidl::InterfaceRequest<StoryController> request) {
  new GetControllerCall(&operation_queue_, ledger_, root_page_, this,
                        &story_controllers_, story_id, std::move(request));
}

// |StoryProvider|
void StoryProviderImpl::PreviousStories(
    const PreviousStoriesCallback& callback) {
  new PreviousStoriesCall(&operation_queue_, root_page_, callback);
}

// |StoryProvider|
void StoryProviderImpl::RunningStories(const RunningStoriesCallback& callback) {
  auto stories = fidl::Array<fidl::String>::New(0);
  for (const auto& controller : story_controllers_) {
    if (controller.second->IsRunning()) {
      stories.push_back(controller.second->GetStoryId());
    }
  }
  callback(std::move(stories));
}

// |PageClient|
void StoryProviderImpl::OnChange(const std::string& key,
                                 const std::string& value) {
  auto story_data = StoryData::New();
  if (!XdrRead(value, &story_data, XdrStoryData)) {
    return;
  }

  // HACK(jimbe) We don't have the page and it's expensive to get it, so just
  // mark it as STOPPED. We know it's not running or we'd have a
  // StoryController.
  StoryState state = StoryState::STOPPED;
  auto i = story_controllers_.find(story_data->story_info->id);
  if (i != story_controllers_.end()) {
    state = i->second->GetStoryState();
  }

  watchers_.ForAllPtrs(
      [&story_data, state](StoryProviderWatcher* const watcher) {
        watcher->OnChange(story_data->story_info.Clone(), state);
      });

  // TODO(mesch): If there is an update for a running story, the story
  // controller needs to be notified.
}

// |PageClient|
void StoryProviderImpl::OnDelete(const std::string& key) {
  // Extract the story ID from the ledger key. cf. kStoryKeyPrefix.
  const fidl::String story_id = key.substr(sizeof(kStoryKeyPrefix) - 1);

  watchers_.ForAllPtrs([&story_id](StoryProviderWatcher* const watcher) {
    watcher->OnDelete(story_id);
  });

  new DeleteStoryCall(&operation_queue_, root_page_, story_id,
                      &story_controllers_, true /* already_deleted */, [] {});
}

}  // namespace modular
