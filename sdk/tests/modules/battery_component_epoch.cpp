// Business-only ABI-v2 DLL/SO fixture: native DDS not linked into the module.
// Host sends complete Battery snapshots; this Handler validates source-token
// continuity across updates and nonreuse across an explicit Reader epoch.
#include "umaa/component.hpp"
#include "umaa/official_battery_component_bridge.hpp"

#include <cstdint>

namespace b=umaa::official_battery;
static b::Guid guid(unsigned char c) { b::Guid g{}; g[0]=c; return g; }

struct BusinessSink final : umaa::ReportSink<b::BatteryStatusHostPreview> {
  std::uint64_t active_token{0}, previous_token{0};
  int accepted{0}, retired{0};

  umaa::Result on_report(const b::StableList& value,
                         std::uint64_t source_token) override {
    if(!source_token || value.nativeDdsCoherenceProved ||
       value.report.source.id!=guid(0x33) ||
       value.report.cellsListMetadata.listID!=guid(0x11) ||
       value.ordered.size()!=1 ||
       value.ordered[0].elementID!=guid(0x21))
      return umaa::Result::Rejected;

    if(accepted==0 && (value.generation!=1 ||
                       value.ordered[0].element.voltage!=3.75))
      return umaa::Result::Rejected;
    if(accepted==1 && (value.generation!=2 ||
                       value.ordered[0].element.voltage!=4.25 ||
                       source_token!=active_token))
      return umaa::Result::Rejected;
    if(accepted==2 && (value.generation!=1 ||
                       value.ordered[0].element.voltage!=3.75 ||
                       source_token==previous_token))
      return umaa::Result::Rejected;
    if(accepted>2 || (accepted==0 && active_token))
      return umaa::Result::Rejected;
    active_token=source_token;
    ++accepted;
    return umaa::Result::Completed;
  }

  umaa::Result on_dispose(std::uint64_t source_token) override {
    if(source_token!=active_token || !source_token ||
       (retired==0 && accepted!=2) ||
       (retired==1 && accepted!=3) || retired>1)
      return umaa::Result::Rejected;
    previous_token=source_token;
    active_token=0;
    ++retired;
    return umaa::Result::Completed;
  }
};

static BusinessSink sink;
static void register_handlers(umaa::Component& c) {
  c.consume<b::BatteryStatusHostPreview>(sink);
}
UMAA_EXPORT_COMPONENT_V2("battery-epoch-business-fixture",register_handlers)
