// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_STORY_RUNNER_CONFLICT_RESOLVER_IMPL_H_
#define APPS_MODULAR_SRC_STORY_RUNNER_CONFLICT_RESOLVER_IMPL_H_

#include "apps/ledger/services/public/ledger.fidl.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"

namespace modular {

// The key under which the device map is stored in the root page of
// the user. A conflict in this key is the only one we actually
// resolve for now.
const char kDeviceMapKey[] = "device-map";

// A conflict resolver for the user's ledger. So far it only resolves
// the device map entry of the root page, so it's very compact. Once
// we resolve more, we need to move it elsewhere and give it more
// structure.
class ConflictResolverImpl : public ledger::ConflictResolverFactory,
                             public ledger::ConflictResolver {
 public:
  ConflictResolverImpl();
  ~ConflictResolverImpl() override;

  fidl::InterfaceHandle<ConflictResolverFactory> AddBinding();

 private:
  // |ConflictResolverFactory|
  void GetPolicy(fidl::Array<uint8_t> page_id,
                 const GetPolicyCallback& callback) override;

  // |ConflictResolverFactory|
  void NewConflictResolver(
      fidl::Array<uint8_t> page_id,
      fidl::InterfaceRequest<ConflictResolver> request) override;

  // |ConflictResolver|
  void Resolve(ledger::PageChangePtr change_left,
               ledger::PageChangePtr change_right,
               fidl::InterfaceHandle<ledger::PageSnapshot> common_version,
               const ResolveCallback& callback) override;

  fidl::BindingSet<ledger::ConflictResolverFactory> factory_bindings_;
  fidl::BindingSet<ledger::ConflictResolver> bindings_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ConflictResolverImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_STORY_RUNNER_CONFLICT_RESOLVER_IMPL_H_