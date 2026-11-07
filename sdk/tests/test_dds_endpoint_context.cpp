#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/dds_endpoint_context.hpp"
#include "umaa/internal/component_runtime.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
struct Samples {
  int count{0};
};
void on_sample(void* user,const UmaaDdsSampleV1* sample) {
  if(user && sample && sample->event_kind==UMAA_DDS_EVENT_DATA)
    ++static_cast<Samples*>(user)->count;
}
template<class Func> void must_fail(Func&& fn) {
  bool failed=false;
  try {fn();} catch(const std::exception&) {failed=true;}
  assert(failed);
}
}
int main(int argc,char** argv) {
  if(argc!=2)return 2;
  const UmaaDdsTopicV1 topic{"demo/BatteryStatus","demo:battery-v1",
    "demo-reliable",UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};

  std::shared_ptr<umaa::DdsEndpointContext> retained;
  {
    umaa::internal::ComponentRuntime runtime;
    must_fail([&]{(void)runtime.dds_endpoint_context();});
    runtime.load_backend(argv[1],42);
    const auto first=runtime.dds_endpoint_context();
    assert(first && first==runtime.dds_endpoint_context());
    retained=first;

    // A writer opened through the shared context and an existing Report
    // reader owned by ComponentRuntime MUST communicate in the same Domain.
    const auto report=runtime.subscribe_report_fixture(
      "Demo::EO::BatteryStatus","demo/BatteryStatus","demo:battery-v1",
      "demo-reliable");
    void* writer=first->writer(topic);
    const std::uint8_t payload=0x5a;
    const UmaaDdsSampleV1 sample{&payload,1,"source-1",100,
      UMAA_DDS_EVENT_DATA};
    assert(first->domain()->write(writer,sample)==UMAA_DDS_OK);
    assert(report->poll(4)==1);
    std::vector<std::uint8_t> read;
    assert(report->latest_for("source-1",read) &&
           read==std::vector<std::uint8_t>{payload});
    first->domain()->close_writer(writer);

    // Existing Reader, not only context-created endpoints, freezes QoS.
    must_fail([&]{
      runtime.set_qos_profiles({{"demo-reliable",umaa::qos::Policy{}}});
    });
  }

  // A Client/Handle can outlive its parent Application; context retains
  // the Domain and backend mapping until its last native endpoint closes.
  Samples received;
  auto* reader=retained->reader(topic,on_sample,&received);
  auto* writer=retained->writer(topic);
  const std::uint8_t payload=7;
  const UmaaDdsSampleV1 sample{&payload,1,"source-2",200,
    UMAA_DDS_EVENT_DATA};
  assert(retained->domain()->write(writer,sample)==UMAA_DDS_OK);
  assert(retained->domain()->poll(reader,4)==1 && received.count==1);
  retained->domain()->close_reader(reader);
  retained->domain()->close_writer(writer);

  // The reviewed requirements gate cannot be satisfied by absent profiles:
  // even a V1-capable mock endpoint must not be opened as official QoS.
  umaa::qos::Requirements reviewed;
  reviewed.require_ordered_access=true;
  must_fail([&]{
    auto* w=retained->writer(topic,reviewed);
    retained->domain()->close_writer(w);
  });
  must_fail([&]{
    auto* r=retained->reader(topic,on_sample,&received,reviewed);
    retained->domain()->close_reader(r);
  });

  // The old mock DDS plugin has no QoS V2 extension. An explicit profile
  // MUST fail rather than falling back silently to legacy V1 endpoints.
  umaa::DdsEndpointContext explicit_context(argv[1],43);
  explicit_context.set_qos_profiles({{"demo-reliable",umaa::qos::Policy{}}});
  must_fail([&]{
    auto* w=explicit_context.writer(topic);
    explicit_context.domain()->close_writer(w);
  });
  must_fail([&]{
    auto* r=explicit_context.reader(topic,on_sample,&received);
    explicit_context.domain()->close_reader(r);
  });

  umaa::DdsEndpointContext missing_profile(argv[1],44);
  missing_profile.set_qos_profiles({{"another-profile",umaa::qos::Policy{}}});
  must_fail([&]{(void)missing_profile.writer(topic);});
  must_fail([&]{(void)missing_profile.reader(topic,on_sample,&received);});

  std::cout<<"PASS shared DDS host context, lifetime, QoS freeze, V2 no fallback\n";
  return 0;
}
