// Two independent genuine UMAA EO BatteryStatus Report Sources and two
// Collection listIDs, carried on the SAME original Cyclone DDS Topic pair.
// This is a scoped on-wire event-routing test, not native DDS coherence,
// full Component QoS, reconnect replay, or an UMAA conformance claim.
#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include "umaa/official_battery_collection_router.hpp"
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

static void require(dds_return_t rc, const char* action) {
  if (rc < 0) throw std::runtime_error(
      std::string(action) + ": " + dds_strretcode(-rc));
}
static host::Guid guid(unsigned char prefix) {
  host::Guid out{}; out[0] = prefix; return out;
}
// Verify the *effective DDS entity QoS*, not merely the requested qos_t.
// EO ICD 6.0 §3.8.1 reviews this minimum for Element Topic subscribers.
// Do not infer durability, GROUP coherence or an entire service QoS profile.
static bool has_ordered_topic_presentation(dds_entity_t entity) {
  std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> effective(
      dds_create_qos(), &dds_delete_qos);
  if (!effective) throw std::runtime_error("cannot query actual DDS QoS");
  require(dds_get_qos(entity, effective.get()), "get effective subscriber QoS");
  dds_presentation_access_scope_kind_t scope = DDS_PRESENTATION_INSTANCE;
  bool coherent = false, ordered = false;
  return dds_qget_presentation(effective.get(), &scope, &coherent, &ordered) &&
         scope == DDS_PRESENTATION_TOPIC && ordered;
}
using CellKey = std::pair<host::Guid, host::Guid>;

static bool known_cell(const CellKey& key) {
  if (key.first == guid(0x11))
    return key.second == guid(0x21) ||
           key.second == guid(0x22) ||
           key.second == guid(0x23);
  return key.first == guid(0x12) && key.second == guid(0x31);
}
static void verify_a(const host::StableList& state) {
  if (state.nativeDdsCoherenceProved || state.generation != 1 ||
      state.ordered.size() != 3 || state.report.source.id != guid(0x33) ||
      state.report.cellsListMetadata.listID != guid(0x11) ||
      state.report.cellsListMetadata.updateElementID != guid(0x23) ||
      !state.report.current || *state.report.current != -3.25)
    throw std::runtime_error("source A snapshot did not match original Report");
  const unsigned char ids[] = {0x21, 0x22, 0x23};
  const double volts[] = {3.75, 4.0, 4.25};
  for (std::size_t i = 0; i < 3; ++i)
    if (state.ordered[i].listID != guid(0x11) ||
        state.ordered[i].elementID != guid(ids[i]) ||
        state.ordered[i].element.voltage != volts[i])
      throw std::runtime_error("source A Cell payload/order corrupt");
}
static void verify_b(const host::StableList& state) {
  if (state.nativeDdsCoherenceProved || state.generation != 1 ||
      state.ordered.size() != 1 || state.report.source.id != guid(0x44) ||
      state.report.cellsListMetadata.listID != guid(0x12) ||
      state.report.cellsListMetadata.updateElementID != guid(0x31) ||
      !state.report.current || *state.report.current != -2.5 ||
      state.ordered[0].listID != guid(0x12) ||
      state.ordered[0].elementID != guid(0x31) ||
      state.ordered[0].element.voltage != 5.0 ||
      state.ordered[0].element.temperature != 27.0 ||
      state.ordered[0].element.current != -0.5)
    throw std::runtime_error("source B snapshot/payload corrupt");
}

int main(int argc, char** argv) {
  const bool negative =
      argc == 2 && std::string(argv[1]) == "--bad-checkpoint";
  const bool default_qos_negative =
      argc == 2 && std::string(argv[1]) == "--reject-default-qos";
  if (argc != 1 && !negative && !default_qos_negative) return 2;
  dds_entity_t p = 0;
  try {
    p = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    require(p, "create two-list DDS participant");
    if (default_qos_negative) {
      const dds_entity_t unmanaged = dds_create_subscriber(p, nullptr, nullptr);
      require(unmanaged, "create default QoS Subscriber for negative test");
      const bool approved = has_ordered_topic_presentation(unmanaged);
      require(dds_delete(p), "delete default QoS negative participant");
      p = 0;
      if (approved)
        throw std::runtime_error("default DDS Subscriber incorrectly met official Topic ordering");
      std::puts("BATTERY_OFFICIAL_ELEMENT_DEFAULT_QOS_REJECTED");
      return 0;
    }
    const dds_entity_t report_t = dds_create_topic(
        p, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
        "BatteryReportType", nullptr, nullptr);
    const dds_entity_t element_t = dds_create_topic(
        p, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
        "BatteryReportTypeCellsListElement", nullptr, nullptr);
    require(report_t, "create Report Topic");
    require(element_t, "create Element Topic");

    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> presentation(
        dds_create_qos(), &dds_delete_qos);
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> endpoint(
        dds_create_qos(), &dds_delete_qos);
    if (!presentation || !endpoint)
      throw std::runtime_error("DDS QoS allocation failure");
    // EO ICD 3.8.1 Element TOPIC ordered-access minimum; not full QoS.
    dds_qset_presentation(presentation.get(), DDS_PRESENTATION_TOPIC,
                          false, true);
    dds_qset_reliability(endpoint.get(), DDS_RELIABILITY_RELIABLE,
                         DDS_SECS(1));
    dds_qset_history(endpoint.get(), DDS_HISTORY_KEEP_LAST, 16);
    const dds_entity_t sub = dds_create_subscriber(
        p, presentation.get(), nullptr);
    require(sub, "create ordered subscriber");
    if (!has_ordered_topic_presentation(sub))
      throw std::runtime_error("actual DDS Subscriber lacks mandatory EO §3.8.1 Topic ordered Presentation");
    const dds_entity_t report_reader =
        dds_create_reader(sub, report_t, endpoint.get(), nullptr);
    const dds_entity_t element_reader =
        dds_create_reader(sub, element_t, endpoint.get(), nullptr);
    require(report_reader, "create Report reader");
    require(element_reader, "create Cell reader");

    host::LargeListRouter router(
        host::LargeListJoiner::ElementOrdering::CallerVerifiedTopicOrdered,
        2, 16, 4);
    std::map<dds_instance_handle_t, host::Identifier> reports_by_handle;
    std::map<dds_instance_handle_t, CellKey> cells_by_handle;
    std::set<host::Guid> report_data, report_disposed;
    std::set<CellKey> cell_data, cell_disposed;
    bool stable_a = false, stable_b = false;

    // We never manufacture a Dispose event. A callback is considered genuine
    // only after seeing NOT_ALIVE_DISPOSED and matching its native DDS handle
    // to a previously decoded, original-keyed data sample.
    for (int tick = 0; tick < 650 &&
         !(report_data.size() == 2 && report_disposed.size() == 2 &&
           cell_data.size() == 4 && cell_disposed.size() == 4); ++tick) {
      {
        codec::NativeReport sample{};
        void* samples[1] = {&sample};
        dds_sample_info_t info[1]{};
        const auto rc = dds_take(report_reader, samples, info, 1, 1);
        require(rc, "take native Report");
        if (rc > 0) {
          if (info[0].valid_data) {
            const auto value = codec::decode_report(sample);
            const auto source = value.source.id;
            const auto expected = source == guid(0x33) ? guid(0x11) :
                                  source == guid(0x44) ? guid(0x12) : guid(0);
            if (expected == guid(0) ||
                value.cellsListMetadata.listID != expected ||
                !report_data.insert(source).second)
              throw std::runtime_error("unexpected or duplicate Report source/listID");
            reports_by_handle.insert_or_assign(info[0].instance_handle,
                                               value.source);
            router.on_report(value);
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            const auto known = reports_by_handle.find(info[0].instance_handle);
            if (known == reports_by_handle.end() ||
                !report_disposed.insert(known->second.id).second)
              throw std::runtime_error("unknown or duplicate Report DDS Dispose");
          }
          dds_sample_free(&sample,
              &UMAA_EO_BatteryStatus_BatteryReportType_desc,
              DDS_FREE_CONTENTS);
        }
      }
      {
        codec::NativeElement sample{};
        void* samples[1] = {&sample};
        dds_sample_info_t info[1]{};
        const auto rc = dds_take(element_reader, samples, info, 1, 1);
        require(rc, "take native Cell");
        if (rc > 0) {
          if (info[0].valid_data) {
            const auto value = codec::decode_element(sample);
            const CellKey key{value.listID, value.elementID};
            if (!known_cell(key) || !cell_data.insert(key).second)
              throw std::runtime_error("unknown or duplicate Cell list/element Key");
            cells_by_handle.insert_or_assign(info[0].instance_handle, key);
            router.on_element(value);
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            const auto known = cells_by_handle.find(info[0].instance_handle);
            if (known == cells_by_handle.end() ||
                !cell_disposed.insert(known->second).second)
              throw std::runtime_error("unknown or duplicate genuine Cell Dispose");
            // The two distinct Topics have no relative delivery guarantee.
            // Apply the original Dispose only AFTER inspecting committed
            // snapshots, even if DDS delivered the tombstone first.
          }
          dds_sample_free(&sample,
              &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
              DDS_FREE_CONTENTS);
        }
      }

      if (!stable_a && router.latest_stable(guid(0x11))) {
        verify_a(*router.latest_stable(guid(0x11)));
        stable_a = true;
      }
      if (!stable_b && router.latest_stable(guid(0x12))) {
        verify_b(*router.latest_stable(guid(0x12)));
        stable_b = true;
      }
      if (!(report_data.size() == 2 && report_disposed.size() == 2 &&
            cell_data.size() == 4 && cell_disposed.size() == 4))
        dds_sleepfor(DDS_MSECS(20));
    }

    if (report_data.size() != 2 || report_disposed.size() != 2 ||
        cell_data.size() != 4 || cell_disposed.size() != 4 ||
        router.active_lists() != 2)
      throw std::runtime_error("incomplete two-list native DDS data/Dispose trace");
    if (!stable_a)
      throw std::runtime_error("valid first List was not independently stable");
    if (negative && stable_b)
      throw std::runtime_error("bad second List checkpoint was accepted");
    if (!negative && !stable_b)
      throw std::runtime_error("valid second List did not reach stable state");

    for (const auto& entry : cells_by_handle)
      router.on_dispose(entry.second.first, entry.second.second);
    router.reset_after_disconnect();
    if (router.active_lists() != 0 ||
        router.latest_stable(guid(0x11)) ||
        router.latest_stable(guid(0x12)))
      throw std::runtime_error("native Report Dispose left stale List snapshots");
    require(dds_delete(p), "delete two-list DDS participant");
    p = 0;

    if (negative) {
      std::puts("BATTERY_TWO_LIST_BAD_CHECKPOINT_REJECTED");
      return 10;
    }
    std::puts("BATTERY_TWO_LIST_NATIVE_ROUTER_PASS");
    return 0;
  } catch (const std::exception& exc) {
    std::fprintf(stderr, "original two-list Cyclone router: %s\n", exc.what());
    if (p > 0) dds_delete(p);
    return 3;
  }
}
