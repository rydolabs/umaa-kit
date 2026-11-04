// Cyclone DDS adapter for the explicitly reviewed, narrow original AUVSI
// UMAA 6.0 EO BatteryStatus Report + Large List Element vertical slice.
//
// Real native generated TypeSupport owns CDR/wire semantics. This adapter
// owns DDS Participant, Subscriber, both native DataReaders, actual native
// instance-handle-to-Key bookkeeping, and calls the vendor-neutral SDK
// BatteryStatusSubscription. A user writes only BatteryStatusHandler methods.
//
// Scope: NOT generic official Runtime generation, not full Component QoS,
// not proof of remote Publisher QoS, cross-Topic coherence, complete DDS
// discovery/replay after partitions, Fast DDS official parity or conformance.
// All callbacks/poll operations must be serialized by the caller.
#pragma once

#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include "umaa/official_battery_subscription.hpp"
#include "umaa/cyclone_official_battery_codec.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace umaa::cyclone::official_battery {

class OriginalBatteryStatusReader final {
 public:
  using ReaderEpoch = ::umaa::official_battery::ReaderEpoch;
  using Host = ::umaa::official_battery::BatteryStatusSubscription;

  struct Statistics {
    std::size_t report_samples{};
    std::size_t element_samples{};
    std::size_t report_disposes{};
    std::size_t element_disposes{};
  };

  explicit OriginalBatteryStatusReader(
      ::umaa::official_battery::BatteryStatusHandler& handler,
      dds_domainid_t domain = DDS_DOMAIN_DEFAULT,
      std::size_t max_lists = 16,
      std::size_t max_elements = 4096)
      : session_(handler,
          Host::ElementOrdering::CallerVerifiedTopicOrdered,
          max_lists, max_elements, 32),
        domain_(domain) {
    try { open(); }
    catch (...) { close(); throw; }
  }

  OriginalBatteryStatusReader(const OriginalBatteryStatusReader&) = delete;
  OriginalBatteryStatusReader& operator=(const OriginalBatteryStatusReader&) = delete;

  ~OriginalBatteryStatusReader() { close(); }

  // A single serialized SDK polling iteration. Genuine native generated
  // DDS DataReader samples are decoded, guarded by scoped sample cleanup,
  // routed by exact instance handles, then delivered to business Handler.
  // Any failure destroys reader entities and requires explicit restart.
  bool poll_once() {
    if (participant_ <= 0 || session_.state() != Host::State::Active)
      throw std::logic_error("official Battery reader not active");
    try {
      const bool element = read_element();
      const bool report = read_report();
      return element || report;
    } catch (...) {
      close();
      throw;
    }
  }

  // Explicit teardown/recreation, with a NEW callback epoch and empty state.
  // No claimed automatic DDS discovery recovery or stale sample replay.
  void restart() {
    close();
    try { open(); }
    catch (...) { close(); throw; }
  }

  void close() noexcept {
    session_.stop();
    source_keys_.clear();
    cell_keys_.clear();
    if (participant_ > 0) dds_delete(participant_);
    participant_ = 0;
    report_reader_ = 0;
    element_reader_ = 0;
  }

  ReaderEpoch epoch() const noexcept { return epoch_; }
  const Statistics& stats() const noexcept { return stats_; }
  std::size_t active_lists() const noexcept { return session_.active_lists(); }
  const ::umaa::official_battery::StableList* latest(
      const ::umaa::official_battery::Guid& id) const noexcept {
    return session_.latest(id);
  }

 private:
  using Guid = ::umaa::official_battery::Guid;
  using Identifier = ::umaa::official_battery::Identifier;
  using CellKey = std::pair<Guid, Guid>;

  static void require(dds_return_t result, const char* what) {
    if (result < 0)
      throw std::runtime_error(std::string(what) + ": " +
                               dds_strretcode(-result));
  }

  static void verify_native_ordering(dds_entity_t subscriber) {
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> actual(
        dds_create_qos(), &dds_delete_qos);
    if (!actual) throw std::runtime_error("cannot inspect official DDS Subscriber QoS");
    require(dds_get_qos(subscriber, actual.get()), "get effective Subscriber QoS");
    dds_presentation_access_scope_kind_t scope = DDS_PRESENTATION_INSTANCE;
    bool coherent = false, ordered = false;
    if (!dds_qget_presentation(actual.get(), &scope, &coherent, &ordered) ||
        scope != DDS_PRESENTATION_TOPIC || !ordered)
      throw std::runtime_error(
          "EO ICD 6.0 section 3.8.1 TOPIC ordered Presentation not effective");
  }

  struct NativeReportSample final {
    NativeReport data{};
    ~NativeReportSample() {
      dds_sample_free(&data,
                      &UMAA_EO_BatteryStatus_BatteryReportType_desc,
                      DDS_FREE_CONTENTS);
    }
  };
  struct NativeElementSample final {
    NativeElement data{};
    ~NativeElementSample() {
      dds_sample_free(&data,
          &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
          DDS_FREE_CONTENTS);
    }
  };

  void open() {
    participant_ = dds_create_participant(domain_, nullptr, nullptr);
    require(participant_, "create SDK official Battery Participant");
    const dds_entity_t report_topic = dds_create_topic(
        participant_, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
        "BatteryReportType", nullptr, nullptr);
    const dds_entity_t cell_topic = dds_create_topic(
        participant_, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
        "BatteryReportTypeCellsListElement", nullptr, nullptr);
    require(report_topic, "create official Battery Report Topic");
    require(cell_topic, "create official Battery Cell Topic");
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> presentation(
        dds_create_qos(), &dds_delete_qos);
    std::unique_ptr<dds_qos_t, decltype(&dds_delete_qos)> endpoint(
        dds_create_qos(), &dds_delete_qos);
    if (!presentation || !endpoint)
      throw std::runtime_error("allocate official Battery DDS QoS");
    // Only the TOPIC ordered requirement is source-reviewed normative EO
    // §3.8.1; RELIABLE and KEEP_LAST(16) remain explicit test parameters.
    dds_qset_presentation(presentation.get(), DDS_PRESENTATION_TOPIC,
                          false, true);
    dds_qset_reliability(endpoint.get(), DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
    dds_qset_history(endpoint.get(), DDS_HISTORY_KEEP_LAST, 16);
    const dds_entity_t sub = dds_create_subscriber(
        participant_, presentation.get(), nullptr);
    require(sub, "create official Topic ordered Subscriber");
    verify_native_ordering(sub);
    report_reader_ = dds_create_reader(
        sub, report_topic, endpoint.get(), nullptr);
    element_reader_ = dds_create_reader(
        sub, cell_topic, endpoint.get(), nullptr);
    require(report_reader_, "create generated original Report DataReader");
    require(element_reader_, "create generated original Cell DataReader");
    epoch_ = session_.start();
    stats_ = {};
  }

  bool read_element() {
    NativeElementSample holder;
    void* samples[1] = {&holder.data};
    dds_sample_info_t info[1]{};
    const auto rc = dds_take(element_reader_, samples, info, 1, 1);
    require(rc, "take original Cell DataReader");
    if (rc == 0) return false;
    if (info[0].valid_data) {
      const auto value = decode_element(holder.data);
      const CellKey keys{value.listID, value.elementID};
      auto found = cell_keys_.find(info[0].instance_handle);
      if (found != cell_keys_.end() && found->second != keys)
        throw std::runtime_error("DDS Cell instance handle changed its original Keys");
      session_.on_element(value, epoch_);
      cell_keys_.insert_or_assign(info[0].instance_handle, keys);
      ++stats_.element_samples;
    } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
      const auto found = cell_keys_.find(info[0].instance_handle);
      if (found == cell_keys_.end())
        throw std::runtime_error("unmatched genuine DDS Cell Dispose handle");
      session_.on_element_dispose(found->second.first, found->second.second, epoch_);
      cell_keys_.erase(found);
      ++stats_.element_disposes;
    } else {
      throw std::runtime_error("unknown Cell life state; no synthetic Dispose");
    }
    return true;
  }

  bool read_report() {
    NativeReportSample holder;
    void* samples[1] = {&holder.data};
    dds_sample_info_t info[1]{};
    const auto rc = dds_take(report_reader_, samples, info, 1, 1);
    require(rc, "take original Battery Report DataReader");
    if (rc == 0) return false;
    if (info[0].valid_data) {
      const auto value = decode_report(holder.data);
      auto found = source_keys_.find(info[0].instance_handle);
      if (found != source_keys_.end() &&
          (found->second.id != value.source.id ||
           found->second.parentID != value.source.parentID))
        throw std::runtime_error("DDS Report instance handle changed original Source Keys");
      session_.on_report(value, epoch_);
      source_keys_.insert_or_assign(info[0].instance_handle, value.source);
      ++stats_.report_samples;
    } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
      const auto found = source_keys_.find(info[0].instance_handle);
      if (found == source_keys_.end())
        throw std::runtime_error("unmatched genuine DDS Report Dispose handle");
      session_.on_source_dispose(found->second, epoch_);
      source_keys_.erase(found);
      ++stats_.report_disposes;
    } else {
      throw std::runtime_error("unknown Report life state; fail closed");
    }
    return true;
  }

  Host session_;
  dds_domainid_t domain_;
  dds_entity_t participant_{};
  dds_entity_t report_reader_{};
  dds_entity_t element_reader_{};
  ReaderEpoch epoch_{};
  Statistics stats_{};
  std::map<dds_instance_handle_t, Identifier> source_keys_;
  std::map<dds_instance_handle_t, CellKey> cell_keys_;
};

} // namespace umaa::cyclone::official_battery
