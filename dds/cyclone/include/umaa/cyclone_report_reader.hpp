// CycloneDDS-backed queryless Report reader lifecycle.
// The host supplies reviewed Topic, TypeSupport and QoS. This adapter owns
// only the Reader; sample contents are valid solely during the callback.
#pragma once
#include <dds/dds.h>
#include "umaa/report_transport.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace umaa::cyclone {

template<class WireSample> class ReportReader final : public ReportSubscriptionTransport {
 public:
  ReportReader(dds_entity_t participant, dds_entity_t topic,
               const dds_topic_descriptor_t* descriptor, const dds_qos_t* qos)
      : participant_(participant), topic_(topic), descriptor_(descriptor), qos_(qos) {
    if (participant_ <= 0 || topic_ <= 0 || descriptor_ == nullptr || qos_ == nullptr)
      throw std::invalid_argument("Report Reader requires valid DDS entities, descriptor and reviewed QoS");
  }

  ReportReader(const ReportReader&) = delete;
  ReportReader& operator=(const ReportReader&) = delete;
  ~ReportReader() override {
    // A consumer may fault before unsubscribe. Best-effort release in that
    // case; destruction must not throw during error unwinding.
    if (reader_ > 0) (void)dds_delete(reader_);
  }

  void subscribe(std::uint64_t epoch) override {
    if (reader_ > 0) throw std::logic_error("Report reader already subscribed");
    if (epoch == 0) throw std::invalid_argument("Report reader epoch must be positive");
    const dds_entity_t created = dds_create_reader(participant_, topic_, qos_, nullptr);
    check(created, "create Report reader");
    reader_ = created;
    epoch_ = epoch;
  }

  void unsubscribe(std::uint64_t epoch) override {
    require_epoch(epoch);
    // A failed deletion has an unknown vendor outcome: preserve the reader
    // handle and reject further operations instead of silently re-subscribing.
    const dds_return_t result = dds_delete(reader_);
    if (result < 0) {
      faulted_ = true;
      check(result, "delete Report reader");
    }
    reader_ = 0;
    epoch_ = 0;
  }

  // Return false when no DDS event is pending. Explicitly check epoch before
  // take: queued callbacks from an old Reader must not consume new samples.
  // A callback exception still frees the DDS-owned dynamic sample contents.
  template<class Callback> bool poll(std::uint64_t epoch, Callback&& callback) {
    require_epoch(epoch);
    WireSample sample{};
    void* samples[1] = {&sample};
    dds_sample_info_t info[1]{};
    const dds_return_t count = dds_take(reader_, samples, info, 1, 1);
    if (count < 0) {
      faulted_ = true;
      check(count, "take Report sample");
    }
    if (count == 0) return false;
    struct SampleGuard {
      WireSample* sample;
      const dds_topic_descriptor_t* descriptor;
      ~SampleGuard() { dds_sample_free(sample, descriptor, DDS_FREE_CONTENTS); }
    } guard{&sample, descriptor_};
    try {
      callback(static_cast<const WireSample&>(sample),
               static_cast<const dds_sample_info_t&>(info[0]));
    } catch (...) {
      // The sample is irretrievably taken. The application may have applied
      // a partial update: never continue the same Reader epoch silently.
      faulted_ = true;
      throw;
    }
    return true;
  }

  dds_entity_t entity() const { return reader_; }
  std::uint64_t active_epoch() const { return epoch_; }
  bool faulted() const { return faulted_; }

 private:
  static void check(dds_return_t result, const char* operation) {
    if (result < 0)
      throw std::runtime_error(std::string(operation) + " failed: " + dds_strretcode(-result));
  }
  void require_epoch(std::uint64_t epoch) const {
    if (faulted_ || reader_ <= 0 || epoch == 0 || epoch != epoch_)
      throw std::logic_error("Report Reader inactive, faulted or stale epoch");
  }
  dds_entity_t participant_;
  dds_entity_t topic_;
  const dds_topic_descriptor_t* descriptor_;
  const dds_qos_t* qos_; // Borrowed: host retains it for the reader lifecycle.
  dds_entity_t reader_{0};
  std::uint64_t epoch_{0};
  bool faulted_{false};
};
} // namespace umaa::cyclone
