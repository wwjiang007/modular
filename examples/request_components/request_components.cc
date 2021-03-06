// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "application/lib/app/application_context.h"
#include "application/lib/app/connect.h"
#include "apps/modular/services/component/component.fidl.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "lib/mtl/tasks/message_loop.h"

namespace {

class RequestComponentApp {
 public:
  RequestComponentApp(const std::string& component_id)
      : context_(app::ApplicationContext::CreateFromStartupInfo()) {
    component_index_ =
        context_->ConnectToEnvironmentService<component::ComponentIndex>();
    component_index_->GetComponent(
        component_id,
        [this](component::ComponentManifestPtr manifest,
               fidl::InterfaceHandle<component::ComponentResources> resources,
               network::NetworkErrorPtr error) {
          FTL_LOG(INFO) << "GetComponent returned.";
        });
  }

 private:
  std::unique_ptr<app::ApplicationContext> context_;
  fidl::InterfacePtr<component::ComponentIndex> component_index_;

  FTL_DISALLOW_COPY_AND_ASSIGN(RequestComponentApp);
};

}  // namespace

int main(int argc, const char** argv) {
  mtl::MessageLoop loop;
  FTL_CHECK(argc == 2);
  RequestComponentApp app(argv[1]);
  loop.Run();
  return 0;
}
