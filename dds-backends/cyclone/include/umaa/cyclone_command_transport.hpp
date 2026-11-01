// CycloneDDS-backed CommandStatus/AckReport writer, independent of a specific
// UMAA service. Every service must supply an explicitly reviewed wire mapper.
// Requires real CycloneDDS C at deployment; SDK unit tests inject an ABI stub.
#pragma once
#include <dds/dds.h>
#include "umaa/command_transport.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace umaa::cyclone {

// Mapper contract:
//   StatusSample make_status(const Identity&, const Event&) const;
//   AckSample make_ack(const Identity&, const Payload&) const;
// Status/Ack samples MUST carry the exact reviewed DDS instance key. The
// mapper owns deep-copy requirements for any dynamically allocated IDL fields.
// This class deliberately does not choose QoS, key mapping, or create DDS
// entities, all of which must be supplied by the host's reviewed service.
template<class Payload, class StatusSample, class AckSample, class Mapper>
class CommandWriter final : public umaa::CommandTransport<Payload> {
 public:
  CommandWriter(dds_entity_t status_writer, dds_entity_t ack_writer, Mapper mapper)
      : status_writer_(status_writer), ack_writer_(ack_writer), mapper_(std::move(mapper)) {
    if (status_writer_ <= 0 || ack_writer_ <= 0)
      throw std::invalid_argument("Command DDS writer entity invalid");
  }

  void publish_status(const Identity& id, const Event& event) override {
    StatusSample sample = mapper_.make_status(id, event);
    checked(dds_write(status_writer_, &sample), "write CommandStatus");
    // Keep the exact keyed instance for later Dispose, not an uninitialized
    // or cross-session sample. An allocation failure after write faults the
    // upstream BoundCommandProvider; it may not replay unknown delivery.
    last_status_.insert_or_assign(id.session, std::move(sample));
  }
  void publish_ack(const Identity& id, const Payload& payload) override {
    AckSample sample = mapper_.make_ack(id, payload);
    checked(dds_write(ack_writer_, &sample), "write CommandAck");
  }
  void dispose_status(const Identity& id) override {
    auto it = last_status_.find(id.session);
    if (it == last_status_.end())
      throw std::logic_error("CommandStatus has no published DDS instance");
    checked(dds_dispose(status_writer_, &it->second), "dispose CommandStatus");
    last_status_.erase(it);
  }
  void dispose_ack(const Identity& id, const Payload& payload) override {
    AckSample sample = mapper_.make_ack(id, payload);
    checked(dds_dispose(ack_writer_, &sample), "dispose CommandAck");
  }
  std::size_t published_status_instances() const { return last_status_.size(); }

 private:
  static void checked(dds_return_t result, const char* operation) {
    if (result < 0)
      throw std::runtime_error(std::string(operation) + " failed: " +
                               dds_strretcode(-result));
  }
  dds_entity_t status_writer_;
  dds_entity_t ack_writer_;
  Mapper mapper_;
  std::unordered_map<std::string, StatusSample> last_status_;
};
} // namespace umaa::cyclone
