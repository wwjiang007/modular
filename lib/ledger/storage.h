// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is a description of the pages and keys used by the modular runtime.

#ifndef APPS_MODULAR_LIB_STORAGE_LEDGER_H_
#define APPS_MODULAR_LIB_STORAGE_LEDGER_H_

#include <string>

#include "lib/fidl/cpp/bindings/array.h"
#include "lib/fidl/cpp/bindings/string.h"

namespace modular {

// There are four kinds of pages used by the modular runtime:
//
// 1. The user root page contains a list of all stories and of all devices.
//

// 2. The message queue page lists all message queues that exist.
//
constexpr char kMessageQueuePageId[] = "MessageQueuePage";  // 16 chars

// 3. The trigger page contains the trigger conditions for all agents.
//
constexpr char kAgentRunnerPageId[] = "AgentRunnerPage_";  // 16 chars

// 4. Story pages (one per story) contain the story state expressed as link and
//    module data.

// Keys in these pages are constructed as follows:
//
// 1. A prefix indicates the kind of information stored under the key. The
//    prefix ends in a slash. The prefix is used to construct keys for reading
//    and writing, and to filter keys for bulk reading and in change
//    notifications.
//
//    Root page:
constexpr char kStoryKeyPrefix[] = "Story/";
constexpr char kDeviceKeyPrefix[] = "Device/";
constexpr char kFocusKeyPrefix[] = "Focus/";
//    Message Queue page:
constexpr char kMessageQueueKeyPrefix[] = "MessageQueue/";
constexpr char kMessageQueueTokenKeyPrefix[] = "MessageQueueToken/";
//    Agent Trigger page:
constexpr char kTriggerKeyPrefix[] = "Trigger/";
//    Story page:
constexpr char kLinkKeyPrefix[] = "Link/";
constexpr char kModuleKeyPrefix[] = "Module/";

// 2. ID values, separated by slashes, to identify the data item under this
//    key. The set of ID values under each key is defined by the arguments of
//    factory functions for the keys:
//
std::string MakeStoryKey(const fidl::String& story_id);
std::string MakeDeviceKey(const fidl::String& device_name);
std::string MakeFocusKey(const fidl::String& device_name);
std::string MakeMessageQueueTokenKey(const std::string& component_namespace,
                                     const std::string& component_instance_id,
                                     const std::string& queue_name);
std::string MakeMessageQueueKey(const std::string& queue_token);
std::string MakeTriggerKey(const std::string& agent_url,
                           const std::string& task_id);
std::string MakeLinkKey(const fidl::Array<fidl::String>& module_path,
                        const fidl::String& link_name);
std::string MakeModuleKey(const fidl::String& module_name);

// 3. The slash separator is escaped by a backslash inside the ID
//    values. Backslashes inside the ID values are escaped by backslash too.
//
constexpr char kSeparator[] = "/";
constexpr char kEscaper = '\\';
constexpr char kCharsToEscape[] = ":/";

// 4. The ID values may have internal structure on their own too, expressed by a
//    second sub separator character.
//
constexpr char kSubSeparator[] = ":";
std::string EncodeModulePath(const fidl::Array<fidl::String>& path);
std::string EncodeModuleComponentNamespace(const std::string& story_id);

// More notes:
//
// * Although keys can be parsed, the information encoded in the keys is usually
//   repeated in the value, and thus can be obtained without parsing the
//   key. This is the preferred way, as it leaves the possibility open to
//   replace key components with hashes.
//
// * The values under all keys are JSON. The structure of the JSON is defined by
//   Xdr*() functions to be found in the page access code.

}  // namespace modular

#endif  // APPS_MODULAR_LIB_STORAGE_LEDGER_H_
