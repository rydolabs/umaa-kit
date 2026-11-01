// Host-side DDS backend plugin loader. No DDS vendor header is included.
#pragma once
#include "umaa/internal/dds_backend.h"
#include "umaa/internal/dds_backend_qos.h"
#include "umaa/qos_policy.hpp"
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
    auto ext=reinterpret_cast<UmaaGetDdsQosBackendV2>(
        library_->symbol(UMAA_DDS_QOS_BACKEND_ENTRY));
    if(ext) {
      qos_api_=ext(UMAA_DDS_QOS_ABI_VERSION);
      if(!qos_api_ || qos_api_->abi_version!=UMAA_DDS_QOS_ABI_VERSION ||
         qos_api_->struct_size!=sizeof(UmaaDdsQosBackendV2) ||
         !qos_api_->open_writer_qos || !qos_api_->open_reader_qos)
        throw std::runtime_error("invalid DDS QoS extension ABI");
    }
  }
  DdsBackendPlugin(const DdsBackendPlugin&) = delete;
  DdsBackendPlugin& operator=(const DdsBackendPlugin&) = delete;
  std::string name() const { return api_->vendor_name; }
  uint64_t capabilities() const { return api_->capabilities; }
  bool has_qos_extension() const noexcept { return qos_api_!=nullptr; }
  class Domain final {
  public:
    Domain(const UmaaDdsBackendV1* api, const UmaaDdsQosBackendV2* qapi,
           uint32_t domain_id) : api_(api), qos_api_(qapi) {
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
    void* writer(const UmaaDdsTopicV1& topic, const qos::Policy& policy,
                 const qos::Requirements& requirements={}) {
      validate(topic,true); qos::validate(policy,requirements);
      if(!qos_api_)throw std::runtime_error("DDS backend lacks explicit QoS extension");
      if((topic.required_features&UMAA_DDS_CAP_RELIABLE) &&
         policy.reliability!=qos::Reliability::reliable)
        throw std::invalid_argument("reviewed topic requires reliable DDS endpoint");
      const auto needed=qos::required_capabilities(policy)|topic.required_features;
      if((qos_api_->capabilities & needed)!=needed)
        throw std::runtime_error("DDS backend lacks required QoS capability");
      const auto raw=qos::to_abi(policy);
      void* out=qos_api_->open_writer_qos(domain_,&topic,&raw);
      if(!out)throw std::runtime_error("DDS backend rejected requested Writer QoS/TypeBinding");
      return out;
    }
    void* reader(const UmaaDdsTopicV1& topic, const qos::Policy& policy,
                 UmaaDdsOnSampleV1 callback, void* ctx,
                 const qos::Requirements& requirements={}) {
      validate(topic); qos::validate(policy,requirements);
      if(!callback)throw std::invalid_argument("DDS receive callback required");
      if(!qos_api_)throw std::runtime_error("DDS backend lacks explicit QoS extension");
      const auto needed=qos::required_capabilities(policy)|topic.required_features;
      if((qos_api_->capabilities & needed)!=needed)
        throw std::runtime_error("DDS backend lacks required QoS capability");
      const auto raw=qos::to_abi(policy);
      void* out=qos_api_->open_reader_qos(domain_,&topic,&raw,callback,ctx);
      if(!out)throw std::runtime_error("DDS backend rejected requested Reader QoS/TypeBinding");
      return out;
    }
    int32_t write(void* w, const UmaaDdsSampleV1& sample) { return api_->write(w, &sample); }
    int32_t dispose(void* w, const UmaaDdsSampleV1& sample) { return api_->dispose(w, &sample); }
    int32_t unregister_instance(void* w, const UmaaDdsSampleV1& sample) { return api_->unregister_instance(w, &sample); }
    int32_t poll(void* r, uint32_t max_samples = 32) { return api_->poll(r, max_samples); }
    int32_t wait_for_acks(void* w, uint32_t timeout_ms) { return api_->wait_for_acks(w, timeout_ms); }
    void close_writer(void* w) { if (w) api_->close_writer(w); }
    void close_reader(void* r) { if (r) api_->close_reader(r); }
  private:
    void validate(const UmaaDdsTopicV1& t,bool extended=false) const {
      if (!t.topic_name || !*t.topic_name || !t.schema_id || !*t.schema_id ||
          !t.qos_profile || !*t.qos_profile)
        throw std::invalid_argument("DDS topic requires explicit reviewed schema and QoS");
      const uint64_t caps=extended&&qos_api_?qos_api_->capabilities:api_->capabilities;
      if ((caps & t.required_features) != t.required_features)
        throw std::runtime_error("DDS vendor lacks mandatory capability");
    }
    const UmaaDdsBackendV1* api_;
    const UmaaDdsQosBackendV2* qos_api_{nullptr};
    void* domain_{nullptr};
  };
  // Domain and all endpoints MUST be destroyed before the owning plugin.
  std::unique_ptr<Domain> open_domain(uint32_t id) const {
    return std::make_unique<Domain>(api_, qos_api_, id);
  }
private:
  std::unique_ptr<internal::NativeLibrary> library_;
  const UmaaDdsBackendV1* api_{nullptr};
  const UmaaDdsQosBackendV2* qos_api_{nullptr};
};
} // namespace umaa
