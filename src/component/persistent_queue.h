// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_COMPONENT_PERSISTENT_QUEUE_H_
#define APPS_MODULAR_SRC_COMPONENT_PERSISTENT_QUEUE_H_

#include <deque>
#include <string>

namespace modular {

/* Implements a FIFO queue of strings that is persisted to local storage as
 * JSON. It is not safe to use from multiple processes or threads. If writing
 * the queue JSON to disk fails an error will be logged but calls will not fail.
 */
class PersistentQueue {
 public:
  PersistentQueue(const std::string& file_name);
  bool IsEmpty() const { return queue_.empty(); }

  std::string Dequeue() {
    std::string value = queue_.front();
    queue_.pop_front();
    Save();
    return value;
  }

  void Enqueue(const std::string& value) {
    queue_.push_back(value);
    Save();
  }

 private:
  std::string file_name_;
  std::deque<std::string> queue_;

  void Save();
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_COMPONENT_PERSISTENT_QUEUE_H_
