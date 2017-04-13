// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_COMPONENT_MESSAGE_QUEUE_MANAGER_H_
#define APPS_MODULAR_SRC_COMPONENT_MESSAGE_QUEUE_MANAGER_H_

#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>

#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/services/component/message_queue.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/fidl/cpp/bindings/string.h"
#include "lib/ftl/macros.h"

namespace modular {

class MessageQueueStorage;
class XdrContext;

// Manages message queues for components. One MessageQueueManager
// instance is used by all ComponentContextImpl instances, and manages
// the message queues for all component instances. The
// ComponentContext instance is responsible for deleting the message
// queues it has created, otherwise they are persisted.
class MessageQueueManager {
 public:
  MessageQueueManager(ledger::PagePtr page);
  ~MessageQueueManager();

  void ObtainMessageQueue(const std::string& component_namespace,
                          const std::string& component_instance_id,
                          const std::string& queue_name,
                          fidl::InterfaceRequest<MessageQueue> request);

  void DeleteMessageQueue(const std::string& component_namespace,
                          const std::string& component_instance_id,
                          const std::string& queue_name);

  void GetMessageSender(const std::string& queue_token,
                        fidl::InterfaceRequest<MessageSender> request);

  // Used by AgentRunner to look for new messages on queues which have
  // a trigger associated with it. If a queue corresponding to
  // |component_namespace| x |component_instance_id| x |queue_name| does not
  // exist, a new one is created.
  //
  // Registering a new watcher stomps over any existing watcher.
  void RegisterWatcher(const std::string& component_namespace,
                       const std::string& component_instance_id,
                       const std::string& queue_name,
                       const std::function<void()>& watcher);
  void DropWatcher(const std::string& component_namespace,
                   const std::string& component_instance_id,
                   const std::string& queue_name);

 private:
  struct MessageQueueInfo;
  using ComponentNamespace = std::string;
  using ComponentInstanceId = std::string;
  using ComponentQueueName = std::string;
  template <typename Value>
  using ComponentQueueNameMap = std::unordered_map<
      ComponentNamespace,
      std::unordered_map<ComponentInstanceId,
                         std::unordered_map<ComponentQueueName, Value>>>;

  static void XdrMessageQueueInfo(XdrContext* const xdr,
                                  MessageQueueInfo* const data);

  // Returns the |MessageQueueStorage| for the queue_token. Creates it
  // if it doesn't exist yet.
  MessageQueueStorage* GetMessageQueueStorage(const MessageQueueInfo& info);

  // Clears the |MessageQueueStorage| for the queue_token.
  void ClearMessageQueueStorage(const MessageQueueInfo& info);

  // |FindQueueName()| and |EraseQueueName()| are helpers used to operate on
  // component (namespace, id, queue name) mappings.
  // If the given message queue |info| is found the stored pointer value, or
  // nullptr otherwise.
  template <typename ValueType>
  const ValueType* FindQueueName(
      const ComponentQueueNameMap<ValueType>& queue_map,
      const MessageQueueInfo& info);

  template <typename ValueType>
  void EraseQueueName(ComponentQueueNameMap<ValueType>& queue_map,
                      const MessageQueueInfo& info);

  ledger::PagePtr page_;

  // A map of queue_token to |MessageStorageQueue|.
  std::unordered_map<std::string, std::unique_ptr<MessageQueueStorage>>
      message_queues_;

  // A map of component instance id and queue name to queue tokens.
  // Entries are only here while a |MessageQueueStorage| exists.
  ComponentQueueNameMap<std::string> message_queue_tokens_;

  // A map of component instance id and queue name to watcher
  // callbacks. If a watcher is registered before a
  // |MessageQueueStorage| exists then it is stashed here until a
  // |MessageQueueStorage| is available.
  ComponentQueueNameMap<ftl::Closure> pending_watcher_callbacks_;

  OperationCollection operation_collection_;

  // Operations implemented here.
  class GetQueueTokenCall;
  class GetMessageSenderCall;
  class ObtainMessageQueueCall;
  class DeleteMessageQueueCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(MessageQueueManager);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_COMPONENT_MESSAGE_QUEUE_MANAGER_H_
