#ifdef NDEBUG
#undef NDEBUG
#endif
// Multiple Component ABI v2 modules may implement disjoint actions for the
// SAME service, but overlapping roles or incompatible schemas must fail.
#include "umaa/internal/component_runtime.hpp"
#include "umaa/module_api_v2.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

template<class F> void must_reject(F&& f) {
  bool failed=false;
  try{f();}catch(const std::exception&){failed=true;}
  assert(failed);
}

int main(int argc,char** argv) {
  if(argc!=6)return 2;
  using umaa::internal::ComponentRuntime;
  ComponentRuntime runtime;
  runtime.load_backend(argv[5],106);
  runtime.load_component(argv[1]); // multiple services: provider + commands
  runtime.load_component(argv[2]); // same BatteryStatus, consumer-only
  assert(runtime.services().size()==3); // unique logical service identities

  // A duplicate provider, a schema mismatch, and the same module ID all
  // reject atomically; previous routes must remain callable.
  must_reject([&]{runtime.load_component(argv[3]);});
  must_reject([&]{runtime.load_component(argv[4]);});
  must_reject([&]{runtime.load_component(argv[2]);});
  assert(runtime.services().size()==3);

  int32_t outcome=-1;
  std::vector<std::uint8_t> out;
  const std::vector<std::uint8_t> no_payload;
  std::int32_t report=80;
  std::vector<std::uint8_t> payload(sizeof(report));
  std::memcpy(payload.data(),&report,payload.size());
  const std::string service="Demo::EO::BatteryStatus";

  // Producer business actions remain routed only to the provider DLL/SO.
  assert(runtime.call(service,UMAA_COMPONENT_REPORT,no_payload,out,outcome)==
         UMAA_COMPONENT_OK);
  std::int32_t actual=0;
  assert(out.size()==sizeof(actual));
  std::memcpy(&actual,out.data(),sizeof(actual));
  assert(actual==80);
  // Inbound DDS Report lifecycle reaches only the Consumer module.
  assert(runtime.call(service,UMAA_COMPONENT_REPORT_ACCEPT,payload,out,outcome,71)==
         UMAA_COMPONENT_OK);
  assert(outcome==0);
  assert(runtime.call(service,UMAA_COMPONENT_REPORT_DISPOSE,no_payload,out,outcome,71)==
         UMAA_COMPONENT_OK);
  assert(outcome==0);
  assert(runtime.call(service,UMAA_COMPONENT_REPORT_ACCEPT,no_payload,out,outcome,71)==
         UMAA_COMPONENT_INVALID);
  assert(runtime.call(service,UMAA_COMPONENT_CANCEL,no_payload,out,outcome,71)==
         UMAA_COMPONENT_NOT_FOUND);

  auto subscriber=runtime.subscribe_report_fixture(
    service,"demo/BatteryStatus","demo:battery-v1","demo-reliable");
  auto publisher=runtime.open_report_fixture(
    service,"demo/BatteryStatus","demo:battery-v1","vehicle-1");
  publisher->update();
  assert(subscriber->poll(1)==1);
  std::vector<std::uint8_t> latest;
  assert(subscriber->latest_for("vehicle-1",latest)&&latest.size()==sizeof(report));
  std::memcpy(&actual,latest.data(),sizeof(actual));
  assert(actual==80);
  // Retained publisher still fences *only* its Provider module.
  assert(!runtime.unload_component("vehicle-component"));
  // A Consumer on the same service is independently unloadable.
  assert(runtime.unload_component("battery-consumer"));
  assert(runtime.services().size()==3);
  assert(runtime.call(service,UMAA_COMPONENT_REPORT_ACCEPT,payload,out,outcome,71)==
         UMAA_COMPONENT_NOT_FOUND);
  assert(runtime.call(service,UMAA_COMPONENT_REPORT,no_payload,out,outcome)==
         UMAA_COMPONENT_OK);
  publisher->dispose();publisher->close();publisher.reset();
  assert(runtime.unload_component("vehicle-component"));
  assert(runtime.services().empty());

  // Consumer-only local service does not invent a local Provider. Subscription
  // is permitted and still must obey the actual backend TypeBinding.
  runtime.load_component(argv[2]);
  assert(runtime.services().size()==1);
  assert(runtime.call(service,UMAA_COMPONENT_REPORT,no_payload,out,outcome)==
         UMAA_COMPONENT_NOT_FOUND);
  assert(runtime.call(service,UMAA_COMPONENT_REPORT_ACCEPT,payload,out,outcome,72)==
         UMAA_COMPONENT_OK);
  auto remote_only=runtime.subscribe_report_fixture(
    service,"demo/BatteryStatus","demo:battery-v1","demo-reliable");
  assert(remote_only->poll(1)==0);
  assert(runtime.unload_component("battery-consumer"));
  assert(runtime.services().empty());
  std::cout<<"PASS cross-module role-aware service routing, fail-closed collisions and unload\n";
  return 0;
}
