// Shared, host-owned DDS session for independently constructed endpoints.
// V1 fallback remains a development-only path without explicit QoS profiles.
// Once a reader/writer is opened, configuration is frozen for this session.
// No official ServiceBinding/QoS approval is inferred from a configured profile.
#pragma once

#include "umaa/dds_backend_loader.hpp"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace umaa {
class DdsEndpointContext final {
 public:
  explicit DdsEndpointContext(const std::string& plugin, std::uint32_t domain_id)
      :backend_(std::make_shared<DdsBackendPlugin>(plugin)),
       domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
           backend_->open_domain(domain_id).release())) {}

  DdsEndpointContext(std::shared_ptr<DdsBackendPlugin> backend,
                     std::shared_ptr<DdsBackendPlugin::Domain> domain)
      :backend_(std::move(backend)), domain_(std::move(domain)) {
    if(!backend_ || !domain_)throw std::invalid_argument("DDS context requires backend and domain");
  }
  DdsEndpointContext(const DdsEndpointContext&)=delete;
  DdsEndpointContext& operator=(const DdsEndpointContext&)=delete;

  std::shared_ptr<DdsBackendPlugin> backend() const noexcept {return backend_;}
  std::shared_ptr<DdsBackendPlugin::Domain> domain() const noexcept {return domain_;}

  void set_qos_profiles(const std::unordered_map<std::string,qos::Policy>& profiles) {
    for(const auto& item:profiles)qos::validate(item.second);
    std::lock_guard<std::mutex> lock(mutex_);
    if(endpoint_started_)throw std::logic_error(
        "DDS QoS profiles must be configured before any endpoint is opened");
    profiles_=profiles;
  }

  void* writer(const UmaaDdsTopicV1& topic,
               const std::optional<qos::Requirements>& required=std::nullopt) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(profiles_.empty()) {
      if(required)throw std::runtime_error("mandatory official DDS QoS requires explicit profile");
      auto* result=domain_->writer(topic);
      endpoint_started_=true;
      return result;
    }
    const auto found=profiles_.find(topic.qos_profile ? topic.qos_profile : "");
    if(found==profiles_.end())throw std::runtime_error(
        "missing configured DDS QoS profile for Writer");
    // A configured QoS profile MUST use the V2 extension, never silently V1.
    auto* result=domain_->writer(topic,found->second,
                                 required.value_or(qos::Requirements{}));
    endpoint_started_=true;
    return result;
  }

  void* reader(const UmaaDdsTopicV1& topic,
               UmaaDdsOnSampleV1 callback,void* user,
               const std::optional<qos::Requirements>& required=std::nullopt) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(profiles_.empty()) {
      if(required)throw std::runtime_error("mandatory official DDS QoS requires explicit profile");
      auto* result=domain_->reader(topic,callback,user);
      endpoint_started_=true;
      return result;
    }
    const auto found=profiles_.find(topic.qos_profile ? topic.qos_profile : "");
    if(found==profiles_.end())throw std::runtime_error(
        "missing configured DDS QoS profile for Reader");
    auto* result=domain_->reader(topic,found->second,callback,user,
                                 required.value_or(qos::Requirements{}));
    endpoint_started_=true;
    return result;
  }

 private:
  // Destruction order: Domain first, then owning plugin, including for handles
  // that outlive Application/ComponentRuntime.
  std::shared_ptr<DdsBackendPlugin> backend_;
  std::shared_ptr<DdsBackendPlugin::Domain> domain_;
  std::mutex mutex_;
  bool endpoint_started_{false};
  std::unordered_map<std::string,qos::Policy> profiles_;
};
} // namespace umaa
