#ifdef NDEBUG
#undef NDEBUG  // Keep assertions effective in Release CI.
#endif
// Mock DDS is intentionally scoped to a single Domain object. This catches
// accidental endpoint-local open_domain(): independent contexts cannot
// exchange any samples with this fixture, even when domain IDs are equal.
#include "umaa/application.hpp"
#include "fixtures/synthetic_collection_binding.hpp"
#include "fixtures/synthetic_split_collection_binding.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
template<class Operation> void must_fail(Operation&& op) {
  bool rejected=false;
  try {op();} catch(const std::exception&) {rejected=true;}
  assert(rejected);
}

using OnePublisher=umaa::DdsCollectionPublisher<SyntheticCollection>;
using OneConsumer=umaa::DdsCollectionConsumer<SyntheticCollection>;
using SplitPublisher=umaa::DdsSplitCollectionPublisher<SyntheticSplitSet>;
using SplitConsumer=umaa::DdsSplitCollectionConsumer<SyntheticSplitSet>;

void single_in_same_application(const char* plugin) {
  umaa::Application app;
  app.use_backend(plugin,92);
  auto consumer=app.consume_collection<SyntheticCollection>("battery",8);
  auto producer=app.publish_collection<SyntheticCollection>("battery",8);
  consumer->expect_snapshot(1);
  producer->snapshot(1,{{"a",1,11,""},{"b",2,22,""}});
  assert(consumer->poll(8)==1);
  auto data=consumer->snapshot();
  assert(data && data->size()==2 && data->at(0)==11 && data->at(1)==22);
  producer->revise({umaa::CollectionOperation<std::int32_t>::upsert({"a",3,33,""})});
  assert(consumer->poll(8)==1);
  data=consumer->snapshot();
  assert(data && data->size()==2 && data->at(0)==33 && data->at(1)==22);
  producer->finish();
  assert(consumer->poll(8)==1);
  assert(consumer->disposed() && !consumer->snapshot() && !producer->faulted());
  must_fail([&]{app.configure_qos_profiles({{"demo-reliable",umaa::qos::Policy{}}});});
}

void split_in_same_application(const char* plugin) {
  umaa::Application app;
  app.use_backend(plugin,93);
  auto consumer=app.consume_split_collection<SyntheticSplitSet>("cells",8);
  auto producer=app.publish_split_collection<SyntheticSplitSet>("cells",8);
  consumer->expect_snapshot(1);
  producer->snapshot(1,{{"a",1,10,""},{"b",2,20,""}});
  assert(consumer->poll(8)==2);
  auto data=consumer->snapshot();
  assert(data && data->size()==2 && data->at(0)==10 && data->at(1)==20);
  producer->revise({umaa::CollectionOperation<std::int32_t>::upsert({"a",3,15,""})});
  assert(consumer->poll(8)==2);
  data=consumer->snapshot();
  assert(data && data->size()==2 && data->at(0)==15 && data->at(1)==20);
  producer->finish();
  assert(consumer->poll(8)==2);
  assert(consumer->disposed() && !consumer->snapshot() && !producer->faulted());
  must_fail([&]{app.configure_qos_profiles({{"demo-reliable",umaa::qos::Policy{}}});});
}

void survives_parent_application(const char* plugin) {
  // Both native endpoint wrappers retain the shared Plugin and Domain after
  // Application destruction; the application never owns their data buffers.
  auto endpoints=[&] {
    umaa::Application app;
    app.use_backend(plugin,94);
    auto sub=app.consume_collection<SyntheticCollection>("kept",8);
    auto pub=app.publish_collection<SyntheticCollection>("kept",8);
    return std::make_pair(std::move(pub),std::move(sub));
  }();
  endpoints.second->expect_snapshot(1);
  endpoints.first->snapshot(1,{{"k",1,64,""}});
  assert(endpoints.second->poll(8)==1);
  auto snapshot=endpoints.second->snapshot();
  assert(snapshot && snapshot->size()==1 && snapshot->at(0)==64);
  endpoints.first->finish();
  assert(endpoints.second->poll(8)==1);
  assert(endpoints.second->disposed());

  auto split_endpoints=[&] {
    umaa::Application app;
    app.use_backend(plugin,95);
    auto sub=app.consume_split_collection<SyntheticSplitSet>("kept-split",8);
    auto pub=app.publish_split_collection<SyntheticSplitSet>("kept-split",8);
    return std::make_pair(std::move(pub),std::move(sub));
  }();
  split_endpoints.second->expect_snapshot(1);
  split_endpoints.first->snapshot(1,{{"k",1,64,""}});
  assert(split_endpoints.second->poll(8)==2);
  snapshot=split_endpoints.second->snapshot();
  assert(snapshot && snapshot->size()==1 && snapshot->at(0)==64);
  split_endpoints.first->finish();
  assert(split_endpoints.second->poll(8)==2);
  assert(split_endpoints.second->disposed());
}

void qos_must_not_fallback(const char* plugin) {
  // The mock implements only the legacy DDS ABI V1. A configured profile
  // forces QoS V2 and must reject every Collection reader and writer.
  umaa::Application app;
  app.use_backend(plugin,96);
  app.configure_qos_profiles({{"demo-reliable",umaa::qos::Policy{}}});
  must_fail([&]{(void)app.publish_collection<SyntheticCollection>("x",8);});
  must_fail([&]{(void)app.consume_collection<SyntheticCollection>("x",8);});
  must_fail([&]{(void)app.publish_split_collection<SyntheticSplitSet>("x",8);});
  must_fail([&]{(void)app.consume_split_collection<SyntheticSplitSet>("x",8);});

  // An unrelated configured profile is not permission to use the default.
  umaa::Application unknown;
  unknown.use_backend(plugin,97);
  unknown.configure_qos_profiles({{"unrelated",umaa::qos::Policy{}}});
  must_fail([&]{(void)unknown.publish_collection<SyntheticCollection>("x",8);});
  must_fail([&]{(void)unknown.consume_collection<SyntheticCollection>("x",8);});
  must_fail([&]{(void)unknown.publish_split_collection<SyntheticSplitSet>("x",8);});
  must_fail([&]{(void)unknown.consume_split_collection<SyntheticSplitSet>("x",8);});
}

void standalone_compatibility_and_null(const char* plugin) {
  OnePublisher single(plugin,98,"legacy",8);
  single.snapshot(1,{{"a",1,10,""}});
  single.finish();
  SplitPublisher split(plugin,99,"legacy-split",8);
  split.snapshot(1,{{"a",1,10,""}});
  split.finish();

  must_fail([&]{OnePublisher p(std::shared_ptr<umaa::DdsEndpointContext>{},"x",8);});
  must_fail([&]{OneConsumer c(std::shared_ptr<umaa::DdsEndpointContext>{},"x",8);});
  must_fail([&]{SplitPublisher p(std::shared_ptr<umaa::DdsEndpointContext>{},"x",8);});
  must_fail([&]{SplitConsumer c(std::shared_ptr<umaa::DdsEndpointContext>{},"x",8);});
}
} // namespace

int main(int argc,char** argv) {
  if(argc!=2)return 2;
  single_in_same_application(argv[1]);
  split_in_same_application(argv[1]);
  survives_parent_application(argv[1]);
  qos_must_not_fallback(argv[1]);
  standalone_compatibility_and_null(argv[1]);
  std::cout<<"PASS shared Collection/Split DDS context, lifetime, lifecycle, QoS rejection\n";
  return 0;
}
