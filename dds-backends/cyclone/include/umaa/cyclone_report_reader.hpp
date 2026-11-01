// CycloneDDS-backed queryless Report subscription, delegating DDS reader
// lifetime and exception-safe sample cleanup to the shared SampleReader.
#pragma once
#include "umaa/cyclone_sample_reader.hpp"
#include "umaa/report_transport.hpp"
#include <cstdint>
#include <utility>

namespace umaa::cyclone {
template<class WireSample> class ReportReader final : public ReportSubscriptionTransport {
 public:
  ReportReader(dds_entity_t participant, dds_entity_t topic,
               const dds_topic_descriptor_t* descriptor, const dds_qos_t* qos)
      : reader_(participant, topic, descriptor, qos) {}
  ReportReader(const ReportReader&) = delete;
  ReportReader& operator=(const ReportReader&) = delete;

  void subscribe(std::uint64_t epoch) override { reader_.start(epoch); }
  void unsubscribe(std::uint64_t epoch) override { reader_.stop(epoch); }

  template<class Callback> bool poll(std::uint64_t epoch, Callback&& callback) {
    return reader_.poll(epoch, std::forward<Callback>(callback));
  }
  dds_entity_t entity() const { return reader_.entity(); }
  std::uint64_t active_epoch() const { return reader_.active_epoch(); }
  bool faulted() const { return reader_.faulted(); }
 private:
  SampleReader<WireSample> reader_;
};
} // namespace umaa::cyclone
