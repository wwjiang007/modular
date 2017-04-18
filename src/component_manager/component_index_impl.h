// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_COMPONENT_INDEX_IMPL_H_
#define APPS_COMPONENT_INDEX_IMPL_H_

#include "apps/modular/services/component/component.fidl.h"
#include "apps/modular/src/component_manager/component_resources_impl.h"
#include "apps/modular/src/component_manager/resource_loader.h"
#include "apps/network/services/network_service.fidl.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/ftl/macros.h"

namespace component {

class ComponentIndexImpl : public ComponentIndex {
 public:
  ComponentIndexImpl(network::NetworkServicePtr network_service);

  void GetComponent(const fidl::String& component_id,
                    const GetComponentCallback& callback) override;

  void FindComponentManifests(
      fidl::Map<fidl::String, fidl::String> filter,
      const FindComponentManifestsCallback& callback) override;

 private:
  void LoadComponentIndex(const std::string& contents, const std::string& path);

  std::shared_ptr<ResourceLoader> resource_loader_;

  // A list of component URIs that are installed locally.
  std::vector<std::string> local_index_;

  fidl::BindingSet<ComponentResources, std::unique_ptr<ComponentResourcesImpl>>
      resources_bindings_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ComponentIndexImpl);
};

}  // namespace component

#endif  // APPS_COMPONENT_INDEX_IMPL_H_
