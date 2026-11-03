// Real original UMAA 6.0 TypeSupport; SDK host values -> Fast DDS wire.
// Standalone prototype with explicit QoS; NOT official component conformance.
#include "umaa/fastdds_official_battery_codec.hpp"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/core/status/PublicationMatchedStatus.hpp>
#include <fastdds/dds/core/Time_t.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

namespace fd = eprosima::fastdds::dds;
namespace host = umaa::official_battery;
namespace codec = umaa::fastdds::official_battery;

static host::Guid guid(uint8_t first) {
  host::Guid g{}; g[0] = first; return g;
}

static void ok(fd::ReturnCode_t result, const char* action) {
  if (result != fd::RETCODE_OK)
    throw std::runtime_error(std::string(action) + " failed");
}

static void wait_match(fd::DataWriter* writer) {
  for (int i = 0; i < 250; ++i) {
    fd::PublicationMatchedStatus status{};
    ok(writer->get_publication_matched_status(status), "get matches");
    if (status.current_count >= 1) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  throw std::runtime_error("official Fast DDS reader discovery timed out");
}

int main(int argc, char** argv) {
  if (argc != 1 && !(argc == 2 &&
      std::strcmp(argv[1], "--bad-checkpoint") == 0)) return 2;
  const bool bad = argc == 2;
  auto* factory = fd::DomainParticipantFactory::get_instance();
  fd::DomainParticipant* participant = nullptr;
  try {
    host::Report h{};
    h.source.id = guid(0x33); h.source.parentID = guid(0);
    h.timeStamp = {1700000000, 250}; h.state = host::State::ON;
    h.chargeRemaining = 80.0; h.current = -3.25;
    h.energyUsageRate = 120.5; h.hours = 8.0;
    h.temp = 25.0; h.voltage = 24.5;
    h.cellsListMetadata.listID = guid(0x11);
    h.cellsListMetadata.updateElementID = guid(0x22);
    h.cellsListMetadata.startingElementID = guid(0x22);
    h.cellsListMetadata.updateElementTimestamp = {1700000001, bad ? 111 : 110};
    h.cellsListMetadata.size = 1;

    host::ListElement cell{};
    cell.element = {-0.25, 26.0, 3.75};
    cell.listID = guid(0x11); cell.elementID = guid(0x22);
    cell.elementTimestamp = {1700000001, 110};

    codec::NativeReport report = codec::encode_report(h);
    codec::NativeElement element = codec::encode_element(cell);
    const auto checked_report = codec::decode_report(report);
    const auto checked_element = codec::decode_element(element);
    if (!checked_report.current || *checked_report.current != -3.25 ||
        checked_report.source.id != h.source.id ||
        !checked_report.cellsListMetadata.updateElementTimestamp ||
        checked_report.cellsListMetadata.updateElementTimestamp->nanoseconds != (bad ? 111 : 110) ||
        checked_element.element.voltage != 3.75 ||
        checked_element.elementID != cell.elementID || checked_element.nextElementID)
      throw std::runtime_error("official Fast DDS codec local host round trip failed");

    participant = factory->create_participant(0, fd::PARTICIPANT_QOS_DEFAULT);
    if (!participant) throw std::runtime_error("participant creation failed");
    fd::TypeSupport report_type(new codec::ReportTypeSupport());
    fd::TypeSupport element_type(new codec::ElementTypeSupport());
    ok(report_type.register_type(participant), "register report");
    ok(element_type.register_type(participant), "register element");
    auto* report_topic = participant->create_topic(
      "BatteryReportType", report_type.get_type_name(), fd::TOPIC_QOS_DEFAULT);
    auto* element_topic = participant->create_topic(
      "BatteryReportTypeCellsListElement", element_type.get_type_name(), fd::TOPIC_QOS_DEFAULT);
    if (!report_topic || !element_topic) throw std::runtime_error("create official Topics failed");
    auto pubq = fd::PUBLISHER_QOS_DEFAULT;
    pubq.presentation().access_scope = fd::TOPIC_PRESENTATION_QOS;
    pubq.presentation().ordered_access = true;
    auto* publisher = participant->create_publisher(pubq);
    if (!publisher) throw std::runtime_error("create publisher failed");

    auto writerq = fd::DATAWRITER_QOS_DEFAULT;
    writerq.reliability().kind = fd::RELIABLE_RELIABILITY_QOS;
    writerq.history().kind = fd::KEEP_LAST_HISTORY_QOS;
    writerq.history().depth = 16;
    writerq.writer_data_lifecycle().autodispose_unregistered_instances = false;
    auto* report_writer = publisher->create_datawriter(report_topic, writerq);
    auto* element_writer = publisher->create_datawriter(element_topic, writerq);
    if (!report_writer || !element_writer) throw std::runtime_error("create writers failed");
    wait_match(report_writer);
    wait_match(element_writer);
    // Separate Topics: order below is NOT a cross-topic DDS ordering claim.
    ok(element_writer->write(&element), "write Cell");
    ok(report_writer->write(&report), "write Report");
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    ok(element_writer->dispose(&element, fd::HANDLE_NIL), "dispose Cell");
    ok(report_writer->dispose(&report, fd::HANDLE_NIL), "dispose Report");
    ok(element_writer->wait_for_acknowledgments(fd::Duration_t(5, 0)),
       "wait Cell ACK");
    ok(report_writer->wait_for_acknowledgments(fd::Duration_t(5, 0)),
       "wait Report ACK");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    ok(participant->delete_contained_entities(), "delete DDS entities");
    ok(factory->delete_participant(participant), "delete participant");
    participant = nullptr;
    std::puts(bad ? "FAST_BATTERY_BAD_CHECKPOINT_SENT" :
                    "FAST_BATTERY_REPORT_CELL_DISPOSE_SENT");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "official Fast DDS Battery publisher: %s\n", e.what());
    if (participant) {
      (void)participant->delete_contained_entities();
      (void)factory->delete_participant(participant);
    }
    return 3;
  }
}
