// Actual DDS-subscription bridge for the scoped UMAA Battery Large List.
// Consumes real original Cyclone TypeSupport data and keyed Instance Dispose.
// NEVER assumes DDS ordering between Report and Cell Topics.
#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include "umaa/official_battery_collection.hpp"
#include "umaa/cyclone_official_battery_codec.hpp"

#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace host = umaa::official_battery;
namespace codec = umaa::cyclone::official_battery;

static void check(dds_return_t result, const char* step) {
  if (result < 0) throw std::runtime_error(std::string(step) + ": " + dds_strretcode(-result));
}

static host::Guid guid(unsigned char leading) {
  host::Guid v{}; v[0] = leading; return v;
}

static void check_stable(const host::StableList& snapshot) {
  if (snapshot.nativeDdsCoherenceProved || snapshot.ordered.size() != 1 ||
      snapshot.generation != 1 ||
      snapshot.ordered.front().listID != guid(0x11) ||
      snapshot.ordered.front().elementID != guid(0x22) ||
      snapshot.ordered.front().element.voltage != 3.75 ||
      !snapshot.report.current || *snapshot.report.current != -3.25 ||
      snapshot.report.source.id != guid(0x33) ||
      snapshot.report.cellsListMetadata.updateElementID != guid(0x22)) {
    throw std::runtime_error("official Battery joiner produced wrong native snapshot");
  }
}

int main(int argc, char** argv) {
  if (argc != 1 && !(argc == 2 && std::string(argv[1]) == "--bad-checkpoint")) return 2;
  const bool negative = argc == 2;
  dds_entity_t participant = 0;
  try {
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    check(participant, "create official Battery participant");
    const auto report_topic = dds_create_topic(
      participant, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
      "BatteryReportType", nullptr, nullptr);
    const auto element_topic = dds_create_topic(
      participant, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
      "BatteryReportTypeCellsListElement", nullptr, nullptr);
    check(report_topic, "create official Report Topic");
    check(element_topic, "create official Element Topic");

    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> topicq(
      dds_create_qos(), &dds_delete_qos);
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> endpointq(
      dds_create_qos(), &dds_delete_qos);
    if (!topicq || !endpointq) throw std::runtime_error("cannot allocate DDS QoS");
    // The original Collection Element subscription minimum (EO ICD 3.8.1).
    dds_qset_presentation(topicq.get(), DDS_PRESENTATION_TOPIC, false, true);
    dds_qset_reliability(endpointq.get(), DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(endpointq.get(), DDS_HISTORY_KEEP_LAST, 16);
    const auto subscriber = dds_create_subscriber(participant, topicq.get(), nullptr);
    check(subscriber, "create TOPIC-ordered subscriber");
    const auto report_reader = dds_create_reader(
      subscriber, report_topic, endpointq.get(), nullptr);
    const auto element_reader = dds_create_reader(
      subscriber, element_topic, endpointq.get(), nullptr);
    check(report_reader, "create official Report reader");
    check(element_reader, "create official Element reader");

    host::LargeListJoiner joiner(
      host::LargeListJoiner::ElementOrdering::CallerVerifiedTopicOrdered, 16, 4);
    std::map<dds_instance_handle_t, host::Identifier> report_keys;
    std::map<dds_instance_handle_t, std::pair<host::Guid, host::Guid>> element_keys;
    bool report_data = false, element_data = false;
    bool report_disposed = false, element_disposed = false, saw_stable = false;

    for (int i = 0; i < 450 &&
        !(report_data && element_data && report_disposed && element_disposed); ++i) {
      {
        codec::NativeReport sample{};
        void* samples[1] = {&sample};
        dds_sample_info_t info[1]{};
        const dds_return_t rc = dds_take(report_reader, samples, info, 1, 1);
        check(rc, "take official Report");
        if (rc > 0) {
          if (info[0].valid_data) {
            if (report_data) throw std::runtime_error("duplicate official Report data");
            const auto decoded = codec::decode_report(sample);
            report_keys.insert_or_assign(info[0].instance_handle, decoded.source);
            joiner.on_report(decoded);
            report_data = true;
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            const auto known = report_keys.find(info[0].instance_handle);
            if (!report_data || known == report_keys.end() ||
                known->second.id != guid(0x33))
              throw std::runtime_error("unrecognized Battery Report DDS Dispose");
            // Report and Cell Topics have NO relative delivery ordering.
            // Retain the pending host cycle until both Topics' data and
            // terminal disposals have arrived, then invalidate it. Resetting
            // here could erase metadata before a delayed Cell sample arrives.
            report_disposed = true;
          }
          dds_sample_free(&sample,
              &UMAA_EO_BatteryStatus_BatteryReportType_desc, DDS_FREE_CONTENTS);
        }
      }
      {
        codec::NativeElement sample{};
        void* samples[1] = {&sample};
        dds_sample_info_t info[1]{};
        const dds_return_t rc = dds_take(element_reader, samples, info, 1, 1);
        check(rc, "take official Cell");
        if (rc > 0) {
          if (info[0].valid_data) {
            if (element_data) throw std::runtime_error("duplicate official Cell data");
            const auto decoded = codec::decode_element(sample);
            element_keys.insert_or_assign(info[0].instance_handle,
                                          std::make_pair(decoded.listID, decoded.elementID));
            joiner.on_element(decoded);
            element_data = true;
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            const auto known = element_keys.find(info[0].instance_handle);
            if (!element_data || known == element_keys.end() ||
                known->second.first != guid(0x11) ||
                known->second.second != guid(0x22))
              throw std::runtime_error("unrecognized Battery Cell DDS Dispose");
            joiner.on_dispose(known->second.first, known->second.second);
            element_disposed = true;
          }
          dds_sample_free(&sample,
              &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
              DDS_FREE_CONTENTS);
        }
      }
      if (!saw_stable && joiner.latest_stable()) {
        check_stable(*joiner.latest_stable());
        saw_stable = true;
      }
      if (!(report_data && element_data && report_disposed && element_disposed))
        dds_sleepfor(DDS_MSECS(20));
    }

    if (!report_data || !element_data || !report_disposed || !element_disposed)
      throw std::runtime_error("incomplete real DDS Report/Cell data or Dispose events");
    // Actual keyed Report Dispose is the lifetime boundary, but in this
    // two-topic test the four required events must be observed first.
    joiner.reset_after_disconnect();
    if (joiner.latest_stable() || joiner.buffered_elements() || joiner.pending_reports())
      throw std::runtime_error("disposed Report left stale SDK Collection state");
    check(dds_delete(participant), "delete official Battery reader participant");
    participant = 0;

    if (negative) {
      if (saw_stable) throw std::runtime_error("inconsistent metadata checkpoint was accepted");
      std::puts("SDK_BATTERY_LIVE_CHECKPOINT_REJECTED");
      return 10;
    }
    if (!saw_stable) throw std::runtime_error("official Battery never reached stable snapshot");
    std::puts("SDK_BATTERY_LIVE_JOINER_PASS");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "official Battery live DDS joiner: %s\n", e.what());
    if (participant > 0) dds_delete(participant);
    return 3;
  }
}
