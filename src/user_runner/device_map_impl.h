// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_USER_RUNNER_DEVICE_MAP_IMPL_H_
#define APPS_MODULAR_SRC_USER_RUNNER_DEVICE_MAP_IMPL_H_

#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "apps/modular/lib/fidl/page_client.h"
#include "apps/modular/services/user/device_map.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"

namespace modular {

// See services/user/device_map.fidl for details.
//
// Mostly scaffolding to demonstrate a complete page client.
class DeviceMapImpl : DeviceMap, PageClient {
 public:
  DeviceMapImpl(const std::string& device_name,
                const std::string& device_id,
                const std::string& device_profile,
                ledger::Page* const page);
  ~DeviceMapImpl() override;

  void Connect(fidl::InterfaceRequest<DeviceMap> request);

 private:
  // |DeviceMap|
  void Query(const QueryCallback& callback) override;

  // |PageClient|
  void OnChange(const std::string& key, const std::string& value) override;

  // |PageClient|
  void OnDelete(const std::string& key) override;

  fidl::BindingSet<DeviceMap> bindings_;

  OperationQueue operation_queue_;

  // Operations implemented here.
  class QueryCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(DeviceMapImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_USER_RUNNER_DEVICE_MAP_H_
