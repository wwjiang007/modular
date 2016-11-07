// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/application_manager/url_resolver.h"

namespace modular {
namespace {

constexpr char kFileUriPrefix[] = "file://";
constexpr size_t kFileUriPrefixLength = sizeof(kFileUriPrefix) - 1;

}  // namespace

std::string GetPathFromURL(const std::string& url) {
  if (url.find(kFileUriPrefix) == 0)
    return url.substr(kFileUriPrefixLength);
  return std::string();
}

std::string GetURLFromPath(const std::string& path) {
  return kFileUriPrefix + path;
}

}  // namespace modular