// CycloneDDS queryless Report writer, independent of concrete UMAA wire type.
// Service hosts supply reviewed TypeSupport/QoS and a mapper with stable sample
// memory. Never infer service-specific Topic keys or profile from a type name.
#pragma once
#include <dds/dds.h>
#include "umaa/report_transport.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace umaa::cyclone {

template<class Value, class WireSample, class Mapper>
class ReportWriter final : public ReportTransport<Value> {
 public:
  enum class State { Idle, Active, Stopped, Faulted };
  ReportWriter(std::string source, dds_entity_t participant, dds_entity_t topic,
               const dds_qos_t* qos, Mapper mapper, int expected_readers = 0,
               int discovery_attempts = 200,
               std::uint32_t retire_grace_ms = 0)
      : source_(std::move(source)), participant_(participant), topic_(topic),
        qos_(qos), mapper_(std::move(mapper)), expected_readers_(expected_readers),
        discovery_attempts_(discovery_attempts),
        retire_grace_ms_(retire_grace_ms) {
    if (source_.empty() || participant_ <= 0 || topic_ <= 0 || !qos_ ||
        expected_readers_ < 0 || discovery_attempts_ <= 0 ||
        retire_grace_ms_ > 5000)
      throw std::invalid_argument("Report Writer requires explicit source, entities and QoS");
  }
  ReportWriter(const ReportWriter&) = delete;
  ReportWriter& operator=(const ReportWriter&) = delete;
  ~ReportWriter() override {
    if (writer_ > 0) (void)dds_delete(writer_); // best-effort after fault
  }

  void register_source(const std::string& source) override {
    require(source, State::Idle);
    try {
      const dds_entity_t writer = dds_create_writer(participant_, topic_, qos_, nullptr);
      check(writer, "create Report writer");
      writer_ = writer;
      // Publisher startup blocks until the requested number of independent
      // readers is discovered. This is not a service-compliance QoS assertion.
      for (int attempt = 0; expected_readers_ && attempt < discovery_attempts_; ++attempt) {
        dds_publication_matched_status_t matched{};
        check(dds_get_publication_matched_status(writer_, &matched), "Report reader discovery");
        if (matched.current_count >= expected_readers_) {
          state_ = State::Active;
          return;
        }
        dds_sleepfor(DDS_MSECS(25));
      }
      if (expected_readers_) throw std::runtime_error("Report reader discovery timed out");
      state_ = State::Active;
    } catch (...) { state_ = State::Faulted; throw; }
  }
  void publish(const ReportSample<Value>& report) override {
    require(report.source, State::Active);
    try {
      WireSample sample = mapper_.make_report(report);
      check(dds_write(writer_, &sample), "write Report");
    } catch (...) { state_ = State::Faulted; throw; }
  }
  void dispose(const std::string& source, std::int64_t timestamp) override {
    require(source, State::Active);
    if (timestamp <= 0) throw std::invalid_argument("Dispose timestamp must be positive");
    try {
      WireSample sample = mapper_.make_dispose(source, timestamp);
      check(dds_dispose(writer_, &sample), "dispose Report");
    } catch (...) { state_ = State::Faulted; throw; }
  }
  void unregister_source(const std::string& source) override {
    require(source, State::Active);
    try {
      // Reliable middleware ACK: wait for prior Report/Dispose deliveries
      // before deleting the writer, not an arbitrary sleep.
      check(dds_wait_for_acks(writer_, DDS_SECS(5)), "wait for Report ACKs");
      // A DDS ACK proves Writer-history delivery but not the application's
      // observation of the final Dispose. Under explicit network-impairment
      // testing, keep the Writer alive for the configured late-repair window.
      // This is NOT a normative UMAA DDS QoS policy or fabricated app ACK.
      if (retire_grace_ms_)
        dds_sleepfor(DDS_MSECS(retire_grace_ms_));
      check(dds_delete(writer_), "delete Report writer");
      writer_ = 0;
      state_ = State::Stopped;
    } catch (...) { state_ = State::Faulted; throw; }
  }
  State state() const { return state_; }
  dds_entity_t entity() const { return writer_; }

 private:
  void require(const std::string& source, State required) const {
    if (source != source_ || state_ != required)
      throw std::logic_error("Report source identity/lifecycle mismatch or fault");
  }
  static void check(dds_return_t result, const char* operation) {
    if (result < 0)
      throw std::runtime_error(std::string(operation) + " failed: " + dds_strretcode(-result));
  }
  std::string source_;
  dds_entity_t participant_;
  dds_entity_t topic_;
  const dds_qos_t* qos_; // Borrowed from the host; retained during lifecycle.
  Mapper mapper_;
  int expected_readers_;
  int discovery_attempts_;
  std::uint32_t retire_grace_ms_{0};
  dds_entity_t writer_{0};
  State state_{State::Idle};
};
} // namespace umaa::cyclone
