// Scoped official UMAA BatteryStatus BoundReportProvider -> generated native
// Cyclone DDS C TypeSupport integration. NO authoritative Component QoS is
// inferred and the official Runtime generator's fail-closed gate is intact.
#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include "umaa/official_battery_host.hpp"
#include "umaa/cyclone_official_battery_codec.hpp"
#include "umaa/cyclone_report_writer.hpp"
#include "umaa/report_transport.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

namespace battery = umaa::official_battery;
namespace codec = umaa::cyclone::official_battery;

static void require(dds_return_t rc, const char* action) {
  if (rc < 0) throw std::runtime_error(std::string(action) + ": " + dds_strretcode(-rc));
}
static battery::Guid guid(unsigned char first) {
  battery::Guid id{}; id[0] = first; return id;
}

class BatteryHandler final : public umaa::ReportHandler<battery::Report> {
 public:
  explicit BatteryHandler(bool bad) {
    value_.source.id = guid(0x33);
    value_.source.parentID = guid(0);
    value_.timeStamp = {1700000000, 250};
    value_.state = battery::State::ON;
    value_.chargeRemaining = 80.0;
    value_.current = -3.25;
    value_.energyUsageRate = 120.5;
    value_.hours = 8.0;
    value_.temp = 25.0;
    value_.voltage = 24.5;
    auto& metadata = value_.cellsListMetadata;
    metadata.listID = guid(0x11);
    metadata.updateElementID = guid(0x22);
    metadata.startingElementID = guid(0x22);
    metadata.size = 1;
    metadata.updateElementTimestamp = battery::DateTime{1700000001, bad ? 111 : 110};
  }
  battery::Report read_report() const override { return value_; }
 private:
  battery::Report value_{};
};

// The mapper retains all optional backing memory until the synchronous DDS
// write completes; a shallow native struct copy never outlives its owner.
class BatteryMapper {
 public:
  BatteryMapper(std::string source, battery::Identifier expected)
      : source_(std::move(source)), expected_(expected) {}
  BatteryMapper(const BatteryMapper&) = delete;
  BatteryMapper& operator=(const BatteryMapper&) = delete;
  BatteryMapper(BatteryMapper&&) = default;
  BatteryMapper& operator=(BatteryMapper&&) = default;

  codec::NativeReport make_report(
      const umaa::ReportSample<battery::Report>& report) const {
    if (report.source != source_ ||
        report.data.source.id != expected_.id ||
        report.data.source.parentID != expected_.parentID)
      throw std::invalid_argument("official Battery Handler source identity drift");
    owned_ = std::make_unique<codec::ReportWire>(report.data);
    return owned_->get();
  }

  codec::NativeReport make_dispose(const std::string& source,
                                   std::int64_t time) const {
    if (source != source_ || time <= 0)
      throw std::invalid_argument("official Battery Dispose source mismatch");
    // The generated @key identity is the two-octet-array Identifier members.
    // Never substitute the old scalar demo/chargeRemaining as an instance Key.
    codec::NativeReport keyed{};
    codec::set_guid(keyed.source.id, expected_.id);
    codec::set_guid(keyed.source.parentID, expected_.parentID);
    return keyed;
  }
 private:
  std::string source_;
  battery::Identifier expected_{};
  mutable std::unique_ptr<codec::ReportWire> owned_;
};

static void await_reader(dds_entity_t writer) {
  for (int i = 0; i < 250; ++i) {
    dds_publication_matched_status_t matched{};
    require(dds_get_publication_matched_status(writer, &matched), "Cell discovery");
    if (matched.current_count > 0) return;
    dds_sleepfor(DDS_MSECS(20));
  }
  throw std::runtime_error("Cell reader did not discover real DDS instance");
}

int main(int argc, char** argv) {
  if (argc != 2 &&
      !(argc == 3 && std::strcmp(argv[2], "--bad-checkpoint") == 0)) return 2;
  const bool bad = argc == 3;
  dds_entity_t participant = 0;
  try {
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    require(participant, "create official Battery DDS participant");
    const auto report_topic = dds_create_topic(
      participant, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
      "BatteryReportType", nullptr, nullptr);
    const auto element_topic = dds_create_topic(
      participant, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
      "BatteryReportTypeCellsListElement", nullptr, nullptr);
    require(report_topic, "create report Topic");
    require(element_topic, "create element Topic");

    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> pq(
      dds_create_qos(), &dds_delete_qos);
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> eq(
      dds_create_qos(), &dds_delete_qos);
    if (!pq || !eq) throw std::runtime_error("cannot allocate prototype DDS QoS");
    // The collection-element Publisher/Subscriber must be TOPIC ordered.
    // RELIABLE and KEEP_LAST(16) are demo parameters, not normative QoS.
    dds_qset_presentation(pq.get(), DDS_PRESENTATION_TOPIC, false, true);
    dds_qset_reliability(eq.get(), DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(eq.get(), DDS_HISTORY_KEEP_LAST, 16);
    const auto publisher = dds_create_publisher(participant, pq.get(), nullptr);
    require(publisher, "create Topic-ordered publisher");
    const auto cell_writer = dds_create_writer(publisher, element_topic, eq.get(), nullptr);
    require(cell_writer, "create official Cell writer");

    BatteryHandler handler(bad);
    const auto first = handler.read_report();
    BatteryMapper mapper("battery-sdk-provider", first.source);
    umaa::cyclone::ReportWriter<battery::Report, codec::NativeReport, BatteryMapper>
        // DDS ReportWriter accepts an explicit Publisher parent as well as a
        // participant. Use the SAME TOPIC-ordered Publisher as the Cell writer;
        // the implicit default publisher is INSTANCE-scoped and would be
        // incompatible with the independent oracle TOPIC-level subscriber.
        transport("battery-sdk-provider", publisher, report_topic,
                  eq.get(), std::move(mapper), 1, 250, 250);
    umaa::BoundReportProvider<battery::Report> provider(
      "battery-sdk-provider", handler, transport);

    await_reader(cell_writer);
    provider.start(1);
    if (provider.state() != decltype(provider)::State::Active)
      throw std::runtime_error("SDK Battery Report provider did not activate");
    battery::ListElement host_cell{};
    host_cell.listID = guid(0x11);
    host_cell.elementID = guid(0x22);
    host_cell.elementTimestamp = {1700000001, 110};
    host_cell.element = {-0.25, 26.0, 3.75};
    codec::ElementWire cell(host_cell);
    require(dds_write(cell_writer, &cell.get()), "write official Battery Cell");
    dds_sleepfor(DDS_MSECS(300));
    require(dds_dispose(cell_writer, &cell.get()), "dispose official Battery Cell");
    provider.shutdown(3);
    if (provider.state() != decltype(provider)::State::Stopped ||
        transport.state() != decltype(transport)::State::Stopped)
      throw std::runtime_error("SDK official Battery provider failed shutdown");
    require(dds_wait_for_acks(cell_writer, DDS_SECS(5)), "wait Cell ACKs");
    dds_sleepfor(DDS_MSECS(200));
    require(dds_delete(participant), "delete DDS participant");
    participant = 0;
    std::puts(bad ? "SDK_BATTERY_BOUND_PROVIDER_BAD_CHECKPOINT" :
                    "SDK_BATTERY_BOUND_PROVIDER_TYPED_WIRE_PASS");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "SDK official Battery BoundReportProvider: %s\n", e.what());
    if (participant > 0) dds_delete(participant);
    return 3;
  }
}
