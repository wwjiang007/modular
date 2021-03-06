// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_COMPONENT_COMPONENT_CONTEXT_IMPL_H_
#define APPS_MODULAR_SRC_COMPONENT_COMPONENT_CONTEXT_IMPL_H_

#include <string>

#include "apps/ledger/services/internal/internal.fidl.h"
#include "apps/modular/services/component/component_context.fidl.h"
#include "apps/modular/src/component/message_queue_manager.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/fidl/cpp/bindings/string.h"
#include "lib/ftl/macros.h"

namespace modular {

class AgentRunner;

// The parameters of component context that do not vary by instance.
struct ComponentContextInfo {
  MessageQueueManager* const message_queue_manager;
  AgentRunner* const agent_runner;
  ledger::LedgerRepository* const ledger_repository;
};

// Implements the ComponentContext interface, which is provided to
// modules and agents. The interface is public, because the class
// doesn't contain the Bindings for this interface. TODO(mesch): Move
// bindings into the class.
class ComponentContextImpl : public ComponentContext {
 public:
  // * A component namespace identifies components whose lifetimes are related,
  //   where all of their persisted information will live together; for modules
  //   this is the story id, for agents it is kAgentComponentNamespace, etc.
  // * A component instance ID identifies a particular instance of a component;
  //   for modules, this is the module path in their story. For agents, it is
  //   the agent URL.
  explicit ComponentContextImpl(const ComponentContextInfo& info,
                                const std::string& component_namespace,
                                const std::string& component_instance_id);

  ~ComponentContextImpl() override;

  const std::string& component_instance_id() { return component_instance_id_; }

 private:
  // |ComponentContext|
  void GetLedger(fidl::InterfaceRequest<ledger::Ledger> request,
                 const GetLedgerCallback& result) override;

  // |ComponentContext|
  void ConnectToAgent(
      const fidl::String& url,
      fidl::InterfaceRequest<app::ServiceProvider> incoming_services_request,
      fidl::InterfaceRequest<AgentController> agent_controller_request)
      override;

  // |ComponentContext|
  void ObtainMessageQueue(
      const fidl::String& name,
      fidl::InterfaceRequest<MessageQueue> request) override;

  // |ComponentContext|
  void DeleteMessageQueue(const fidl::String& name) override;

  // |ComponentContext|
  void GetMessageSender(const fidl::String& queue_token,
                        fidl::InterfaceRequest<MessageSender> request) override;

  MessageQueueManager* const message_queue_manager_;
  AgentRunner* const agent_runner_;
  ledger::LedgerRepository* const ledger_repository_;

  const std::string component_namespace_;
  const std::string component_instance_id_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ComponentContextImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_COMPONENT_COMPONENT_CONTEXT_IMPL_H_
