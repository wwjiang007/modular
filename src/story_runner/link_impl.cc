// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/modular/src/story_runner/link_impl.h"

#include <functional>

#include "apps/modular/lib/rapidjson/rapidjson.h"
#include "apps/modular/services/story/link.fidl.h"
#include "lib/fidl/cpp/bindings/interface_handle.h"
#include "lib/fidl/cpp/bindings/interface_request.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/ftl/logging.h"

namespace modular {

namespace {

template <typename Doc>
rapidjson::GenericPointer<typename Doc::ValueType> CreatePointerFromArray(
    const Doc& doc,
    typename fidl::Array<fidl::String>::Iterator begin,
    typename fidl::Array<fidl::String>::Iterator end) {
  rapidjson::GenericPointer<typename Doc::ValueType> pointer;
  for (auto it = begin; it != end; ++it) {
    pointer = pointer.Append(it->get(), nullptr);
  }
  return pointer;
}

//// Take an Array of fidl::String and convert it to "/a/b/c" form for display.
//// For debugging/logging purposes only.
// inline std::string PrettyPrintPath(const fidl::Array<fidl::String>& path) {
//  return (path.is_null() || path.storage().empty())
//             ? std::string("/")
//             : modular::PrettyPrintPath(path.To<std::vector<std::string>>());
//}
}  // namespace

LinkImpl::LinkImpl(StoryStorageImpl* const story_storage,
                   const fidl::String& name,
                   fidl::InterfaceRequest<Link> request)
    : name_(name),
      story_storage_(story_storage),
      write_link_data_(Bottleneck::FRONT, this, &LinkImpl::WriteLinkDataImpl) {
  requests_.emplace_back(std::move(request));
  ReadLinkData(
      [this]() {
        for (auto& request : requests_) {
          LinkConnection::New(this, std::move(request));
        }
        requests_.clear();
        ready_ = true;
      });

  story_storage_->WatchLink(
      name, [this](const fidl::String& json) { OnChange(json); });
}

LinkImpl::~LinkImpl() {}

void LinkImpl::Connect(fidl::InterfaceRequest<Link> request) {
  if (ready_) {
    LinkConnection::New(this, std::move(request));
  } else {
    requests_.emplace_back(std::move(request));
  }
}

void LinkImpl::SetSchema(const fidl::String& json_schema) {
  rapidjson::Document doc;
  doc.Parse(json_schema.get());
  FTL_DCHECK(!doc.HasParseError())
      << "LinkImpl::SetSchema() " << name_ << " JSON parse failed error #"
      << doc.GetParseError() << std::endl
      << json_schema.get();
  schema_doc_ = std::make_unique<rapidjson::SchemaDocument>(doc);
}

// The |LinkConnection| object knows which client made the call to Set() or
// Update(), so it notifies either all clients or all other clients, depending
// on whether WatchAll() or Watch() was called, respectively.
//
// TODO(jimbe) This mechanism breaks if the call to Watch() is made
// *after* the call to SetAllDocument(). Need to find a way to improve
// this.

void LinkImpl::Set(fidl::Array<fidl::String> path,
                   const fidl::String& json,
                   LinkConnection* const src) {
  //  FTL_LOG(INFO) << "**** Set()" << std::endl
  //                << "PATH " << PrettyPrintPath(path) << std::endl
  //                << "JSON " << json;
  CrtJsonDoc new_value;
  new_value.Parse(json);
  if (new_value.HasParseError()) {
    // TODO(jimbe) Handle errors better
    FTL_CHECK(!new_value.HasParseError())
        << "LinkImpl::Set() " << name_ << " JSON parse failed error #"
        << new_value.GetParseError() << std::endl
        << json.get();
    return;
  }

  bool dirty = true;
  bool alreadyExist = false;

  CrtJsonPointer ptr = CreatePointerFromArray(doc_, path.begin(), path.end());
  CrtJsonValue& current_value =
      ptr.Create(doc_, doc_.GetAllocator(), &alreadyExist);
  if (alreadyExist) {
    dirty = new_value != current_value;
  }

  if (dirty) {
    ptr.Set(doc_, new_value);
    ValidateSchema("LinkImpl::Set", ptr, json.get());
    DatabaseChanged(src);
  }
  // FTL_LOG(INFO) << "LinkImpl::Set() " << JsonValueToPrettyString(doc_);
}

void LinkImpl::UpdateObject(fidl::Array<fidl::String> path,
                            const fidl::String& json,
                            LinkConnection* const src) {
  //  FTL_LOG(INFO) << "**** UpdateObject() starting" << std::endl
  //                << "PATH " << PrettyPrintPath(path) << std::endl
  //                << "JSON " << json;
  CrtJsonDoc new_value;
  new_value.Parse(json);
  if (new_value.HasParseError()) {
    // TODO(jimbe) Handle errors better
    FTL_CHECK(!new_value.HasParseError())
        << "LinkImpl::UpdateObject() " << name_ << " JSON parse failed error #"
        << new_value.GetParseError() << std::endl
        << json.get();
    return;
  }

  auto ptr = CreatePointerFromArray(doc_, path.begin(), path.end());
  CrtJsonValue& current_value = ptr.Create(doc_);

  const bool dirty =
      MergeObject(current_value, std::move(new_value), doc_.GetAllocator());
  if (dirty) {
    ValidateSchema("LinkImpl::UpdateObject", ptr, json.get());
    DatabaseChanged(src);
  }
  //  FTL_LOG(INFO) << "LinkImpl::UpdateObject() result "
  //                << JsonValueToPrettyString(doc_);
}

void LinkImpl::Erase(fidl::Array<fidl::String> path,
                     LinkConnection* const src) {
  //  FTL_LOG(INFO) << "LinkImpl::Erase() "
  //                << "PATH " << PrettyPrintPath(path) << std::endl;
  auto ptr = CreatePointerFromArray(doc_, path.begin(), path.end());
  auto value = ptr.Get(doc_);
  if (value != nullptr && ptr.Erase(doc_)) {
    ValidateSchema("LinkImpl::Erase", ptr, std::string());
    DatabaseChanged(src);
  }
}

void LinkImpl::Sync(const std::function<void()>& callback) {
  story_storage_->Sync(callback);
}

// Merge source into target. The values will be move()'d.
// Returns true if the merge operation caused any changes.
bool LinkImpl::MergeObject(CrtJsonValue& target,
                           CrtJsonValue&& source,
                           CrtJsonValue::AllocatorType& allocator) {
  if (!source.IsObject()) {
    FTL_LOG(INFO) << "LinkImpl::MergeObject() - source is not an object "
                  << JsonValueToPrettyString(doc_);
    return false;
  }

  if (!target.IsObject()) {
    target = std::move(source);
    return true;
  }

  bool diff = false;
  for (auto& source_itr : source.GetObject()) {
    auto target_itr = target.FindMember(source_itr.name);
    // If the value already exists and not identical, set it.
    if (target_itr == target.MemberEnd()) {
      target.AddMember(std::move(source_itr.name), std::move(source_itr.value),
                       allocator);
      diff = true;
    } else if (source_itr.value != target_itr->value) {
      // TODO(jimbe) The above comparison is O(n^2). Need to revisit the
      // detection logic.
      target_itr->value = std::move(source_itr.value);
      diff = true;
    }
  }
  return diff;
}

void LinkImpl::ReadLinkData(const std::function<void()>& done) {
  story_storage_->ReadLinkData(name_, [this, done](const fidl::String& json) {
    if (!json.is_null()) {
      doc_.Parse(json.get());
      FTL_LOG(INFO) << "LinkImpl::ReadLinkData() "
                    << JsonValueToPrettyString(doc_);
    }

    done();
  });
}

void LinkImpl::WriteLinkData(const std::function<void()>& done) {
  write_link_data_(done);
}

void LinkImpl::WriteLinkDataImpl(const std::function<void()>& done) {
  story_storage_->WriteLinkData(name_, JsonValueToString(doc_), done);
}

void LinkImpl::DatabaseChanged(LinkConnection* const src) {
  // src is only used to compare its value. If the connection was
  // deleted before the callback is invoked, it will also be removed
  // from connections_.
  WriteLinkData([this, src] { NotifyWatchers(src); });
}

void LinkImpl::ValidateSchema(const char* const entry_point,
                              const CrtJsonPointer& pointer,
                              const std::string& json) {
  if (!schema_doc_) {
    return;
  }

  rapidjson::GenericSchemaValidator<rapidjson::SchemaDocument> validator(
      *schema_doc_);
  if (!doc_.Accept(validator)) {
    if (!validator.IsValid()) {
      rapidjson::StringBuffer sbpath;
      validator.GetInvalidSchemaPointer().StringifyUriFragment(sbpath);
      rapidjson::StringBuffer sbdoc;
      validator.GetInvalidDocumentPointer().StringifyUriFragment(sbdoc);
      rapidjson::StringBuffer sbapipath;
      pointer.StringifyUriFragment(sbapipath);
      FTL_LOG(ERROR) << "Schema constraint violation in " << name_ << ":"
                     << std::endl
                     << "  Constraint " << sbpath.GetString() << "/"
                     << validator.GetInvalidSchemaKeyword() << std::endl
                     << "  Doc location: " << sbdoc.GetString() << std::endl
                     << "  API " << entry_point << std::endl
                     << "  API path " << sbapipath.GetString() << std::endl
                     << "  API json " << json << std::endl;
    }
  }
}

void LinkImpl::OnChange(const fidl::String& json) {
  // NOTE(jimbe) With rapidjson, the opposite check is more expensive,
  // O(n^2), so we won't do it for now. See case kObjectType in
  // operator==() in include/rapidjson/document.h.
  //  if (doc_.Equals(json)) {
  //    return;
  //  }
  //
  // Since all json in a link was written by the same serializer, this
  // check is mostly accurate. This test has false negatives when only
  // order differs.
  if (json == JsonValueToString(doc_)) {
    return;
  }

  // TODO(jimbe) Decide how these changes should be merged into the current
  // CrtJsonDoc. In this first iteration, we'll do a wholesale replace.
  doc_.Parse(json);
  NotifyWatchers(nullptr);
}

void LinkImpl::NotifyWatchers(LinkConnection* const src) {
  for (auto& dst : connections_) {
    const bool self_notify = (dst.get() != src);
    dst->NotifyWatchers(doc_, self_notify);
  }
}

void LinkImpl::AddConnection(LinkConnection* const connection) {
  connections_.emplace_back(connection);
}

void LinkImpl::RemoveConnection(LinkConnection* const connection) {
  auto it =
      std::remove_if(connections_.begin(), connections_.end(),
                     [connection](const std::unique_ptr<LinkConnection>& p) {
                       return p.get() == connection;
                     });
  FTL_DCHECK(it != connections_.end());
  connections_.erase(it, connections_.end());

  if (connections_.empty() && orphaned_handler_) {
    orphaned_handler_();
  }
}

LinkConnection::LinkConnection(LinkImpl* const impl,
                               fidl::InterfaceRequest<Link> link_request)
    : impl_(impl), binding_(this, std::move(link_request)) {
  impl_->AddConnection(this);
  binding_.set_connection_error_handler(
      [this] { impl_->RemoveConnection(this); });
}

LinkConnection::~LinkConnection() {}

void LinkConnection::Watch(fidl::InterfaceHandle<LinkWatcher> watcher) {
  AddWatcher(std::move(watcher), false);
}

void LinkConnection::WatchAll(fidl::InterfaceHandle<LinkWatcher> watcher) {
  AddWatcher(std::move(watcher), true);
}

void LinkConnection::AddWatcher(fidl::InterfaceHandle<LinkWatcher> watcher,
                                const bool self_notify) {
  LinkWatcherPtr watcher_ptr;
  watcher_ptr.Bind(std::move(watcher));

  // TODO(jimbe) We need to send an initial notification of state until
  // there is snapshot information that can be used by clients to query the
  // state at this instant. Otherwise there is no sequence information about
  // total state versus incremental changes.
  auto& doc = impl_->doc();
  watcher_ptr->Notify(JsonValueToString(doc));

  auto& watcher_set = self_notify ? all_watchers_ : watchers_;
  watcher_set.AddInterfacePtr(std::move(watcher_ptr));
}

void LinkConnection::NotifyWatchers(const CrtJsonDoc& doc,
                                    const bool self_notify) {
  fidl::String json(JsonValueToString(impl_->doc()));

  if (self_notify) {
    watchers_.ForAllPtrs(
        [&json](LinkWatcher* const watcher) { watcher->Notify(json); });
  }
  all_watchers_.ForAllPtrs(
      [&json](LinkWatcher* const watcher) { watcher->Notify(json); });
}

void LinkConnection::Dup(fidl::InterfaceRequest<Link> dup) {
  LinkConnection::New(impl_, std::move(dup));
}

void LinkConnection::Sync(const SyncCallback& callback) {
  impl_->Sync(callback);
}

void LinkConnection::SetSchema(const fidl::String& json_schema) {
  impl_->SetSchema(json_schema);
}

void LinkConnection::UpdateObject(fidl::Array<fidl::String> path,
                                  const fidl::String& json) {
  impl_->UpdateObject(std::move(path), json, this);
}

void LinkConnection::Set(fidl::Array<fidl::String> path,
                         const fidl::String& json) {
  impl_->Set(std::move(path), json, this);
}

void LinkConnection::Erase(fidl::Array<fidl::String> path) {
  impl_->Erase(std::move(path), this);
}

void LinkConnection::Get(fidl::Array<fidl::String> path,
                         const GetCallback& callback) {
  auto p = CreatePointerFromArray(impl_->doc(), path.begin(), path.end())
               .Get(impl_->doc());
  if (p == nullptr) {
    callback(fidl::String());
  } else {
    callback(fidl::String(JsonValueToString(*p)));
  }
}

}  // namespace modular
