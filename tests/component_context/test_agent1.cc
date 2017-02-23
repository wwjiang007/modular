// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/lib/fidl/single_service_app.h"
#include "apps/modular/lib/testing/reporting.h"
#include "apps/modular/lib/testing/testing.h"
#include "apps/modular/services/agent/agent.fidl.h"
#include "apps/modular/tests/component_context/test_agent1_interface.fidl.h"
#include "lib/ftl/logging.h"
#include "lib/mtl/tasks/message_loop.h"

using modular::testing::TestPoint;

namespace {

constexpr char kTest2Agent[] =
    "file:///system/apps/modular_tests/component_context_test_agent2";

class TestAgentApp : public modular::SingleServiceApp<modular::Agent>,
                     modular::testing::Agent1Interface {
 public:
  TestAgentApp() { modular::testing::Init(application_context(), __FILE__); }

  ~TestAgentApp() override { mtl::MessageLoop::GetCurrent()->PostQuitTask(); }

 private:
  // |Agent|
  void Initialize(
      fidl::InterfaceHandle<modular::AgentContext> agent_context) override {
    agent_context_.Bind(std::move(agent_context));
    agent_context_->GetComponentContext(component_context_.NewRequest());
    agent1_services_.AddService<modular::testing::Agent1Interface>(
        [this](fidl::InterfaceRequest<modular::testing::Agent1Interface>
                   interface_request) {
          agent1_interface_.AddBinding(this, std::move(interface_request));
        });

    // Connecting to the agent should start it up.
    app::ServiceProviderPtr agent_services;
    component_context_->ConnectToAgent(kTest2Agent, agent_services.NewRequest(),
                        agent2_controller_.NewRequest());

    // Killing the agent controller should stop it.
    agent2_controller_.reset();
  }

  // |Agent|
  void Connect(const fidl::String& requestor_url,
               fidl::InterfaceRequest<app::ServiceProvider> services) override {
    agent1_services_.AddBinding(std::move(services));
    modular::testing::GetStore()->Put("test_agent1_connected", "", [] {});
  }

  // |Agent|
  void RunTask(const fidl::String& task_id,
               const RunTaskCallback& callback) override {}

  // |Agent|
  void Stop(const StopCallback& callback) override {
    // Before reporting that we stop, we wait until agent2 has connected.
    modular::testing::GetStore()->Get(
        "test_agent2_connected", [this, callback](const fidl::String&) {
          agent2_connected_.Pass();
          modular::testing::GetStore()->Put("test_agent1_stopped", "", [] {});

          TEST_PASS("Test agent1 exited");
          modular::testing::Done();
          callback();
          delete this;
        });
  }

  // |Agent1Interface|
  void SendToMessageQueue(const fidl::String& message_queue_token,
                          const fidl::String& message_to_send) override {
    modular::MessageSenderPtr message_sender;
    component_context_->GetMessageSender(message_queue_token,
                                        message_sender.NewRequest());

    message_sender->Send(message_to_send);
  }

  TestPoint agent2_connected_{"Test agent2 accepted connection"};

  modular::AgentContextPtr agent_context_;
  modular::ComponentContextPtr component_context_;
  modular::AgentControllerPtr agent2_controller_;

  modular::ServiceProviderImpl agent1_services_;
  fidl::BindingSet<modular::testing::Agent1Interface> agent1_interface_;
};

}  // namespace

int main(int argc, const char** argv) {
  mtl::MessageLoop loop;
  new TestAgentApp();
  loop.Run();
  return 0;
}