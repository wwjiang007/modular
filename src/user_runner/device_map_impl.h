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
class DeviceMapImpl : DeviceMap, ledger::PageWatcher {
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

  // |ledger::PageWatcher|
  void OnChange(ledger::PageChangePtr page,
                ledger::ResultState result_state,
                const OnChangeCallback& callback) override;

  const std::string device_name_;
  ledger::Page* const page_;

  fidl::BindingSet<DeviceMap> bindings_;
  fidl::Binding<ledger::PageWatcher> page_watcher_binding_;

  PageClient page_client_;
  OperationQueue operation_queue_;

  // Operations implemented here.
  class QueryCall;

  FTL_DISALLOW_COPY_AND_ASSIGN(DeviceMapImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_USER_RUNNER_DEVICE_MAP_H_
