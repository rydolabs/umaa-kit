// Host-side DDS backend plugin loader. No DDS vendor header is included.
#pragma once
#include "umaa/internal/dds_backend.h"
#include "umaa/internal/native_library.hpp"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
namespace umaa {
class DdsBackendPlugin final {
public:
  explicit DdsBackendPlugin(const std::string& filename)
      : library_(std::make_unique<internal::NativeLibrary>(filename)) {
    auto fn = reinterpret_cast<UmaaGetDdsBackendV1>(library_->symbol(UMAA_DDS_BACKEND_ENTRY));
    if (!fn) throw std::runtime_error("DDS backend entry missing");
    api_ = fn(UMAA_DDS_ABI_VERSION);
    if (!api_ || api_->abi_version != UMAA_DDS_ABI_VERSION ||
        api_->struct_size != sizeof(UmaaDdsBackendV1) || !api_->vendor_name ||
        !*api_->vendor_name || !api_->open_domain || !api_->close_domain ||
        !api_->open_writer || !api_->open_reader || !api_->write || !api_->dispose ||
        !api_->unregister_instance || !api_->poll || !api_->wait_for_acks ||
        !api_->close_writer || !api_->close_reader)
      throw std::runtime_error("invalid DDS backend ABI/entry table");
  }
  DdsBackendPlugin(const DdsBackendPlugin&) = delete;
  DdsBackendPlugin& operator=(const DdsBackendPlugin&) = delete;
  std::string name() const { return api_->vendor_name; }
  uint64_t capabilities() const { return api_->capabilities; }
  class Domain final {
  public:
    Domain(const UmaaDdsBackendV1* api, uint32_t domain_id) : api_(api) {
      domain_ = api_->open_domain(domain_id);
      if (!domain_) throw std::runtime_error("DDS domain creation failed");
    }
    Domain(const Domain&) = delete;
    Domain& operator=(const Domain&) = delete;
    ~Domain() { api_->close_domain(domain_); }
    void* writer(const UmaaDdsTopicV1& topic) {
      validate(topic);
      void* w = api_->open_writer(domain_, &topic);
      if (!w) throw std::runtime_error("DDS writer creation failed (unknown TypeBinding or QoS)");
      return w;
    }
    void* reader(const UmaaDdsTopicV1& topic, UmaaDdsOnSampleV1 cb, void* ctx) {
      validate(topic);
      if (!cb) throw std::invalid_argument("DDS receive callback required");
      void* r = api_->open_reader(domain_, &topic, cb, ctx);
      if (!r) throw std::runtime_error("DDS reader creation failed (unknown TypeBinding or QoS)");
      return r;
    }
    int32_t write(void* w, const UmaaDdsSampleV1& sample) { return api_->write(w, &sample); }
    int32_t dispose(void* w, const UmaaDdsSampleV1& sample) { return api_->dispose(w, &sample); }
    int32_t unregister_instance(void* w, const UmaaDdsSampleV1& sample) { return api_->unregister_instance(w, &sample); }
    int32_t poll(void* r, uint32_t max_samples = 32) { return api_->poll(r, max_samples); }
    int32_t wait_for_acks(void* w, uint32_t timeout_ms) { return api_->wait_for_acks(w, timeout_ms); }
    void close_writer(void* w) { if (w) api_->close_writer(w); }
    void close_reader(void* r) { if (r) api_->close_reader(r); }
  private:
    void validate(const UmaaDdsTopicV1& t) const {
      if (!t.topic_name || !*t.topic_name || !t.schema_id || !*t.schema_id ||
          !t.qos_profile || !*t.qos_profile)
        throw std::invalid_argument("DDS topic requires explicit reviewed schema and QoS");
      if ((api_->capabilities & t.required_features) != t.required_features)
        throw std::runtime_error("DDS vendor lacks mandatory capability");
    }
    const UmaaDdsBackendV1* api_;
    void* domain_{nullptr};
  };
  // Domain and all endpoints MUST be destroyed before the owning plugin.
  std::unique_ptr<Domain> open_domain(uint32_t id) const {
    return std::make_unique<Domain>(api_, id);
  }
private:
  std::unique_ptr<internal::NativeLibrary> library_;
  const UmaaDdsBackendV1* api_{nullptr};
};
} // namespace umaa
