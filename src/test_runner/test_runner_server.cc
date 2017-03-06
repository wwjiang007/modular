// Copyright 2017 The Fuchsia Authors. All rights reserved.
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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <string>
#include <vector>

#include "application/lib/app/application_context.h"
#include "apps/modular/lib/fidl/scope.h"
#include "apps/modular/services/test_runner/test_runner.fidl.h"
#include "apps/modular/src/test_runner/test_runner.h"
#include "apps/modular/src/test_runner/test_runner_store_impl.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/strings/split_string.h"
#include "lib/ftl/strings/string_view.h"
#include "lib/ftl/synchronization/sleep.h"
#include "lib/ftl/tasks/one_shot_timer.h"
#include "lib/mtl/tasks/message_loop.h"

// TODO(vardhan): Make listen port command-line configurable.
constexpr uint16_t kListenPort = 8342;  // TCP port

namespace modular {
namespace testing {

// Represents a client connection, and is self-owned (it will exit the
// MessageLoop upon completion). TestRunnerConnection receives commands to run
// tests and runs them one at a time using TestRunContext.
class TestRunnerConnection : public TestRunObserver {
 public:
  TestRunnerConnection(int socket_fd,
                       std::shared_ptr<app::ApplicationContext> app_context)
      : app_context_(app_context), socket_(socket_fd) {}

  void Start() {
    FTL_CHECK(!test_context_);
    ReadAndRunCommand();
  }

  void SendMessage(const std::string& test_id, const std::string& operation,
                   const std::string& msg) override {
    std::stringstream stream;
    stream << test_id << " " << operation << " " << msg << "\n";

    std::string bytes = stream.str();
    FTL_CHECK(write(socket_, bytes.data(), bytes.size()) > 0);
  }

  // Called by TestRunContext when it has finished running its test. This will
  // trigger reading more commands from TCP socket.
  void Teardown(const std::string& test_id, bool success) override {
    FTL_CHECK(test_context_);

    // IMPORTANT: leave this log here, exactly as it is. Currently, tests
    // launched from host (e.g. Linux) grep for this text to figure out the
    // amount of the log to associate with the test.
    FTL_LOG(INFO) << "test_runner: teardown " << test_id
                  << " success=" << success;

    SendMessage(test_id, "teardown", success ? "pass" : "fail");
    test_context_.reset();
    Start();
  }

 private:
  virtual ~TestRunnerConnection() {
    close(socket_);
    mtl::MessageLoop::GetCurrent()->PostQuitTask();
  }

  // Read and entire command (which consists of one line) and return it.
  // Can be called again to read the next command. Blocks until an entire line
  // has been read.
  std::string ReadCommand() {
    char buf[1024];
    // Read until we see a new line.
    auto newline_pos = command_buffer_.find("\n");
    while (newline_pos == std::string::npos) {
      ssize_t n = read(socket_, buf, sizeof(buf));
      if (n <= 0) {
        return std::string();
      }
      command_buffer_.append(buf, n);
      newline_pos = command_buffer_.find("\n");
    }

    // Consume only until the new line (and leave the rest of the bytes for
    // subsequent ReadCommand()s.
    auto retval = command_buffer_.substr(0, newline_pos + 1);
    command_buffer_.erase(0, newline_pos + 1);
    return retval;
  }

  // Read an entire line representing the command to run and run it. When the
  // test has finished running, TestRunnerConnection::Teardown is invoked. We do
  // not read any further commands until that has happened.
  void ReadAndRunCommand() {
    std::string command = ReadCommand();
    if (command.empty()) {
      delete this;
      return;
    }

    // command_parse[0] = "run"
    // command_parse[1] = test_id
    // command_parse[2] = url
    // command_parse[3..] = args (optional)
    std::vector<std::string> command_parse = ftl::SplitStringCopy(
        command, " ", ftl::kTrimWhitespace, ftl::kSplitWantNonEmpty);

    FTL_CHECK(command_parse.size() >= 3)
        << "Not enough args. Must be: `run <test id> <command to run>`";
    FTL_CHECK(command_parse[0] == "run")
        << command_parse[0] << " is not a supported command.";

    FTL_LOG(INFO) << "test_runner: run " << command_parse[1];

    std::vector<std::string> args;
    for (size_t i = 3; i < command_parse.size(); i++) {
      args.push_back(std::move(command_parse[i]));
    }

    // When TestRunContext is done with the test, it calls
    // TestRunnerConnection::Teardown().
    test_context_.reset(new TestRunContext(app_context_, this, command_parse[1],
                                           command_parse[2], args));
  }

  std::shared_ptr<app::ApplicationContext> app_context_;
  std::unique_ptr<TestRunContext> test_context_;

  // Posix fd for the TCP connection.
  const int socket_;
  std::string command_buffer_;

  FTL_DISALLOW_COPY_AND_ASSIGN(TestRunnerConnection);
};

// TestRunnerTCPServer is a TCP server that accepts connections and launches
// them as TestRunnerConnection.
class TestRunnerTCPServer {
 public:
  TestRunnerTCPServer(uint16_t port)
      : app_context_(app::ApplicationContext::CreateFromStartupInfo()) {
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 1. Make a TCP socket.
    // We need to retry because there's a race condition at boot
    // between netstack initializing and us calling socket().
    const auto duration = ftl::TimeDelta::FromMilliseconds(200u);

    for (int i = 0; i < 5 * 10; ++i) {
      listener_ = socket(addr.sin_family, SOCK_STREAM, IPPROTO_TCP);
      if (listener_ != -1) {
        break;
      }
      ftl::SleepFor(duration);
    }
    FTL_CHECK(listener_ != -1);

    // 2. Bind it to an address.
    FTL_CHECK(bind(listener_, reinterpret_cast<struct sockaddr*>(&addr),
                   sizeof(addr)) != -1);

    // 3. Make it a listening socket.
    FTL_CHECK(listen(listener_, 100) != -1);
  }

  ~TestRunnerTCPServer() { close(listener_); }

  // Blocks until there is a new connection.
  TestRunnerConnection* AcceptConnection() {
    int sockfd = accept(listener_, nullptr, nullptr);
    if (sockfd == -1) {
      FTL_LOG(INFO) << "accept() oops";
    }
    return new TestRunnerConnection(sockfd, app_context_);
  }

 private:
  int listener_;
  std::shared_ptr<app::ApplicationContext> app_context_;

  FTL_DISALLOW_COPY_AND_ASSIGN(TestRunnerTCPServer);
};

}  // namespace testing
}  // namespace modular

int main() {
  mtl::MessageLoop loop;
  modular::testing::TestRunnerTCPServer server(kListenPort);
  while (1) {
    // TODO(vardhan): Because our sockets are POSIX fds, they don't work with
    // our message loop, so we do some synchronous operations and have to do
    // manipulate the message loop to pass control back and forth. Consider
    // using separate threads for handle message loop vs. fd polling.
    auto* runner = server.AcceptConnection();
    loop.task_runner()->PostTask(
        std::bind(&modular::testing::TestRunnerConnection::Start, runner));
    loop.Run();
  }
  return 0;
}
