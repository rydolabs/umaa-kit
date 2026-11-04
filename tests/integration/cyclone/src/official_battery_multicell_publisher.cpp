// Real native original-IDL Cyclone DDS publisher for a THREE-cell Battery
// Large List snapshot, with a deliberately mismatched final checkpoint mode.
// No synthetic IDL, JSON bridge or handwritten DDS serialization.
#include <dds/dds.h>
#include "umaa/official_battery_host.hpp"
#include "umaa/cyclone_official_battery_codec.hpp"
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

namespace host = umaa::official_battery;
namespace codec = umaa::cyclone::official_battery;

static void require(dds_return_t rc, const char* action) {
  if (rc < 0) throw std::runtime_error(std::string(action) + ": " + dds_strretcode(-rc));
}
static host::Guid guid(unsigned char v) {
  host::Guid x{}; x[0] = v; return x;
}
static host::ListElement cell(unsigned char id, unsigned char next, int nanos, double voltage) {
  host::ListElement x{};
  x.listID = guid(0x11);
  x.elementID = guid(id);
  x.elementTimestamp = {1700001000, nanos};
  x.element.current = -0.25;
  x.element.temperature = 26.0;
  x.element.voltage = voltage;
  if (next) x.nextElementID = guid(next);
  return x;
}
static void wait_reader(dds_entity_t writer) {
  for (int i=0; i<250; ++i) {
    dds_publication_matched_status_t info{};
    require(dds_get_publication_matched_status(writer, &info), "get independent reader");
    if (info.current_count > 0) return;
    dds_sleepfor(DDS_MSECS(20));
  }
  throw std::runtime_error("three-Cell original reader discovery timed out");
}
int main(int argc, char** argv) {
  const bool bad = argc == 2 && std::strcmp(argv[1], "--bad-checkpoint") == 0;
  if (argc != 1 && !bad) return 2;
  dds_entity_t p = 0;
  try {
    host::Report sample{};
    sample.source.id = guid(0x33);
    sample.source.parentID = guid(0);
    sample.timeStamp = {1700000000, 250};
    sample.state = host::State::ON;
    sample.current = -3.25;
    auto& meta = sample.cellsListMetadata;
    meta.listID = guid(0x11);
    meta.updateElementID = guid(0x23);
    meta.startingElementID = guid(0x21);
    meta.size = 3;
    meta.updateElementTimestamp = {1700001000, bad ? 301 : 300};

    codec::ReportWire report(sample);
    codec::ElementWire first(cell(0x21, 0x22, 100, 3.75));
    codec::ElementWire second(cell(0x22, 0x23, 200, 4.0));
    codec::ElementWire third(cell(0x23, 0, 300, 4.25));

    p = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    require(p, "create participant");
    const dds_entity_t report_t = dds_create_topic(
       p, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
       "BatteryReportType", nullptr, nullptr);
    const dds_entity_t element_t = dds_create_topic(
       p, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
       "BatteryReportTypeCellsListElement", nullptr, nullptr);
    require(report_t, "report topic"); require(element_t, "element topic");
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> presentation(dds_create_qos(), &dds_delete_qos);
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> endpoints(dds_create_qos(), &dds_delete_qos);
    if (!presentation || !endpoints) throw std::runtime_error("QoS allocation failed");
    dds_qset_presentation(presentation.get(), DDS_PRESENTATION_TOPIC, false, true);
    dds_qset_reliability(endpoints.get(), DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(endpoints.get(), DDS_HISTORY_KEEP_LAST, 16);
    const dds_entity_t pub = dds_create_publisher(p, presentation.get(), nullptr);
    require(pub, "publisher");
    const dds_entity_t report_w = dds_create_writer(pub, report_t, endpoints.get(), nullptr);
    const dds_entity_t cell_w = dds_create_writer(pub, element_t, endpoints.get(), nullptr);
    require(report_w, "report writer"); require(cell_w, "cell writer");
    wait_reader(report_w); wait_reader(cell_w);

    // Three distinct keyed instances and one Report metadata checkpoint.
    // The consumer must not publish a stable result from an incomplete set.
    require(dds_write(cell_w, &first.get()), "first keyed Cell");
    require(dds_write(cell_w, &second.get()), "second keyed Cell");
    require(dds_write(cell_w, &third.get()), "third keyed Cell");
    require(dds_write(report_w, &report.get()), "three-Cell Report");
    dds_sleepfor(DDS_MSECS(700));

    require(dds_dispose(cell_w, &first.get()), "first Cell Dispose");
    require(dds_dispose(cell_w, &second.get()), "second Cell Dispose");
    require(dds_dispose(cell_w, &third.get()), "third Cell Dispose");
    require(dds_dispose(report_w, &report.get()), "Report Dispose");
    require(dds_wait_for_acks(cell_w, DDS_SECS(5)), "Cell ACK");
    require(dds_wait_for_acks(report_w, DDS_SECS(5)), "Report ACK");
    dds_sleepfor(DDS_MSECS(350));
    require(dds_delete(p), "delete participant");
    p = 0;
    std::puts(bad ? "BATTERY_THREE_CELL_BAD_CHECKPOINT_SENT" :
                    "BATTERY_THREE_CELL_ORIGINAL_WIRE_SENT");
    return 0;
  } catch(const std::exception& e) {
    std::fprintf(stderr, "official Battery three-cell publisher failed: %s\n", e.what());
    if (p > 0) dds_delete(p);
    return 3;
  }
}
