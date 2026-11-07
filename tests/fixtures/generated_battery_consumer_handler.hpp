#pragma once
// User-owned test Handler installed into a source-verified CLI-generated
// Component. No DDS headers or vendor calls are permitted here.
#include "service_binding.hpp"
#include <cstddef>
#include <cstdint>
#include <map>

namespace user {
namespace battery = ::umaa::official_battery;

class BatteryStatusConsumerLogic final
    : public ::umaa::ReportSink<generated::BatteryStatus> {
 public:
  ::umaa::Result on_report(const battery::StableList& value,
                           std::uint64_t token) override {
    const auto& r = value.report;
    if (!token || value.generation != 1 || value.nativeDdsCoherenceProved ||
        r.state != battery::State::ON || r.source.parentID != battery::Guid{} ||
        !r.current)
      return ::umaa::Result::Rejected;

    if (r.source.id == guid(0x33)) {
      if (*r.current != -3.25 ||
          r.cellsListMetadata.listID != guid(0x11) ||
          value.ordered.size() != 3)
        return ::umaa::Result::Rejected;
      const std::uint8_t ids[] = {0x21, 0x22, 0x23};
      const double voltages[] = {3.75, 4.0, 4.25};
      for (std::size_t i = 0; i < 3; ++i)
        if (value.ordered[i].elementID != guid(ids[i]) ||
            value.ordered[i].element.voltage != voltages[i] ||
            value.ordered[i].element.current != -0.25 ||
            value.ordered[i].element.temperature != 26.0)
          return ::umaa::Result::Rejected;
    } else if (r.source.id == guid(0x44)) {
      if (*r.current != -2.5 ||
          r.cellsListMetadata.listID != guid(0x12) ||
          value.ordered.size() != 1 ||
          value.ordered[0].elementID != guid(0x31) ||
          value.ordered[0].element.voltage != 5.0 ||
          value.ordered[0].element.current != -0.5 ||
          value.ordered[0].element.temperature != 27.0)
        return ::umaa::Result::Rejected;
    } else {
      return ::umaa::Result::Rejected;
    }

    if (!active_.emplace(token, r.source.id).second)
      return ::umaa::Result::Rejected;
    return ::umaa::Result::Completed;
  }

  ::umaa::Result on_dispose(std::uint64_t token) override {
    return active_.erase(token) == 1
        ? ::umaa::Result::Completed : ::umaa::Result::Rejected;
  }

 private:
  static battery::Guid guid(std::uint8_t id) {
    battery::Guid result{};
    result[0] = id;
    return result;
  }
  std::map<std::uint64_t, battery::Guid> active_;
};
}  // namespace user
