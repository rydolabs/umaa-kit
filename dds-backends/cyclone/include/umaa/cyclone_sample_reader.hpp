// Reusable CycloneDDS C sample reader for Report subscriptions and incoming
// Command topics. Reviewed Topic/TypeSupport/QoS remain host responsibilities.
// Callbacks borrow a DDS sample only for the duration of poll(); do not retain
// dynamic pointers, and do not call start/stop concurrently with poll().
#pragma once
#include <dds/dds.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace umaa::cyclone {

template<class WireSample> class SampleReader final {
 public:
  SampleReader(dds_entity_t participant, dds_entity_t topic,
               const dds_topic_descriptor_t* descriptor, const dds_qos_t* qos)
      : participant_(participant), topic_(topic), descriptor_(descriptor), qos_(qos) {
    if (participant_ <= 0 || topic_ <= 0 || descriptor_ == nullptr || qos_ == nullptr)
      throw std::invalid_argument("DDS Reader requires valid entities, descriptor and reviewed QoS");
  }
  SampleReader(const SampleReader&) = delete;
  SampleReader& operator=(const SampleReader&) = delete;
  ~SampleReader() {
    if (reader_ > 0) (void)dds_delete(reader_);
  }

  void start(std::uint64_t epoch) {
    if (faulted_) throw std::logic_error("DDS Reader faulted; reconstruct host");
    if (reader_ > 0) throw std::logic_error("DDS Reader already active");
    if (epoch == 0) throw std::invalid_argument("DDS Reader epoch must be positive");
    const dds_entity_t created = dds_create_reader(participant_, topic_, qos_, nullptr);
    checked(created, "create DDS reader");
    reader_ = created;
    epoch_ = epoch;
  }

  void stop(std::uint64_t epoch) {
    require(epoch);
    const dds_return_t result = dds_delete(reader_);
    if (result < 0) {
      faulted_ = true;
      checked(result, "delete DDS reader");
    }
    reader_ = 0;
    epoch_ = 0;
  }

  template<class Callback> bool poll(std::uint64_t epoch, Callback&& callback) {
    require(epoch);  // never consume from an expired subscription generation
    WireSample sample{};
    void* samples[1] = {&sample};
    dds_sample_info_t info[1]{};
    const dds_return_t count = dds_take(reader_, samples, info, 1, 1);
    if (count < 0) {
      faulted_ = true;
      checked(count, "take DDS sample");
    }
    if (count == 0) return false;
    struct SampleGuard {
      WireSample* sample;
      const dds_topic_descriptor_t* descriptor;
      ~SampleGuard() { dds_sample_free(sample, descriptor, DDS_FREE_CONTENTS); }
    } guard{&sample, descriptor_};
    try {
      std::forward<Callback>(callback)(static_cast<const WireSample&>(sample),
                                       static_cast<const dds_sample_info_t&>(info[0]));
    } catch (...) {
      // Application may have committed part of the event. Never retry/continue
      // the same epoch when its on-wire effect is unknown.
      faulted_ = true;
      throw;
    }
    return true;
  }

  dds_entity_t entity() const { return reader_; }
  std::uint64_t active_epoch() const { return epoch_; }
  bool faulted() const { return faulted_; }

 private:
  static void checked(dds_return_t result, const char* operation) {
    if (result < 0)
      throw std::runtime_error(std::string(operation) + " failed: " + dds_strretcode(-result));
  }
  void require(std::uint64_t epoch) const {
    if (faulted_ || reader_ <= 0 || epoch == 0 || epoch != epoch_)
      throw std::logic_error("DDS Reader inactive, faulted or stale epoch");
  }
  dds_entity_t participant_;
  dds_entity_t topic_;
  const dds_topic_descriptor_t* descriptor_;
  const dds_qos_t* qos_; // Borrowed; host retains it at least through start().
  dds_entity_t reader_{0};
  std::uint64_t epoch_{0};
  bool faulted_{false};
};
} // namespace umaa::cyclone
