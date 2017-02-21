// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_LIB_FIDL_VIEW_HOST_H_
#define APPS_MODULAR_LIB_FIDL_VIEW_HOST_H_

#include <map>
#include <memory>

#include "apps/mozart/lib/view_framework/base_view.h"
#include "apps/mozart/services/views/view_manager.fidl.h"
#include "lib/ftl/macros.h"

namespace modular {

struct ViewData;

// A class that allows modules to display the UI of their child
// modules, without displaying any UI on their own. Used for modules
// that play the role of a view controller (aka quarterback, recipe).
// It supports to embed views of *multiple* children, which are laid
// out horizontally.
class ViewHost : public mozart::BaseView {
 public:
  explicit ViewHost(
      mozart::ViewManagerPtr view_manager,
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request);
  ~ViewHost() override;

  // Connects one more view. Calling this method multiple times adds
  // multiple views and lays them out horizontally next to each other.
  // This is experimental to establish data flow patterns in toy
  // applications and can be changed or extended as needed.
  void ConnectView(fidl::InterfaceHandle<mozart::ViewOwner> view_owner);

 private:
  // |BaseView|:
  void OnChildAttached(uint32_t child_key,
                       mozart::ViewInfoPtr child_view_info) override;
  // |BaseView|:
  void OnChildUnavailable(uint32_t child_key) override;

  // |BaseView|:
  void OnLayout() override;

  // |BaseView|:
  void OnDraw() override;

  std::map<uint32_t, std::unique_ptr<ViewData>> views_;
  uint32_t next_child_key_{};

  FTL_DISALLOW_COPY_AND_ASSIGN(ViewHost);
};

}  // namespace modular

#endif  // APPS_MODULAR_LIB_FIDL_VIEW_HOST_H_
