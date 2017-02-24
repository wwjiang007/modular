// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "application/lib/app/connect.h"
#include "apps/modular/lib/fidl/single_service_app.h"
#include "apps/modular/lib/testing/reporting.h"
#include "apps/modular/lib/testing/testing.h"
#include "apps/modular/services/component/component_context.fidl.h"
#include "apps/modular/services/story/module.fidl.h"
#include "apps/modular/tests/component_context/test_agent1_interface.fidl.h"
#include "lib/mtl/tasks/message_loop.h"

using modular::testing::TestPoint;

namespace {

// This is how long we wait for the test to finish before we timeout and tear
// down our test.
constexpr ftl::TimeDelta kTimeout = ftl::TimeDelta::FromSeconds(15);
// This is how long we wait before declare the story to be Done.
constexpr ftl::TimeDelta kStoryDoneDelay = ftl::TimeDelta::FromSeconds(7);

constexpr char kTest1Agent[] =
    "file:///system/apps/modular_tests/component_context_test_agent1";

constexpr char kUnstoppableAgent[] =
    "file:///system/apps/modular_tests/component_context_unstoppable_agent";

class ParentApp : public modular::SingleServiceApp<modular::Module> {
 public:
  ParentApp() { modular::testing::Init(application_context(), __FILE__); }

  ~ParentApp() override { mtl::MessageLoop::GetCurrent()->PostQuitTask(); }

 private:
  // |Module|
  void Initialize(
      fidl::InterfaceHandle<modular::Story> story,
      fidl::InterfaceHandle<modular::Link> link,
      fidl::InterfaceHandle<app::ServiceProvider> incoming_services,
      fidl::InterfaceRequest<app::ServiceProvider> outgoing_services) override {
    story_.Bind(std::move(story));
    link_.Bind(std::move(link));
    initialized_.Pass();

    // Exercise ComponentContext.ConnectToAgent()
    story_->GetComponentContext(component_context_.NewRequest());

    app::ServiceProviderPtr agent1_services;
    component_context_->ConnectToAgent(kTest1Agent, agent1_services.NewRequest(),
                                       agent1_controller.NewRequest());
    ConnectToService(agent1_services.get(), agent1_interface_.NewRequest());

    modular::testing::GetStore()->Get(
        "test_agent1_connected", [this](const fidl::String&) {
          agent_connected_.Pass();
          TestMessageQueue([this] {
            TestAgentController([this] {
              mtl::MessageLoop::GetCurrent()->task_runner()->PostDelayedTask(
                  [this] { story_->Done(); }, kStoryDoneDelay);
            });
          });
        });

    // Start an agent that will not stop of its own accord.
    app::ServiceProviderPtr unstoppable_agent_services;
    component_context_->ConnectToAgent(
        kUnstoppableAgent, unstoppable_agent_services.NewRequest(),
        unstoppable_agent_controller_.NewRequest());

    // After 500ms close the AgentController for the unstoppable agent.
    mtl::MessageLoop::GetCurrent()->task_runner()->PostDelayedTask(
        [this] { unstoppable_agent_controller_.reset(); },
        ftl::TimeDelta::FromMilliseconds(500));

    // Start a timer to call Story.Done in case the test agent misbehaves and we
    // time out.
    mtl::MessageLoop::GetCurrent()->task_runner()->PostDelayedTask(
        [this] { delete this; }, kTimeout);
  }

  // Tests message queues. Calls |done_cb| when completed successfully.
  void TestMessageQueue(std::function<void()> done_cb) {
    constexpr char kTestMessage[] = "test message!";

    component_context_->ObtainMessageQueue("root_msg_queue",
                                           msg_queue_.NewRequest());

    // This should queue the receive callback in the MessageQueueManager, since
    // no one sent anything to it yet.
    msg_queue_->Receive([this, done_cb, kTestMessage](const fidl::String& msg) {
      if (msg == kTestMessage)
        msg_queue_communicated_.Pass();
      done_cb();
    });

    msg_queue_->GetToken([this, kTestMessage](const fidl::String& token) {
      agent1_interface_->SendToMessageQueue(token, kTestMessage);
    });
  }

  // Tests AgentController. Calls |done_cb| when completed successfully.
  void TestAgentController(std::function<void()> done_cb) {
    // Closing the agent controller should trigger the agent to stop.
    agent1_controller.reset();

    modular::testing::GetStore()->Get("test_agent1_stopped",
                                      [this, done_cb](const fidl::String&) {
                                        agent_stopped_.Pass();
                                        done_cb();
                                      });
  }

  // |Module|
  void Stop(const StopCallback& done) override {
    stopped_.Pass();
    modular::testing::Teardown();
    done();
    delete this;
  }

  modular::StoryPtr story_;
  modular::LinkPtr link_;
  modular::AgentControllerPtr agent1_controller;
  modular::testing::Agent1InterfacePtr agent1_interface_;
  modular::ComponentContextPtr component_context_;
  modular::MessageQueuePtr msg_queue_;

  modular::AgentControllerPtr unstoppable_agent_controller_;

  TestPoint initialized_{"Root module initialized"};
  TestPoint stopped_{"Root module stopped"};
  TestPoint agent_connected_{"Agent1 accepted connection"};
  TestPoint agent_stopped_{"Agent1 stopped"};
  TestPoint msg_queue_communicated_{
      "Communicated message between Agent1 using a MessageQueue"};
};

}  // namespace

int main(int argc, const char** argv) {
  mtl::MessageLoop loop;
  new ParentApp();
  loop.Run();
  return 0;
}
