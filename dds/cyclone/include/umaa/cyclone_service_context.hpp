// Per-service CycloneDDS C entity owner, guarded by the reviewed ICD Topic
// catalog. This is a runtime binding prototype, NOT a normative QoS mapper.
// The official service Topic naming/QoS must be provided by the host.
#pragma once
#include <dds/dds.h>
#include "umaa/reviewed_service_registry.hpp"
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace umaa::cyclone {

class ServiceContext final {
 public:
  enum class ReviewMode { RequireVerifiedQoS, ExplicitPrototypeOnly };

  explicit ServiceContext(std::string service, ReviewMode review = ReviewMode::RequireVerifiedQoS)
      : service_(std::move(service)), mode_(review) {
    if (service_.empty()) throw std::invalid_argument("DDS service name is required");
    const auto created = dds_create_participant(DDS_DOMAIN_DEFAULT, nullptr, nullptr);
    check(created, "create service participant");
    participant_ = created;
  }
  ServiceContext(const ServiceContext&) = delete;
  ServiceContext& operator=(const ServiceContext&) = delete;
  ~ServiceContext() { if (participant_ > 0) (void)dds_delete(participant_); }

  dds_entity_t topic(std::string_view catalog_topic, std::string_view catalog_type,
                     reviewed::Role role, std::string wire_name,
                     const dds_topic_descriptor_t* type_support) {
    if (participant_ <= 0) throw std::logic_error("DDS participant already closed");
    // Check *before* calling any vendor side-effect: never make a DDS topic for
    // an unreviewed/wrong-direction service binding or guessed QoS.
    reviewed::require_topic(service_, catalog_topic, catalog_type, role,
                            mode_ == ReviewMode::RequireVerifiedQoS);
    if (!type_support || wire_name.empty())
      throw std::invalid_argument("DDS TypeSupport and Topic name are required");
    const auto result = dds_create_topic(participant_, type_support,
                                         wire_name.c_str(), nullptr, nullptr);
    check(result, "create service Topic");
    return result;
  }

  dds_entity_t participant() const { return participant_; }
  void close() {
    if (participant_ <= 0) return;
    check(dds_delete(participant_), "delete service participant");
    participant_ = 0;
  }

 private:
  static void check(dds_return_t result, const char* op) {
    if (result < 0)
      throw std::runtime_error(std::string(op) + " failed: " + dds_strretcode(-result));
  }
  std::string service_;
  ReviewMode mode_;
  dds_entity_t participant_{0};
};
} // namespace umaa::cyclone
