// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_LIB_DEVICE_INFO_DEVICE_INFO_H_
#define APPS_MODULAR_LIB_DEVICE_INFO_DEVICE_INFO_H_

#include <string>

namespace modular {

std::string LoadDeviceProfile();

std::string LoadDeviceID(const std::string& user);

}  // namespace modular

#endif  // APPS_MODULAR_LIB_DEVICE_INFO_DEVICE_INFO_H_
