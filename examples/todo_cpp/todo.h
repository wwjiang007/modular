// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_EXAMPLES_TODO_CPP_TODO_H_
#define APPS_MODULAR_EXAMPLES_TODO_CPP_TODO_H_

#include <random>

#include "application/lib/app/application_context.h"
#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/modular/examples/todo_cpp/generator.h"
#include "apps/modular/services/component/component_context.fidl.h"
#include "apps/modular/services/module/module.fidl.h"
#include "apps/modular/services/module/module_context.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/ftl/command_line.h"
#include "lib/ftl/macros.h"

namespace todo {

using Key = fidl::Array<uint8_t>;

class TodoApp : public modular::Module, public ledger::PageWatcher {
 public:
  TodoApp();

  // modular::Module:
  void Initialize(
      fidl::InterfaceHandle<modular::ModuleContext> module_context,
      fidl::InterfaceHandle<modular::Link> link,
      fidl::InterfaceHandle<app::ServiceProvider> incoming_services,
      fidl::InterfaceRequest<app::ServiceProvider> outgoing_services) override;

  void Stop(const StopCallback& done) override;

  // ledger::PageWatcher:
  void OnChange(ledger::PageChangePtr page_change,
                ledger::ResultState result_state,
                const OnChangeCallback& callback) override;

 private:
  void List(ledger::PageSnapshotPtr snapshot);

  void GetKeys(std::function<void(fidl::Array<Key>)> callback);

  void AddNew();

  void DeleteOne(fidl::Array<Key> keys);

  void Act();

  std::default_random_engine rng_;
  std::normal_distribution<> size_distribution_;
  std::uniform_int_distribution<> delay_distribution_;
  Generator generator_;
  std::unique_ptr<app::ApplicationContext> context_;
  fidl::Binding<modular::Module> module_binding_;
  fidl::InterfacePtr<modular::ModuleContext> module_context_;
  modular::ComponentContextPtr component_context_;
  ledger::LedgerPtr ledger_;
  fidl::Binding<ledger::PageWatcher> page_watcher_binding_;
  ledger::PagePtr page_;

  FTL_DISALLOW_COPY_AND_ASSIGN(TodoApp);
};

}  // namespace todo

#endif  // APPS_MODULAR_EXAMPLES_TODO_CPP_TODO_H_
