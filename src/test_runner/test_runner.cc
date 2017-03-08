// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a TCP service and a fidl service. The TCP portion of this process
// accepts test commands, runs them, waits for completion or error, and reports
// back to the TCP client.
// The TCP protocol is as follows:
// - Client connects, sends a single line representing the test command to run:
//   run <test_id> <shell command to run>\n
// - To send a log message, we send to the TCP client:
//   <test_id> log <msg>
// - Once the test is done, we reply to the TCP client:
//   <test_id> teardown pass|fail\n
//
// The <test_id> is an unique ID string that the TCP client gives us per test;
// we tag our replies and device logs with it so the TCP client can identify
// device logs (and possibly if multiple tests are run at the same time).
//
// The shell command representing the running test is launched in a new
// ApplicationEnvironment for easy teardown. This ApplicationEnvironment
// contains a TestRunner service (see test_runner.fidl). The applications
// launched by the shell command (which may launch more than 1 process) may use
// the |TestRunner| service to signal completion of the test, and also provides
// a way to signal process crashes.

// TODO(vardhan): Make it possible to run multiple tests within the same test
// runner environment, without teardown; useful for testing modules, which may
// not need to tear down device_runner.

#include "apps/modular/src/test_runner/test_runner.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <vector>

#include "lib/ftl/logging.h"
#include "lib/ftl/strings/split_string.h"
#include "lib/ftl/strings/string_view.h"
#include "lib/ftl/synchronization/sleep.h"
#include "lib/mtl/tasks/message_loop.h"

namespace modular {
namespace testing {

TestRunnerImpl::TestRunnerImpl(
    fidl::InterfaceRequest<testing::TestRunner> request,
    TestRunContext* test_run_context)
    : binding_(this, std::move(request)), test_run_context_(test_run_context) {
  binding_.set_connection_error_handler([this] {
    if (waiting_for_termination_) {
      FTL_LOG(INFO) << "Test " << test_name_ << " terminated as expected.";
      // Client terminated but that was expected.
      termination_timer_.Stop();
      if (teardown_after_termination_) {
        Teardown();
      } else {
        test_run_context_->StopTrackingClient(this, false);
      }
    } else {
      test_run_context_->StopTrackingClient(this, true);
    }
  });
}

const std::string& TestRunnerImpl::test_name() const {
  return test_name_;
}

bool TestRunnerImpl::waiting_for_termination() const {
  return waiting_for_termination_;
}

void TestRunnerImpl::TeardownAfterTermination() {
  teardown_after_termination_ = true;
}

void TestRunnerImpl::Identify(const fidl::String& test_name) {
  test_name_ = test_name;
}

void TestRunnerImpl::Fail(const fidl::String& log_message) {
  test_run_context_->Fail(log_message);
}

void TestRunnerImpl::Done() {
  test_run_context_->StopTrackingClient(this, false);
}

void TestRunnerImpl::Teardown() {
  test_run_context_->Teardown(this);
}

void TestRunnerImpl::WillTerminate(const double withinSeconds) {
  if (waiting_for_termination_) {
    Fail(test_name_ + " called WillTerminate more than once.");
    return;
  }
  waiting_for_termination_ = true;
  termination_timer_.Start(mtl::MessageLoop::GetCurrent()->task_runner().get(),
                           [this, withinSeconds] {
                             FTL_LOG(ERROR) << test_name_
                                            << " termination timed out after "
                                            << withinSeconds << "s.";
                             binding_.set_connection_error_handler(nullptr);
                             Fail("Termination timed out.");
                             if (teardown_after_termination_) {
                               Teardown();
                             }
                             test_run_context_->StopTrackingClient(this, false);
                           },
                           ftl::TimeDelta::FromSecondsF(withinSeconds));
}

TestRunContext::TestRunContext(
    std::shared_ptr<app::ApplicationContext> app_context,
    TestRunObserver* connection,
    const std::string& test_id,
    const std::string& url,
    const std::vector<std::string>& args)
    : test_runner_connection_(connection), test_id_(test_id), success_(true) {
  // 1. Make a child environment to run the command.
  app::ApplicationEnvironmentPtr parent_env;
  app_context->environment()->Duplicate(parent_env.NewRequest());
  child_env_scope_ =
      std::make_unique<Scope>(std::move(parent_env), "test_runner_env");

  // 1.1 Setup child environment services
  child_env_scope_->AddService<testing::TestRunner>(
      [this](fidl::InterfaceRequest<testing::TestRunner> request) {
        test_runner_clients_.push_back(
            std::make_unique<TestRunnerImpl>(std::move(request), this));
      });
  child_env_scope_->AddService<testing::TestRunnerStore>(
      [this](fidl::InterfaceRequest<testing::TestRunnerStore> request) {
        test_runner_store_.AddBinding(std::move(request));
      });

  // 2. Launch the test command.
  app::ApplicationLauncherPtr launcher;
  child_env_scope_->environment()->GetApplicationLauncher(
      launcher.NewRequest());

  auto info = app::ApplicationLaunchInfo::New();
  info->url = url;
  info->arguments = fidl::Array<fidl::String>::From(args);
  launcher->CreateApplication(std::move(info),
                              child_app_controller_.NewRequest());

  // If the child app closes, the test is reported as a failure.
  child_app_controller_.set_connection_error_handler([this] {
    FTL_LOG(WARNING) << "Child app connection closed unexpectedly.";
    test_runner_connection_->Teardown(test_id_, false);
  });
}

void TestRunContext::Fail(const fidl::String& log_msg) {
  success_ = false;
  std::string msg("FAIL: ");
  msg += log_msg;
  test_runner_connection_->SendMessage(test_id_, "log", msg);
}

void TestRunContext::StopTrackingClient(TestRunnerImpl* client, bool crashed) {
  if (crashed) {
    FTL_LOG(WARNING) << client->test_name()
                     << " finished without calling modular::testing::Done().";
    test_runner_connection_->Teardown(test_id_, false);
    return;
  }

  auto find_it =
      std::find_if(test_runner_clients_.begin(), test_runner_clients_.end(),
                   [client](const std::unique_ptr<TestRunnerImpl>& client_it) {
                     return client_it.get() == client;
                   });

  FTL_DCHECK(find_it != test_runner_clients_.end());
  test_runner_clients_.erase(find_it);
}

void TestRunContext::Teardown(TestRunnerImpl* teardown_client) {
  bool waiting_for_termination = false;
  for (auto& client : test_runner_clients_) {
    if (teardown_client == client.get()) {
      continue;
    }
    if (client->waiting_for_termination()) {
      client->TeardownAfterTermination();
      FTL_LOG(INFO) << "Teardown blocked by test waiting for termination: "
                    << client->test_name();
      waiting_for_termination = true;
      continue;
    }
    FTL_LOG(ERROR) << "Test " << client->test_name()
                   << " not done before Teardown().";
    success_ = false;
  }
  if (waiting_for_termination) {
    StopTrackingClient(teardown_client, false);
  } else {
    test_runner_connection_->Teardown(test_id_, success_);
  }
}

}  // namespace testing
}  // namespace modular
