// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/component/persistent_queue.h"

#include "apps/modular/lib/rapidjson/rapidjson.h"
#include "lib/ftl/files/file.h"
#include "lib/ftl/logging.h"

namespace modular {

PersistentQueue::PersistentQueue(const std::string& file_name)
    : file_name_(file_name) {
  std::string contents;
  if (files::ReadFileToString(file_name_, &contents)) {
    rapidjson::Document document;
    document.Parse(contents);
    if (!document.IsArray()) {
      FTL_LOG(ERROR) << "Expected " << file_name_ << " to contain a JSON array";
      return;
    }
    for (rapidjson::Value::ConstValueIterator it = document.Begin();
         it != document.End(); ++it) {
      if (!it->IsString()) {
        FTL_LOG(ERROR) << "Expected a string but got: " << it;
        continue;
      }
      queue_.push_back(std::string(it->GetString(), it->GetStringLength()));
    }
  }
}

void PersistentQueue::Save() {
  rapidjson::Document document;
  document.SetArray();
  for (const auto& it : queue_) {
    rapidjson::Value value;
    value.SetString(it.data(), it.size());
    document.PushBack(value, document.GetAllocator());
  }
  std::string contents = JsonValueToString(document);
  if (!files::WriteFile(file_name_, contents.data(), contents.size())) {
    FTL_LOG(ERROR) << "Failed to write to: " << file_name_;
  }
}

}  // namespace modular
