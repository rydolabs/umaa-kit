// Synthetic ABI v2 module for cross-DLL Provider/Consumer role routing.
// No DDS vendor dependency and no claim of official UMAA conformance.
#include "umaa/component.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
namespace demo {
struct BatteryStatus {
  static constexpr const char* id="Demo::EO::BatteryStatus";
#if defined(UMAA_TEST_BAD_SCHEMA)
  static constexpr const char* schema_id="demo:wrong-battery-v1";
#else
  static constexpr const char* schema_id="demo:battery-v1";
#endif
  struct Report {std::int32_t level_pct{};};
  static std::vector<std::uint8_t> encode_report(const Report& report) {
    std::vector<std::uint8_t> bytes(sizeof(report.level_pct));
    std::memcpy(bytes.data(),&report.level_pct,bytes.size());
    return bytes;
  }
  static bool decode_report(const std::uint8_t* bytes,std::size_t n,Report& out) {
    if(!bytes||n!=sizeof(out.level_pct))return false;
    std::memcpy(&out.level_pct,bytes,n);return true;
  }
};
#if defined(UMAA_TEST_PROVIDER_COLLISION)
class BatteryHandler final : public umaa::ReportSource<BatteryStatus> {
 public:
  BatteryStatus::Report read() override {return {12};}
};
#else
class BatteryHandler final : public umaa::ReportSink<BatteryStatus> {
 public:
  umaa::Result on_report(const BatteryStatus::Report& value,
                         std::uint64_t source) override {
#if defined(UMAA_TEST_CONSUMER_REJECT)
    (void)value;(void)source;
    return umaa::Result::Rejected;
#else
    return source!=0 && value.level_pct==80
      ?umaa::Result::Completed:umaa::Result::Rejected;
#endif
  }
  umaa::Result on_dispose(std::uint64_t source) override {
    return source!=0?umaa::Result::Completed:umaa::Result::Rejected;
  }
};
#endif
void register_services(umaa::Component& module) {
  static BatteryHandler handler;
#if defined(UMAA_TEST_PROVIDER_COLLISION)
  module.provide<BatteryStatus>(handler);
#else
  module.consume<BatteryStatus>(handler);
#endif
}
}
#if defined(UMAA_TEST_PROVIDER_COLLISION)
UMAA_EXPORT_COMPONENT_V2("battery-duplicate-provider",demo::register_services)
#elif defined(UMAA_TEST_BAD_SCHEMA)
UMAA_EXPORT_COMPONENT_V2("battery-wrong-schema-consumer",demo::register_services)
#elif defined(UMAA_TEST_CONSUMER_REJECT)
UMAA_EXPORT_COMPONENT_V2("battery-reject-consumer",demo::register_services)
#else
UMAA_EXPORT_COMPONENT_V2("battery-consumer",demo::register_services)
#endif
