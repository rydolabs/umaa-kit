// Synthetic-only Component ABI-v2 Report Consumer dispatch and role fusion.
// No native DDS, no official keys/QoS, no normative UMAA Runtime claim.
#include "umaa/component.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

struct BatteryFixture {
  static constexpr const char* id = "Demo::EO::BatteryStatus";
  static constexpr const char* schema_id = "demo:battery-v1";
  struct Report { std::int32_t level_pct{}; };
  static bool decode_report(const std::uint8_t* p, std::size_t n, Report& report) {
    if (!p || n != sizeof(report.level_pct)) return false;
    std::memcpy(&report.level_pct, p, n);
    return true;
  }
  static std::vector<std::uint8_t> encode_report(const Report& report) {
    std::vector<std::uint8_t> bytes(sizeof(report.level_pct));
    std::memcpy(bytes.data(), &report.level_pct, bytes.size());
    return bytes;
  }
};
struct IncompatibleSchema : BatteryFixture {
  static constexpr const char* schema_id = "demo:incompatible";
};
struct Sink final : umaa::ReportSink<BatteryFixture> {
  int received{0};
  int disposed{0};
  bool throwing{false};
  umaa::Result on_report(const BatteryFixture::Report& value,
                         std::uint64_t token) override {
    if (throwing) throw std::runtime_error("simulated user Consumer exception");
    if (token != 55u || value.level_pct != 42)
      return umaa::Result::Rejected;
    ++received;
    return umaa::Result::Completed;
  }
  umaa::Result on_dispose(std::uint64_t token) override {
    if (token != 55u) return umaa::Result::Rejected;
    ++disposed;
    return umaa::Result::Completed;
  }
};
struct Provider final : umaa::ReportSource<BatteryFixture> {
  BatteryFixture::Report read() override { return {42}; }
};
struct BadProvider final : umaa::ReportSource<IncompatibleSchema> {
  IncompatibleSchema::Report read() override { return {99}; }
};
static void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
static std::int32_t perform(umaa::Component& component, std::uint32_t action,
                             const std::vector<std::uint8_t>& bytes,
                             std::uint64_t token,
                             std::vector<std::uint8_t>& output,
                             std::int32_t& business) {
  const UmaaComponentCallV2 call{BatteryFixture::id, action, 0,
      bytes.empty() ? nullptr : bytes.data(),
      static_cast<std::uint32_t>(bytes.size()), token};
  UmaaComponentResponseV2 response{output.data(),
      static_cast<std::uint32_t>(output.size()), 0, business};
  auto status = component.dispatch(&call, &response);
  business = response.business_result;
  output.resize(response.size);
  return status;
}

int main() {
  try {
    umaa::Component component;
    Sink sink;
    Provider provider;
    component.consume<BatteryFixture>(sink);
    component.provide<BatteryFixture>(provider);
    check(component.service_count() == 1, "Provider/Consumer became separate ABI services");
    UmaaComponentServiceV2 desc{};
    check(component.service_at(0, &desc) == UMAA_COMPONENT_OK &&
          std::string(desc.service_id) == BatteryFixture::id &&
          std::string(desc.schema_id) == BatteryFixture::schema_id &&
          desc.kinds == (4u | 8u | 16u),
          "fused ABI-v2 Report role action mask incorrect");

    // Duplicate registrations and incompatible schemas are rejected before
    // any user code can claim a matching Component service.
    try {
      component.consume<BatteryFixture>(sink);
      throw std::runtime_error("duplicate Consumer was accepted");
    } catch (const std::invalid_argument&) {}
    BadProvider incompatible;
    try {
      component.provide<IncompatibleSchema>(incompatible);
      throw std::runtime_error("incompatible duplicate service schema accepted");
    } catch (const std::invalid_argument&) {}

    component.start();
    const auto payload = BatteryFixture::encode_report({42});
    std::vector<std::uint8_t> output(16);
    std::int32_t business = -1;
    check(perform(component, UMAA_COMPONENT_REPORT_ACCEPT, payload, 55u,
                  output, business) == UMAA_COMPONENT_OK &&
          business == static_cast<std::int32_t>(umaa::Result::Completed) &&
          output.empty() && sink.received == 1,
          "synthetic inbound Report failed business Consumer delivery");

    output.resize(16);
    check(perform(component, UMAA_COMPONENT_REPORT_ACCEPT, {1,2}, 55u,
                  output, business) == UMAA_COMPONENT_INVALID &&
          sink.received == 1, "malformed Report reached Consumer");
    output.resize(16);
    check(perform(component, UMAA_COMPONENT_REPORT_ACCEPT, payload, 0u,
                  output, business) == UMAA_COMPONENT_INVALID &&
          sink.received == 1, "missing Source token reached Consumer");

    output.resize(16);
    check(perform(component, UMAA_COMPONENT_REPORT_DISPOSE, {}, 55u,
                  output, business) == UMAA_COMPONENT_OK &&
          business == static_cast<std::int32_t>(umaa::Result::Completed) &&
          output.empty() && sink.disposed == 1,
          "real business on_dispose not invoked");
    output.resize(16);
    check(perform(component, UMAA_COMPONENT_REPORT_DISPOSE, payload, 55u,
                  output, business) == UMAA_COMPONENT_INVALID &&
          sink.disposed == 1, "malformed Dispose payload accepted");

    output.resize(16);
    check(perform(component, UMAA_COMPONENT_REPORT, {}, 0u,
                  output, business) == UMAA_COMPONENT_OK &&
          business == static_cast<std::int32_t>(umaa::Result::Completed) &&
          output.size() == 4 &&
          output == BatteryFixture::encode_report({42}),
          "provider Report dispatch broken by Consumer role fusion");

    output.resize(16);
    sink.throwing = true;
    check(perform(component, UMAA_COMPONENT_REPORT_ACCEPT, payload, 55u,
                  output, business) == UMAA_COMPONENT_EXCEPTION &&
          sink.received == 1, "user exception created fake successful Report");

    component.stop();
    output.resize(16);
    check(perform(component, UMAA_COMPONENT_REPORT_ACCEPT, payload, 55u,
                  output, business) == UMAA_COMPONENT_NOT_ACTIVE &&
          sink.received == 1, "stopped module accepted a Consumer callback");

    // Reverse registration order must produce the same single descriptor.
    umaa::Component reverse;
    reverse.provide<BatteryFixture>(provider);
    reverse.consume<BatteryFixture>(sink);
    UmaaComponentServiceV2 other{};
    check(reverse.service_count() == 1 &&
          reverse.service_at(0, &other) == UMAA_COMPONENT_OK &&
          other.kinds == 28u, "reverse provider/consumer registration mismatch");

    std::puts("UMAA_SYNTHETIC_COMPONENT_REPORT_CONSUMER_ABI_V2_PASS");
    return 0;
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "Component Report Consumer regression: %s\n", ex.what());
    return 3;
  }
}
