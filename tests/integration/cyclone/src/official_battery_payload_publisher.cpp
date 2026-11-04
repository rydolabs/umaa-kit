// Actual official Cyclone DDS TypeSupport / SDK-host value binding, selected EO
// BatteryStatus Report and separate CellsListElement. Prototype-specific QoS;
// NOT an official component definition, complete List protocol, or conformance.
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

static host::Guid guid(std::uint8_t first) {
  host::Guid result{};
  result[0] = first;
  return result;
}

static dds_entity_t make_topic(dds_entity_t participant,
                               const dds_topic_descriptor_t* description,
                               const char* topic_name) {
  const auto topic = dds_create_topic(participant, description, topic_name, nullptr, nullptr);
  require(topic, "create official Battery topic");
  return topic;
}

static void wait_for_reader(dds_entity_t writer) {
  for (int i = 0; i < 200; ++i) {
    dds_publication_matched_status_t status{};
    require(dds_get_publication_matched_status(writer, &status), "get matches");
    if (status.current_count >= 1) return;
    dds_sleepfor(DDS_MSECS(25));
  }
  throw std::runtime_error("independent Battery reader discovery timeout");
}

int main(int argc, char** argv) {
  // run.py uses an existing module argument; this typed value proof doesn't
  // invoke that unrelated synthetic business Handler.
  if (argc != 2 && argc != 3) return 2;
  const bool bad = argc == 3 && std::strcmp(argv[2], "--bad-checkpoint") == 0;
  if (argc == 3 && !bad) return 2;
  dds_entity_t participant = 0;
  try {
    const host::DateTime timestamp{1700000000, 250};
    host::Report h{};
    h.source.id = guid(0x33);
    h.source.parentID = guid(0);
    h.timeStamp = timestamp;
    h.state = host::State::ON;
    h.chargeRemaining = 80.0;
    h.current = -3.25;
    h.energyUsageRate = 120.5;
    h.hours = 8.0;
    h.temp = 25.0;
    h.voltage = 24.5;
    h.cellsListMetadata.listID = guid(0x11);
    h.cellsListMetadata.updateElementID = guid(0x22);
    h.cellsListMetadata.startingElementID = guid(0x22);
    h.cellsListMetadata.updateElementTimestamp =
        host::DateTime{1700000001, bad ? 111 : 110};
    h.cellsListMetadata.size = 1;

    host::ListElement element{};
    element.element.current = -0.25;
    element.element.temperature = 26.0;
    element.element.voltage = 3.75;
    element.listID = guid(0x11);
    element.elementID = guid(0x22);
    element.elementTimestamp = {1700000001, 110};

    // The official C TypeSupport is generated from the SHA-pinned original
    // AUVSI archive at CI time. No home-made CDR codec is permitted here.
    codec::ReportWire encoded(h);
    codec::ElementWire encoded_element(element);
    auto decoded = codec::decode_report(encoded.get());
    auto decoded_element = codec::decode_element(encoded_element.get());
    if (!decoded.current || *decoded.current != -3.25 ||
        decoded.source.id != h.source.id ||
        !decoded.cellsListMetadata.updateElementTimestamp ||
        decoded.cellsListMetadata.updateElementTimestamp->nanoseconds !=
          (bad ? 111 : 110) ||
        decoded_element.elementID != element.elementID ||
        decoded_element.element.voltage != 3.75 ||
        decoded_element.nextElementID.has_value()) {
      throw std::runtime_error("native-to-host Battery complex payload mismatch");
    }

    // Diagnostic only: log the original compiler-assigned DDS type name.
    std::fprintf(stderr, "CYCLONE_ORIGINAL_TYPES report=%s element=%s\n",
      UMAA_EO_BatteryStatus_BatteryReportType_desc.m_typename,
      UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc.m_typename);
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    require(participant, "create participant");
    const auto report_topic = make_topic(
        participant, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
        "BatteryReportType");
    const auto cell_topic = make_topic(
        participant, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
        "BatteryReportTypeCellsListElement");

    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> presentation(
        dds_create_qos(), &dds_delete_qos);
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> endpoint(
        dds_create_qos(), &dds_delete_qos);
    if (!presentation || !endpoint) throw std::runtime_error("DDS QoS allocation failed");
    // EO ICD §3.8.1 only mandates TOPIC/ordered Presentation on the
    // Collection ELEMENT publisher/subscriber, not the entire Report.
    dds_qset_presentation(presentation.get(), DDS_PRESENTATION_TOPIC, false, true);
    dds_qset_reliability(endpoint.get(), DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(endpoint.get(), DDS_HISTORY_KEEP_LAST, 16);
    // This prototype uses one publisher grouping both topics. It does not
    // claim cross-topic atomicity or apply an unreviewed GROUP policy.
    const auto publisher = dds_create_publisher(participant, presentation.get(), nullptr);
    require(publisher, "create Topic-ordered publisher");
    const auto report_writer = dds_create_writer(publisher, report_topic, endpoint.get(), nullptr);
    const auto cell_writer = dds_create_writer(publisher, cell_topic, endpoint.get(), nullptr);
    require(report_writer, "create official Report writer");
    require(cell_writer, "create official Cell Element writer");
    wait_for_reader(report_writer);
    wait_for_reader(cell_writer);

    // An element may arrive before/after the associated metadata on the
    // independent consumer. This publication order is NOT a DDS guarantee.
    require(dds_write(cell_writer, &encoded_element.get()), "write Cell Element");
    require(dds_write(report_writer, &encoded.get()), "write Battery Report");
    dds_sleepfor(DDS_MSECS(250));
    require(dds_dispose(cell_writer, &encoded_element.get()), "dispose Cell Element");
    require(dds_dispose(report_writer, &encoded.get()), "dispose Battery Report");
    require(dds_wait_for_acks(report_writer, DDS_SECS(5)), "wait Report ACKs");
    require(dds_wait_for_acks(cell_writer, DDS_SECS(5)), "wait Element ACKs");
    dds_sleepfor(DDS_MSECS(200));
    require(dds_delete(participant), "close Battery participant");
    participant = 0;
    std::puts(bad ? "BATTERY_NATIVE_PUBLISHER_BAD_CHECKPOINT" :
                    "BATTERY_NATIVE_PUBLISHER_TYPED_REPORT_CELL_DISPOSE_PASS");
    return 0;
  } catch (const std::exception& exc) {
    std::fprintf(stderr, "official Battery native codec publisher: %s\n", exc.what());
    if (participant > 0) dds_delete(participant);
    return 3;
  }
}
