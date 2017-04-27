// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_DEVICE_INFO_DEVICE_INFO_IMPL_H_
#define APPS_MODULAR_SRC_DEVICE_INFO_DEVICE_INFO_IMPL_H_

#include <string>

#include "apps/modular/services/device/device_info.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_request.h"

namespace modular {

class DeviceInfoImpl : public modular::DeviceInfo {
 public:
  DeviceInfoImpl(const std::string& user);

  void AddBinding(fidl::InterfaceRequest<DeviceInfo> request);

 private:
  void GetDeviceIdForSyncing(
      const GetDeviceIdForSyncingCallback& callback) override;

  void GetDeviceProfile(const GetDeviceProfileCallback& callback) override;

  std::string device_id_;
  std::string device_profile_;

  fidl::BindingSet<modular::DeviceInfo> bindings_;

  FTL_DISALLOW_COPY_AND_ASSIGN(DeviceInfoImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_DEVICE_INFO_DEVICE_INFO_IMPL_H_
