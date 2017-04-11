// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_USER_RUNNER_CONFLICT_RESOLVER_IMPL_H_
#define APPS_MODULAR_SRC_USER_RUNNER_CONFLICT_RESOLVER_IMPL_H_

#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/modular/lib/fidl/operation.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"

namespace modular {

// A conflict resolver for the user's ledger. So far it does nothing.
class ConflictResolverImpl : ledger::ConflictResolverFactory,
                             ledger::ConflictResolver {
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
  void Resolve(fidl::InterfaceHandle<ledger::PageSnapshot> left_version,
               fidl::InterfaceHandle<ledger::PageSnapshot> right_version,
               fidl::InterfaceHandle<ledger::PageSnapshot> common_version,
               fidl::InterfaceHandle<ledger::MergeResultProvider>
                   result_provider) override;

  OperationQueue operation_queue_;

  class LogConflictDiffCall;

  fidl::BindingSet<ledger::ConflictResolverFactory> factory_bindings_;
  fidl::BindingSet<ledger::ConflictResolver> bindings_;

  FTL_DISALLOW_COPY_AND_ASSIGN(ConflictResolverImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_USER_RUNNER_CONFLICT_RESOLVER_IMPL_H_
