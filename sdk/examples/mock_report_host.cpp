#include "umaa/generated/BatteryStatus.hpp"
#include <cassert>
class Battery final : public umaa::generated::BatteryStatusHandler {
public:
  umaa::generated::BatteryStatusPayload read_report() const override {
    umaa::generated::BatteryStatusPayload report;
    report.levelPct = 80;
    return report;
  }
};
int main() {
  Battery user;
  umaa::generated::BatteryStatusProvider p("battery-a",user);
  umaa::generated::BatteryStatusConsumerCache c;
  auto sample = p.publish(123);
  assert(c.receive(sample));
  assert(c.latest("battery-a")->data.levelPct == 80);
  assert(!c.receive(sample));
  assert(c.dispose("battery-a"));
}
