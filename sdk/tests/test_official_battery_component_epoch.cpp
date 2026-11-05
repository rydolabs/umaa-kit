// Vendor-free host-to-DLL/SO regression of a selected official BatteryStatus
// bridge boundary; not official DDS wire, Qos or conformance evidence.
#include "umaa/component_module.hpp"
#include "umaa/official_battery_component_bridge.hpp"

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

namespace b=umaa::official_battery;
static b::Guid guid(unsigned char c) { b::Guid g{}; g[0]=c; return g; }
static void check(bool value,const char* message) {
  if(!value) throw std::runtime_error(message);
}
static b::StableList stable(std::uint64_t generation, int nanoseconds,
                            double voltage) {
  b::StableList v{};
  v.report.source.id=guid(0x33);
  v.report.source.parentID=guid(0x05);
  v.report.timeStamp={1700000000+static_cast<std::int64_t>(generation),0};
  v.report.state=b::State::ON;
  v.report.current=-0.25;
  auto& m=v.report.cellsListMetadata;
  m.listID=guid(0x11);
  m.size=1;
  m.startingElementID=guid(0x21);
  m.updateElementID=guid(0x21);
  m.updateElementTimestamp=b::DateTime{1700001000,nanoseconds};
  b::ListElement e{};
  e.listID=m.listID;
  e.elementID=guid(0x21);
  e.elementTimestamp={1700001000,nanoseconds};
  e.element={-0.25,25.0,voltage};
  v.ordered.push_back(e);
  v.generation=generation;
  return v;
}

int main(int argc,char** argv) {
  if(argc!=2) return 2;
  try {
    umaa::ComponentModule module(argv[1]); // actual ABI-v2 dynamic loader
    b::BatteryStatusComponentBridge bridge(module);
    const auto list=guid(0x11);
    const auto initial=stable(1,100,3.75);
    const auto updated=stable(2,101,4.25);
    check(!bridge.current_known(list),"unknown List incorrectly current");
    bridge.on_stable_list(initial,1);
    check(bridge.accepted()==1 && bridge.current_known(list),
          "first complete Battery business checkpoint rejected");
    bridge.on_list_invalidated(list,1);
    check(!bridge.current_known(list),
          "partial Cell update left stale business Host snapshot current");
    // A reconnect is unsafe with active business state: ABI v2 cannot
    // synthesize a DDS Dispose or inform the module of a partial update.
    bool denied=false;
    try {bridge.begin_reader_epoch(2);}
    catch(const std::logic_error&) {denied=true;}
    check(denied && !bridge.faulted() && !bridge.current_known(list),
          "reconnect incorrectly discarded unresolved Source state");
    bridge.on_stable_list(updated,1);
    check(bridge.accepted()==2 && bridge.current_known(list),
          "fresh full checkpoint did not restore known-current state");
    // A real subscription triggers Source Dispose only for native keyed
    // NOT_ALIVE_DISPOSED; the test injects an already authenticated event.
    bridge.on_source_disposed(initial.report.source,list,1);
    check(bridge.disposed()==1 && bridge.active_sources()==0 &&
          !bridge.current_known(list),
          "Dispose did not retire exact Source/List business state");
    denied=false;
    try {bridge.begin_reader_epoch(1);}
    catch(const std::logic_error&) {denied=true;}
    check(denied,"nonmonotonic Reader epoch accepted");
    bridge.begin_reader_epoch(2);
    check(!bridge.faulted() && !bridge.current_known(list),
          "clean Reader recreation retained invalid data");
    // The same genuine Source GUID is allowed in a wholly fresh Reader;
    // its Host ABI source token must be distinct from the prior epoch.
    bridge.on_stable_list(initial,2);
    check(bridge.accepted()==3 && bridge.current_known(list),
          "clean epoch failed to rebind original Source to fresh token");
    bridge.on_source_disposed(initial.report.source,list,2);
    check(bridge.disposed()==2 && !bridge.current_known(list),
          "second epoch failed to retire Source");
    denied=false;
    try {bridge.on_list_invalidated(list,1);}
    catch(const std::runtime_error&) {denied=true;}
    check(denied && bridge.faulted(),
          "callback from old Reader epoch was not fenced");
    check(module.stop(),"business DLL/SO cannot be safely unloaded");
    std::puts("UMAA_BATTERY_ABI_COMPONENT_EPOCH_RECOVERY_PASS");
    return 0;
  } catch(const std::exception& ex) {
    std::fprintf(stderr,"Battery Component Reader epoch test: %s\n",ex.what());
    return 3;
  }
}
