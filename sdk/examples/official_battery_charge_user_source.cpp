// USER-OWNED TEST FIXTURE. Not generated, not official equipment behavior.
// The generated Component V2 owns only service communication plumbing.
#include "battery_charge_subset.hpp"
#include "umaa/component.hpp"
#include <cstdint>

namespace {
class VirtualBatteryCharge final:
    public umaa::ReportSource<umaa::generated::BatteryChargeSubset> {
 public:
  umaa::generated::BatteryChargeSubset::Report read() override {
    ++read_count_;
    return {read_count_==1?20.0:80.0};
  }
 private:
  std::uint32_t read_count_{0};
};
}
namespace umaa::generated {
umaa::ReportSource<BatteryChargeSubset>& external_battery_charge_source() {
  static VirtualBatteryCharge source;
  return source;
}
} // namespace umaa::generated
