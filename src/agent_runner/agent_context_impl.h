// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_AGENT_RUNNER_AGENT_CONTEXT_IMPL_H_
#define APPS_MODULAR_SRC_AGENT_RUNNER_AGENT_CONTEXT_IMPL_H_

#include <string>

#include "application/services/application_controller.fidl.h"
#include "application/services/application_launcher.fidl.h"
#include "application/services/service_provider.fidl.h"
#include "apps/maxwell/services/user/intelligence_services.fidl.h"
#include "apps/maxwell/services/user/user_intelligence_provider.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/services/agent/agent.fidl.h"
#include "apps/modular/services/agent/agent_context.fidl.h"
#include "apps/modular/services/agent/agent_controller/agent_controller.fidl.h"
#include "apps/modular/services/component/component_context.fidl.h"
#include "apps/modular/src/component/component_context_impl.h"
#include "apps/modular/src/component/message_queue_manager.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/ftl/macros.h"
#include "lib/ftl/tasks/one_shot_timer.h"

namespace modular {

class AgentRunner;

// The parameters of agent context that do not vary by instance.
struct AgentContextInfo {
  const ComponentContextInfo component_context_info;
  app::ApplicationLauncher* const app_launcher;
  maxwell::UserIntelligenceProvider* const user_intelligence_provider;
};

// This class manages an agent and its life cycle. AgentRunner owns this class,
// and instantiates one for every instance of an agent running. All requests for
// this agent (identified for now by the agent's URL) are routed to this
// class. This class manages all AgentControllers associated with this agent.
class AgentContextImpl : AgentContext, AgentController {
 public:
  explicit AgentContextImpl(const AgentContextInfo& info,
                            const std::string& url);
  ~AgentContextImpl() override;

  // Stops the running agent, irrespective of whether there are active
  // AgentControllers or outstanding tasks.
  void StopForTeardown(const std::function<void()>& callback);

  // Called by AgentRunner when a component wants to connect to this agent.
  // Connections will pend until Agent::Initialize() responds back, at which
  // point all connections will be forwarded to the agent.
  void NewConnection(
      const std::string& requestor_url,
      fidl::InterfaceRequest<app::ServiceProvider> incoming_services_request,
      fidl::InterfaceRequest<AgentController> agent_controller_request);

  // Called by AgentRunner when a new task has been scheduled.
  void NewTask(const std::string& task_id);

 private:
  // |AgentContext|
  void GetComponentContext(
      fidl::InterfaceRequest<ComponentContext> request) override;
  // |AgentContext|
  void ScheduleTask(TaskInfoPtr task_info) override;
  // |AgentContext|
  void DeleteTask(const fidl::String& task_id) override;
  // |AgentContext|
  void Done() override;
  // |AgentContext|
  void GetIntelligenceServices(
      fidl::InterfaceRequest<maxwell::IntelligenceServices> request) override;

  // Adds an operation on |operation_queue_|. This operation is immediately
  // Done() if this agent is |ready_|. Else, we first setup agent connection and
  // wait for Agent.Initialize() to complete.
  void MaybeInitializeAgent();

  // Adds an operation on |operation_queue_|. This operation is immediately
  // Done() if this agent is not |ready_|. Else if there are no active
  // AgentControllers and no outstanding task, Agent.Stop() is called with a
  // timeout.
  void MaybeStopAgent();

  const std::string url_;
  app::ApplicationLauncher* const application_launcher_;

  app::ApplicationControllerPtr application_controller_;
  app::ServiceProviderPtr application_services_;
  AgentPtr agent_;
  fidl::Binding<AgentContext> agent_context_binding_;
  fidl::BindingSet<AgentController> agent_controller_bindings_;

  AgentRunner* const agent_runner_;

  ComponentContextImpl component_context_impl_;
  fidl::BindingSet<ComponentContext> component_context_bindings_;

  maxwell::UserIntelligenceProvider* const
      user_intelligence_provider_;  // Not owned.

  // |ready_| is true once Initialize() responds.
  bool ready_{};

  // Number of times Agent.RunTask() was called but we're still waiting on its
  // completion callback.
  int incomplete_task_count_ = 0;

  OperationQueue operation_queue_;

  FTL_DISALLOW_COPY_AND_ASSIGN(AgentContextImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_AGENT_RUNNER_AGENT_CONTEXT_IMPL_H_
