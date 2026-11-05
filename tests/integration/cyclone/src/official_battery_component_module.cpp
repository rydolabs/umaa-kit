// Business DLL/SO: zero DDS APIs. It consumes the strictly declared
// non-normative host-only Battery ABI preview, NOT original CDR TypeSupport.
#include "umaa/component.hpp"
#include "umaa/official_battery_component_bridge.hpp"
#include <cstdint>
#include <map>
#include <stdexcept>

namespace b=umaa::official_battery;
static b::Guid guid(std::uint8_t n){b::Guid g{};g[0]=n;return g;}
struct BusinessBattery final : umaa::ReportSink<b::BatteryStatusHostPreview> {
  std::map<std::uint64_t,b::Guid> active;
  std::size_t accepted{0},disposed{0};
  umaa::Result on_report(const b::StableList& value,std::uint64_t token) override {
    const auto& r=value.report;
    if(!token||value.generation!=1||value.nativeDdsCoherenceProved||
       r.state!=b::State::ON||r.source.parentID!=b::Guid{}||!r.current)
      return umaa::Result::Rejected;
    if(r.source.id==guid(0x33)){
      if(*r.current!=-3.25||r.cellsListMetadata.listID!=guid(0x11)||
         value.ordered.size()!=3)return umaa::Result::Rejected;
      const std::uint8_t ids[]={0x21,0x22,0x23};
      const double volts[]={3.75,4.0,4.25};
      for(std::size_t i=0;i<3;++i)
        if(value.ordered[i].elementID!=guid(ids[i])||
           value.ordered[i].element.voltage!=volts[i]||
           value.ordered[i].element.current!=-0.25||
           value.ordered[i].element.temperature!=26.0)
          return umaa::Result::Rejected;
    }else if(r.source.id==guid(0x44)){
      if(*r.current!=-2.5||r.cellsListMetadata.listID!=guid(0x12)||
         value.ordered.size()!=1||
         value.ordered[0].elementID!=guid(0x31)||
         value.ordered[0].element.voltage!=5.0||
         value.ordered[0].element.current!=-0.5||
         value.ordered[0].element.temperature!=27.0)
        return umaa::Result::Rejected;
    }else return umaa::Result::Rejected;
    if(!active.emplace(token,r.source.id).second)return umaa::Result::Rejected;
    ++accepted;
    return umaa::Result::Completed;
  }
  umaa::Result on_dispose(std::uint64_t token) override {
    if(active.erase(token)!=1)return umaa::Result::Rejected;
    ++disposed;
    return umaa::Result::Completed;
  }
};
static BusinessBattery battery;
static void register_handlers(umaa::Component& component){
  component.consume<b::BatteryStatusHostPreview>(battery);
}
UMAA_EXPORT_COMPONENT_V2("battery-host-preview-development-fixture",register_handlers)
