// Independent real original Cyclone TypeSupport -> vendor-neutral SDK Large
// List joiner. Three distinct keyed instances, 1 Report, 4 genuine Disposes.
// No handwritten CDR or manual fake Dispose callbacks are accepted.
#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include "umaa/official_battery_collection.hpp"
#include "umaa/cyclone_official_battery_codec.hpp"

#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace host = umaa::official_battery;
namespace codec = umaa::cyclone::official_battery;

static void require(dds_return_t rc, const char* op) {
  if (rc < 0) throw std::runtime_error(std::string(op) + ": " + dds_strretcode(-rc));
}
static host::Guid guid(unsigned char prefix) {
  host::Guid x{}; x[0] = prefix; return x;
}
static bool expected_id(const host::Guid& id) {
  return id == guid(0x21) || id == guid(0x22) || id == guid(0x23);
}
static void check_complete(const host::StableList& stable) {
  if (stable.nativeDdsCoherenceProved || stable.generation != 1 ||
      stable.ordered.size() != 3 || stable.report.source.id != guid(0x33) ||
      !stable.report.current || *stable.report.current != -3.25 ||
      stable.report.cellsListMetadata.updateElementID != guid(0x23) ||
      stable.report.cellsListMetadata.size != 3)
    throw std::runtime_error("incorrect stable three-Cell original Report");
  const unsigned char ids[] = {0x21, 0x22, 0x23};
  const double volts[] = {3.75, 4.0, 4.25};
  for (unsigned i = 0; i < 3; ++i)
    if (stable.ordered[i].listID != guid(0x11) ||
        stable.ordered[i].elementID != guid(ids[i]) ||
        stable.ordered[i].element.voltage != volts[i] ||
        stable.ordered[i].element.current != -0.25 ||
        stable.ordered[i].element.temperature != 26.0)
      throw std::runtime_error("three-Cell chain/order/payload mismatch");
}

int main(int argc, char** argv) {
  const bool negative = argc == 2 && std::string(argv[1]) == "--bad-checkpoint";
  if (argc != 1 && !negative) return 2;
  dds_entity_t p = 0;
  try {
    p = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    require(p, "DDS Participant");
    const dds_entity_t rtopic = dds_create_topic(
        p, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
        "BatteryReportType", nullptr, nullptr);
    const dds_entity_t ctopic = dds_create_topic(
        p, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
        "BatteryReportTypeCellsListElement", nullptr, nullptr);
    require(rtopic, "Report Topic"); require(ctopic, "Cell Topic");
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> pq(dds_create_qos(), &dds_delete_qos);
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> eq(dds_create_qos(), &dds_delete_qos);
    if (!pq || !eq) throw std::runtime_error("DDS QoS allocation failure");
    dds_qset_presentation(pq.get(), DDS_PRESENTATION_TOPIC, false, true);
    dds_qset_reliability(eq.get(), DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(eq.get(), DDS_HISTORY_KEEP_LAST, 16);
    const dds_entity_t subscriber = dds_create_subscriber(p, pq.get(), nullptr);
    require(subscriber, "ordered element subscriber");
    const dds_entity_t rr = dds_create_reader(subscriber, rtopic, eq.get(), nullptr);
    const dds_entity_t cr = dds_create_reader(subscriber, ctopic, eq.get(), nullptr);
    require(rr, "Report reader"); require(cr, "Cell reader");

    host::LargeListJoiner joiner(
        host::LargeListJoiner::ElementOrdering::CallerVerifiedTopicOrdered, 16, 4);
    std::map<dds_instance_handle_t, host::Identifier> report_handles;
    std::map<dds_instance_handle_t, std::pair<host::Guid,host::Guid>> cell_handles;
    std::set<host::Guid> data_ids, disposed_ids;
    bool report_data = false, report_disposed = false, stable = false;
    for (int iter=0; iter<500 &&
        !(report_data && report_disposed && data_ids.size()==3 && disposed_ids.size()==3);
        ++iter) {
      {
        codec::NativeReport sample{};
        void* samples[1] = {&sample};
        dds_sample_info_t info[1]{};
        const dds_return_t rc = dds_take(rr, samples, info, 1, 1);
        require(rc, "take original Report");
        if (rc > 0) {
          if (info[0].valid_data) {
            if (report_data) throw std::runtime_error("duplicate Report sample");
            const auto decoded = codec::decode_report(sample);
            if (decoded.source.id != guid(0x33))
              throw std::runtime_error("unexpected source IdentifierType Key");
            report_handles.insert_or_assign(info[0].instance_handle, decoded.source);
            joiner.on_report(decoded);
            report_data = true;
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            if (report_handles.find(info[0].instance_handle) == report_handles.end())
              throw std::runtime_error("unrecognized genuine Report Dispose key");
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
        const dds_return_t rc = dds_take(cr, samples, info, 1, 1);
        require(rc, "take original Cell");
        if (rc > 0) {
          if (info[0].valid_data) {
            const auto decoded = codec::decode_element(sample);
            if (decoded.listID != guid(0x11) || !expected_id(decoded.elementID) ||
                !data_ids.insert(decoded.elementID).second)
              throw std::runtime_error("unexpected/duplicate genuine Cell instance Key");
            cell_handles.insert_or_assign(info[0].instance_handle,
                                           std::make_pair(decoded.listID,decoded.elementID));
            joiner.on_element(decoded);
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            const auto known = cell_handles.find(info[0].instance_handle);
            if (known == cell_handles.end() ||
                !disposed_ids.insert(known->second.second).second)
              throw std::runtime_error("unknown/duplicate keyed Cell Dispose");
            // Keep the genuine tombstone keys until the Report checkpoint
            // has been assembled: the two Topics have no relative order.
          }
          dds_sample_free(&sample,
            &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
            DDS_FREE_CONTENTS);
        }
      }
      if (!stable && joiner.latest_stable()) {
        check_complete(*joiner.latest_stable());
        stable = true;
      }
      if (!(report_data && report_disposed && data_ids.size()==3 && disposed_ids.size()==3))
        dds_sleepfor(DDS_MSECS(20));
    }
    if (!report_data || !report_disposed || data_ids.size()!=3 || disposed_ids.size()!=3)
      throw std::runtime_error("missing real DDS multi-Cell data or keyed Dispose events");
    for (const auto& entry : cell_handles)
      joiner.on_dispose(entry.second.first, entry.second.second);
    joiner.reset_after_disconnect();
    if (joiner.latest_stable() || joiner.pending_reports() || joiner.buffered_elements())
      throw std::runtime_error("disposed source retained stale host Cell state");

    require(dds_delete(p), "delete Cyclone subscriber"); p = 0;
    if (negative) {
      if (stable) throw std::runtime_error("bad three-Cell checkpoint wrongly accepted");
      std::puts("BATTERY_THREE_CELL_BAD_CHECKPOINT_REJECTED");
      return 10;
    }
    if (!stable) throw std::runtime_error("three-Cell valid checkpoint never assembled");
    std::puts("BATTERY_THREE_CELL_NATIVE_JOINER_PASS");
    return 0;
  } catch(const std::exception& e) {
    std::fprintf(stderr, "real three-Cell Cyclone joiner: %s\n", e.what());
    if (p > 0) dds_delete(p);
    return 3;
  }
}
