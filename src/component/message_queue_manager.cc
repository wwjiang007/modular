// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/component/message_queue_manager.h"

#include <algorithm>
#include <deque>

#include "apps/modular/lib/fidl/array_to_string.h"
#include "apps/modular/lib/fidl/json_xdr.h"
#include "apps/modular/lib/ledger/storage.h"
#include "apps/modular/services/component/message_queue.fidl.h"
#include "apps/modular/src/component/persistent_queue.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/ftl/strings/string_printf.h"
#include "lib/mtl/vmo/strings.h"

namespace modular {

class MessageQueueStorage;

// This class implements the |MessageQueue| fidl interface, and is owned by
// |MessageQueueStorage|. It forwards all calls to its owner, and expects its
// owner to manage outstanding |MessageQueue.Receive| calls. It also notifies
// its owner on object destruction.
//
// Interface is public, because bindings are outside of the class.
class MessageQueueConnection : public MessageQueue {
 public:
  explicit MessageQueueConnection(MessageQueueStorage* queue_storage);
  ~MessageQueueConnection() override;

 private:
  // |MessageQueue|
  void Receive(const MessageQueue::ReceiveCallback& callback) override;

  // |MessageQueue|
  void GetToken(const GetTokenCallback& callback) override;

  MessageQueueStorage* const queue_storage_;
};

// Class for managing a particular message queue, its tokens and its storage.
// Implementations of |MessageQueue| and |MessageSender| call into this class to
// manipulate the message queue. Owned by |MessageQueueManager|.
class MessageQueueStorage : MessageSender {
 public:
  MessageQueueStorage(const std::string& queue_token,
                      const std::string& file_name_)
      : queue_token_(queue_token), queue_data_(file_name_) {}

  ~MessageQueueStorage() override = default;

  // We store |Receive()| callbacks if the queue is empty. We drop these
  // callbacks if the relevant |MessageQueue| interface dies.
  void Receive(const MessageQueueConnection* const conn,
               const MessageQueue::ReceiveCallback& callback) {
    if (!queue_data_.IsEmpty()) {
      callback(queue_data_.Dequeue());
    } else {
      receive_callback_queue_.push_back(make_pair(conn, callback));
    }
  }

  const std::string& queue_token() const { return queue_token_; }

  void AddMessageSenderBinding(fidl::InterfaceRequest<MessageSender> request) {
    message_sender_bindings_.AddBinding(this, std::move(request));
  }

  void AddMessageQueueBinding(fidl::InterfaceRequest<MessageQueue> request) {
    message_queue_bindings_.AddBinding(
        std::make_unique<MessageQueueConnection>(this), std::move(request));
  }

  // |MessageQueueConnection| calls this method in its destructor so that we can
  // drop any pending receive callbacks.
  void RemoveMessageQueueConnection(const MessageQueueConnection* const conn) {
    auto i = std::remove_if(receive_callback_queue_.begin(),
                            receive_callback_queue_.end(),
                            [conn](const ReceiveCallbackQueueItem& item) {
                              return conn == item.first;
                            });
    receive_callback_queue_.erase(i, receive_callback_queue_.end());
  }

  void RegisterWatcher(const std::function<void()>& watcher) {
    watcher_ = watcher;
    if (watcher_) {
      watcher_();
    }
  }

  void DropWatcher() { watcher_ = nullptr; }

 private:
  // |MessageSender|
  void Send(const fidl::String& message) override {
    if (!receive_callback_queue_.empty()) {
      auto& receive_item = receive_callback_queue_.front();
      receive_item.second(message);
      receive_callback_queue_.pop_front();
    } else {
      queue_data_.Enqueue(message);
    }

    if (watcher_) {
      watcher_();
    }
  }

  const std::string queue_token_;

  std::function<void()> watcher_;

  PersistentQueue queue_data_;

  using ReceiveCallbackQueueItem =
      std::pair<const MessageQueueConnection*, MessageQueue::ReceiveCallback>;
  std::deque<ReceiveCallbackQueueItem> receive_callback_queue_;

  // When a |MessageQueue| connection closes, the corresponding
  // MessageQueueConnection instance gets removed (and destroyed due
  // to unique_ptr semantics), which in turn will notify our
  // RemoveMessageQueueConnection method.
  fidl::BindingSet<MessageQueue, std::unique_ptr<MessageQueueConnection>>
      message_queue_bindings_;

  fidl::BindingSet<MessageSender> message_sender_bindings_;
};

// MessageQueueConnection -----------------------------------------------------

MessageQueueConnection::MessageQueueConnection(
    MessageQueueStorage* const queue_storage)
    : queue_storage_(queue_storage) {}

MessageQueueConnection::~MessageQueueConnection() {
  queue_storage_->RemoveMessageQueueConnection(this);
}

void MessageQueueConnection::Receive(
    const MessageQueue::ReceiveCallback& callback) {
  queue_storage_->Receive(this, callback);
}

void MessageQueueConnection::GetToken(const GetTokenCallback& callback) {
  callback(queue_storage_->queue_token());
}

// MessageQueueManager --------------------------------------------------------

namespace {

std::string GenerateQueueToken() {
  // Get 256 bits of pseudo-randomness.
  uint8_t randomness[256 / 8];
  size_t random_size;
  mx_cprng_draw(&randomness, sizeof randomness, &random_size);
  // TODO: is there a more efficient way to do this?
  std::string token;
  for (uint8_t byte : randomness) {
    ftl::StringAppendf(&token, "%X", byte);
  }
  return token;
}

}  // namespace

struct MessageQueueManager::MessageQueueInfo {
  std::string component_namespace;
  std::string component_instance_id;
  std::string queue_name;
  std::string queue_token;

  bool is_complete() const {
    return !component_instance_id.empty() && !queue_name.empty();
  }
};

class MessageQueueManager::GetQueueTokenCall : Operation<fidl::String> {
 public:
  GetQueueTokenCall(OperationContainer* const container,
                    ledger::Page* const page,
                    const std::string& component_namespace,
                    const std::string& component_instance_id,
                    const std::string& queue_name,
                    ResultCall result_call)
      : Operation(container, std::move(result_call)),
        page_(page),
        component_namespace_(component_namespace),
        component_instance_id_(component_instance_id),
        queue_name_(queue_name) {
    Ready();
  }

 private:
  void Run() override {
    page_->GetSnapshot(
        snapshot_.NewRequest(), nullptr, nullptr,
        [this](ledger::Status status) {
          if (status != ledger::Status::OK) {
            FTL_LOG(ERROR) << "Ledger.GetSnapshot() " << status;
            Done(std::move(nullptr));
            return;
          }

          snapshot_.set_connection_error_handler(
              [] { FTL_LOG(WARNING) << "Error on snapshot connection"; });

          key_ = MakeMessageQueueTokenKey(component_namespace_,
                                          component_instance_id_, queue_name_);
          snapshot_->Get(
              to_array(key_), [this](ledger::Status status, mx::vmo value) {
                if (status == ledger::Status::KEY_NOT_FOUND) {
                  // Key wasn't found, that's not an error.
                  Done(nullptr);
                  return;
                }

                if (status != ledger::Status::OK) {
                  FTL_LOG(ERROR) << "Failed to get key " << key_;
                  Done(nullptr);
                  return;
                }

                if (!value) {
                  FTL_LOG(ERROR) << "Key " << key_ << " has no value";
                  Done(nullptr);
                  return;
                }

                std::string queue_token;
                if (!mtl::StringFromVmo(value, &queue_token)) {
                  FTL_LOG(ERROR)
                      << "VMO for key " << key_ << " couldn't be copied.";
                  Done(nullptr);
                  return;
                }

                Done(queue_token);
              });
        });
  }

  ledger::Page* const page_;  // not owned
  const std::string component_namespace_;
  const std::string component_instance_id_;
  const std::string queue_name_;
  ledger::PageSnapshotPtr snapshot_;
  std::string key_;

  FTL_DISALLOW_COPY_AND_ASSIGN(GetQueueTokenCall);
};

class MessageQueueManager::GetMessageSenderCall : Operation<void> {
 public:
  GetMessageSenderCall(OperationContainer* const container,
                       MessageQueueManager* const message_queue_manager,
                       ledger::Page* const page,
                       const std::string& token,
                       fidl::InterfaceRequest<MessageSender> request)
      : Operation(container, [] {}),
        message_queue_manager_(message_queue_manager),
        page_(page),
        token_(token),
        request_(std::move(request)) {
    Ready();
  }

 private:
  void Run() override {
    page_->GetSnapshot(
        snapshot_.NewRequest(), nullptr, nullptr,
        [this](ledger::Status status) {
          if (status != ledger::Status::OK) {
            FTL_LOG(ERROR) << "Failed to get snapshot for page";
            Done();
            return;
          }

          std::string key = MakeMessageQueueKey(token_);
          snapshot_->Get(
              to_array(key), [this](ledger::Status status, mx::vmo value) {
                if (status != ledger::Status::OK) {
                  if (status != ledger::Status::KEY_NOT_FOUND) {
                    // It's expected that the key is not found when the link
                    // is accessed for the first time. Don't log an error
                    // then.
                    FTL_LOG(ERROR) << "GetMessageSenderCall() " << token_
                                   << " PageSnapshot.Get() " << status;
                  }
                  Done();
                  return;
                }

                std::string value_as_string;
                if (value) {
                  if (!mtl::StringFromVmo(value, &value_as_string)) {
                    FTL_LOG(ERROR) << "Unable to extract data.";
                    Done();
                    return;
                  }
                }

                if (!XdrRead(value_as_string, &result_, XdrMessageQueueInfo)) {
                  Done();
                  return;
                }

                if (!result_.is_complete()) {
                  FTL_LOG(WARNING) << "Queue token " << result_.queue_token
                                   << " not found in the ledger.";
                  Done();
                  return;
                }

                message_queue_manager_->GetMessageQueueStorage(result_)
                    ->AddMessageSenderBinding(std::move(request_));

                Done();
              });
        });
  }

  MessageQueueManager* const message_queue_manager_;  // not owned
  ledger::Page* const page_;                          // not owned
  const std::string token_;
  fidl::InterfaceRequest<MessageSender> request_;

  ledger::PageSnapshotPtr snapshot_;
  std::string key_;

  MessageQueueInfo result_;

  FTL_DISALLOW_COPY_AND_ASSIGN(GetMessageSenderCall);
};

class MessageQueueManager::ObtainMessageQueueCall : Operation<void> {
 public:
  ObtainMessageQueueCall(OperationContainer* const container,
                         MessageQueueManager* const message_queue_manager,
                         ledger::Page* const page,
                         const std::string& component_namespace,
                         const std::string& component_instance_id,
                         const std::string& queue_name,
                         fidl::InterfaceRequest<MessageQueue> request)
      : Operation(container, [] {}),
        message_queue_manager_(message_queue_manager),
        page_(page),
        request_(std::move(request)) {
    message_queue_info_.component_namespace = component_namespace;
    message_queue_info_.component_instance_id = component_instance_id;
    message_queue_info_.queue_name = queue_name;
    Ready();
  }

 private:
  void Run() override {
    new GetQueueTokenCall(
        &operation_collection_, page_, message_queue_info_.component_namespace,
        message_queue_info_.component_instance_id,
        message_queue_info_.queue_name, [this](fidl::String token) {
          if (token) {
            message_queue_info_.queue_token = token.get();
            // Queue token was found in the ledger.
            Finish();
            return;
          }

          // Not found in the ledger, time to create a new message
          // queue.
          message_queue_info_.queue_token = GenerateQueueToken();

          std::string json;
          XdrWrite(&json, &message_queue_info_, XdrMessageQueueInfo);

          page_->StartTransaction([](ledger::Status status) {});

          std::string message_queue_token_key = MakeMessageQueueTokenKey(
              message_queue_info_.component_namespace,
              message_queue_info_.component_instance_id,
              message_queue_info_.queue_name);

          page_->Put(to_array(message_queue_token_key),
                     to_array(message_queue_info_.queue_token),
                     [](ledger::Status status) {});

          std::string message_queue_key =
              MakeMessageQueueKey(message_queue_info_.queue_token);

          page_->Put(to_array(message_queue_key), to_array(json),
                     [](ledger::Status status) {});

          page_->Commit([this](ledger::Status status) {
            if (status != ledger::Status::OK) {
              FTL_LOG(ERROR) << "Error creating queue in ledger: " << status;
              Done();
              return;
            }

            FTL_LOG(INFO) << "Created queue in ledger: "
                          << message_queue_info_.queue_token;

            Finish();
          });
        });
  }

  void Finish() {
    message_queue_manager_->GetMessageQueueStorage(message_queue_info_)
        ->AddMessageQueueBinding(std::move(request_));
    Done();
  }

  MessageQueueManager* const message_queue_manager_;  // not owned
  ledger::Page* const page_;                          // not owned
  fidl::InterfaceRequest<MessageQueue> request_;

  MessageQueueInfo message_queue_info_;
  ledger::PageSnapshotPtr snapshot_;

  OperationCollection operation_collection_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ObtainMessageQueueCall);
};

class MessageQueueManager::DeleteMessageQueueCall : Operation<void> {
 public:
  DeleteMessageQueueCall(OperationContainer* const container,
                         MessageQueueManager* const message_queue_manager,
                         ledger::Page* const page,
                         const std::string& component_namespace,
                         const std::string& component_instance_id,
                         const std::string& queue_name)
      : Operation(container, [] {}),
        message_queue_manager_(message_queue_manager),
        page_(page) {
    message_queue_info_.component_namespace = component_namespace;
    message_queue_info_.component_instance_id = component_instance_id;
    message_queue_info_.queue_name = queue_name;
    Ready();
  }

 private:
  void Run() override {
    new GetQueueTokenCall(
        &operation_collection_, page_, message_queue_info_.component_namespace,
        message_queue_info_.component_instance_id,
        message_queue_info_.queue_name, [this](fidl::String token) {
          if (!token) {
            FTL_LOG(WARNING)
                << "Request to delete queue " << message_queue_info_.queue_name
                << " for component instance "
                << message_queue_info_.component_instance_id
                << " that wasn't found in the ledger";
            Done();
            return;
          }

          message_queue_info_.queue_token = token.get();

          std::string message_queue_key =
              MakeMessageQueueKey(message_queue_info_.queue_token);

          std::string message_queue_token_key = MakeMessageQueueTokenKey(
              message_queue_info_.component_namespace,
              message_queue_info_.component_instance_id,
              message_queue_info_.queue_name);

          // Delete the ledger entries.
          page_->StartTransaction([](ledger::Status status) {});

          page_->Delete(to_array(message_queue_key),
                        [](ledger::Status status) {});
          page_->Delete(to_array(message_queue_token_key),
                        [](ledger::Status status) {});

          message_queue_manager_->ClearMessageQueueStorage(message_queue_info_);

          page_->Commit([this](ledger::Status status) {
            if (status == ledger::Status::OK) {
              FTL_LOG(INFO) << "Deleted queue from ledger: "
                            << message_queue_info_.component_instance_id << "/"
                            << message_queue_info_.queue_name;
            } else {
              FTL_LOG(ERROR) << "Error deleting queue from ledger: " << status;
            }
            Done();
          });
        });
  }

  MessageQueueManager* const message_queue_manager_;  // not owned
  ledger::Page* const page_;                          // not owned
  MessageQueueInfo message_queue_info_;
  ledger::PageSnapshotPtr snapshot_;

  OperationCollection operation_collection_;

  FTL_DISALLOW_COPY_AND_ASSIGN(DeleteMessageQueueCall);
};

MessageQueueManager::MessageQueueManager(ledger::PagePtr page,
                                         const std::string& local_path)
    : page_(std::move(page)), local_path_(local_path) {}

MessageQueueManager::~MessageQueueManager() = default;

void MessageQueueManager::ObtainMessageQueue(
    const std::string& component_namespace,
    const std::string& component_instance_id,
    const std::string& queue_name,
    fidl::InterfaceRequest<MessageQueue> request) {
  new ObtainMessageQueueCall(&operation_collection_, this, page_.get(),
                             component_namespace, component_instance_id,
                             queue_name, std::move(request));
}

template <typename ValueType>
const ValueType* MessageQueueManager::FindQueueName(
    const ComponentQueueNameMap<ValueType>& queue_map,
    const MessageQueueInfo& info) {
  auto it1 = queue_map.find(info.component_namespace);
  if (it1 != queue_map.end()) {
    auto it2 = it1->second.find(info.component_instance_id);
    if (it2 != it1->second.end()) {
      auto it3 = it2->second.find(info.queue_name);
      if (it3 != it2->second.end()) {
        return &(it3->second);
      }
    }
  }

  return nullptr;
}

template <typename ValueType>
void MessageQueueManager::EraseQueueName(
    ComponentQueueNameMap<ValueType>& queue_map,
    const MessageQueueInfo& info) {
  auto it1 = queue_map.find(info.component_namespace);
  if (it1 != queue_map.end()) {
    auto it2 = it1->second.find(info.component_instance_id);
    if (it2 != it1->second.end()) {
      it2->second.erase(info.queue_name);
    }
  }
}

MessageQueueStorage* MessageQueueManager::GetMessageQueueStorage(
    const MessageQueueInfo& info) {
  auto it = message_queues_.find(info.queue_token);
  if (it == message_queues_.end()) {
    // Not found, create one.
    bool inserted;
    std::string path = local_path_;
    path.push_back('/');
    path.append(info.queue_token);
    path.append(".json");
    auto new_queue = std::make_unique<MessageQueueStorage>(info.queue_token,
                                                           std::move(path));
    std::tie(it, inserted) = message_queues_.insert(
        std::make_pair(info.queue_token, std::move(new_queue)));
    FTL_DCHECK(inserted);

    message_queue_tokens_[info.component_namespace][info.component_instance_id]
                         [info.queue_name] = info.queue_token;

    const ftl::Closure* watcher =
        FindQueueName(pending_watcher_callbacks_, info);
    if (watcher) {
      it->second.get()->RegisterWatcher(*watcher);
      EraseQueueName(pending_watcher_callbacks_, info);
    }
  }
  return it->second.get();
}

void MessageQueueManager::ClearMessageQueueStorage(
    const MessageQueueInfo& info) {
  // Remove the |MessageQueueStorage| and delete it which in turn will
  // close all outstanding MessageSender and MessageQueue interface
  // connections, and delete all messages on the queue permanently.
  message_queues_.erase(info.queue_token);

  // Clear entries in message_queue_tokens_ and
  // pending_watcher_callbacks_.
  EraseQueueName(pending_watcher_callbacks_, info);
  EraseQueueName(message_queue_tokens_, info);
}

void MessageQueueManager::DeleteMessageQueue(
    const std::string& component_namespace,
    const std::string& component_instance_id,
    const std::string& queue_name) {
  new DeleteMessageQueueCall(&operation_collection_, this, page_.get(),
                             component_namespace, component_instance_id,
                             queue_name);
}

void MessageQueueManager::GetMessageSender(
    const std::string& queue_token,
    fidl::InterfaceRequest<MessageSender> request) {
  const auto& it = message_queues_.find(queue_token);
  if (it != message_queues_.cend()) {
    // Found the message queue already.
    it->second->AddMessageSenderBinding(std::move(request));
    return;
  }

  new GetMessageSenderCall(&operation_collection_, this, page_.get(),
                           queue_token, std::move(request));
}

void MessageQueueManager::RegisterWatcher(
    const std::string& component_namespace,
    const std::string& component_instance_id,
    const std::string& queue_name,
    const std::function<void()>& watcher) {
  const std::string* token =
      FindQueueName(message_queue_tokens_,
                    MessageQueueInfo{component_namespace, component_instance_id,
                                     queue_name, ""});
  if (token) {
    pending_watcher_callbacks_[component_namespace][component_instance_id]
                              [queue_name] = watcher;
    return;
  }

  auto msq_it = message_queues_.find(*token);
  FTL_DCHECK(msq_it != message_queues_.end());
  msq_it->second->RegisterWatcher(watcher);
}

void MessageQueueManager::DropWatcher(const std::string& component_namespace,
                                      const std::string& component_instance_id,
                                      const std::string& queue_name) {
  auto queue_info = MessageQueueInfo{component_namespace, component_instance_id,
                                     queue_name, ""};

  const std::string* token = FindQueueName(message_queue_tokens_, queue_info);
  if (token) {
    // The |MessageQueueStorage| doesn't exist yet.
    EraseQueueName(message_queue_tokens_, queue_info);
    return;
  }

  auto msq_it = message_queues_.find(*token);
  if (msq_it == message_queues_.end()) {
    FTL_LOG(WARNING) << "Asked to DropWatcher for a queue that doesn't exist";
    return;
  };
  msq_it->second->DropWatcher();
}

void MessageQueueManager::XdrMessageQueueInfo(XdrContext* const xdr,
                                              MessageQueueInfo* const data) {
  xdr->Field("component_namespace", &data->component_namespace);
  xdr->Field("component_instance_id", &data->component_instance_id);
  xdr->Field("queue_name", &data->queue_name);
  xdr->Field("queue_token", &data->queue_token);
}

}  // namespace modular
