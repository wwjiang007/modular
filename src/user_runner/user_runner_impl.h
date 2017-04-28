// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MODULAR_SRC_USER_RUNNER_USER_RUNNER_IMPL_H_
#define APPS_MODULAR_SRC_USER_RUNNER_USER_RUNNER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "apps/ledger/services/public/ledger.fidl.h"
#include "apps/maxwell/services/resolver/resolver.fidl.h"
#include "apps/maxwell/services/suggestion/suggestion_provider.fidl.h"
#include "apps/maxwell/services/user/user_intelligence_provider.fidl.h"
#include "apps/modular/lib/auth/token_provider_impl.h"
#include "apps/modular/lib/fidl/app_client.h"
#include "apps/modular/lib/fidl/array_to_string.h"
#include "apps/modular/lib/fidl/scope.h"
#include "apps/modular/lib/rapidjson/rapidjson.h"
#include "apps/modular/services/config/config.fidl.h"
#include "apps/modular/services/story/story_provider.fidl.h"
#include "apps/modular/services/user/user_context.fidl.h"
#include "apps/modular/services/user/user_runner.fidl.h"
#include "apps/modular/services/user/user_shell.fidl.h"
#include "apps/modular/src/user_runner/conflict_resolver_impl.h"
#include "apps/mozart/services/views/view_token.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/interface_ptr.h"
#include "lib/ftl/macros.h"

namespace modular {

class AgentRunner;
class ComponentContextImpl;
class DeviceInfoImpl;
class DeviceMapImpl;
class FocusHandler;
class LinkImpl;
class MessageQueueManager;
class StoryStorageImpl;
class StoryProviderImpl;
class VisibleStoriesHandler;

class UserRunnerImpl : UserRunner, UserShellContext {
 public:
  UserRunnerImpl(
      app::ApplicationEnvironmentPtr application_environment,
      fidl::Array<uint8_t> user_id,
      const fidl::String& device_name,
      AppConfigPtr user_shell,
      AppConfigPtr story_shell,
      const fidl::String& auth_token,
      fidl::InterfaceHandle<ledger::LedgerRepository> ledger_repository,
      fidl::InterfaceHandle<UserContext> user_context,
      fidl::InterfaceRequest<mozart::ViewOwner> view_owner_request,
      fidl::InterfaceRequest<UserRunner> user_runner_request);

  ~UserRunnerImpl() override;

 private:
  // |UserRunner|
  void Terminate(const TerminateCallback& done) override;

  // |UserShellContext|
  void GetDeviceName(const GetDeviceNameCallback& callback) override;
  void GetAgentProvider(fidl::InterfaceRequest<AgentProvider> request) override;
  void GetStoryProvider(fidl::InterfaceRequest<StoryProvider> request) override;
  void GetSuggestionProvider(
      fidl::InterfaceRequest<maxwell::SuggestionProvider> request) override;
  void GetVisibleStoriesController(
      fidl::InterfaceRequest<VisibleStoriesController> request) override;
  void GetFocusController(
      fidl::InterfaceRequest<FocusController> request) override;
  void GetFocusProvider(fidl::InterfaceRequest<FocusProvider> request) override;
  void GetLink(fidl::InterfaceRequest<Link> request) override;

  app::ServiceProviderPtr GetServiceProvider(AppConfigPtr config);
  app::ServiceProviderPtr GetServiceProvider(const std::string& url);

  std::unique_ptr<fidl::Binding<UserRunner>> binding_;
  fidl::Binding<UserShellContext> user_shell_context_binding_;

  ledger::LedgerRepositoryPtr ledger_repository_;
  ledger::LedgerPtr ledger_;
  ledger::PagePtr root_page_;
  ConflictResolverImpl conflict_resolver_;

  Scope user_scope_;

  std::unique_ptr<AppClientBase> maxwell_;
  AppClient<UserShell> user_shell_;

  std::unique_ptr<StoryProviderImpl> story_provider_impl_;
  std::unique_ptr<MessageQueueManager> message_queue_manager_;
  std::unique_ptr<AgentRunner> agent_runner_;
  std::unique_ptr<DeviceInfoImpl> device_info_impl_;
  std::unique_ptr<DeviceMapImpl> device_map_impl_;
  TokenProviderImpl token_provider_impl_;
  std::string device_name_;

  // This component context is supplied to the user intelligence
  // provider, below, so it can run agents and create message queues.
  std::unique_ptr<ComponentContextImpl> maxwell_component_context_impl_;
  std::unique_ptr<fidl::Binding<ComponentContext>>
      maxwell_component_context_binding_;
  fidl::InterfacePtr<maxwell::UserIntelligenceProvider>
      user_intelligence_provider_;

  // Keeps connections to applications started here around so they are
  // killed when this instance is deleted.
  std::vector<app::ApplicationControllerPtr> application_controllers_;

  std::unique_ptr<FocusHandler> focus_handler_;
  std::unique_ptr<VisibleStoriesHandler> visible_stories_handler_;

  // Given to the user shell so it can store its own data. These data
  // are shared between all user shells (so it's not private to the
  // user shell *app*).
  std::unique_ptr<LinkImpl> user_shell_link_;
  std::unique_ptr<StoryStorageImpl> link_storage_;

  FTL_DISALLOW_COPY_AND_ASSIGN(UserRunnerImpl);
};

}  // namespace modular

#endif  // APPS_MODULAR_SRC_USER_RUNNER_USER_RUNENR_IMPL_H_
