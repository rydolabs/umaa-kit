// Schema-bound, vendor-neutral DDS event writer for CommandEngine.
// A generated/reviewed Service must provide status/ack codecs and bindings.
// No DDS vendor headers, no guessed UMAA QoS, and no automatic replay.
#pragma once
#include "umaa/command_transport.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
template<class Service> class DdsCommandEventTransport final:
    public CommandTransport<typename Service::Command> {
  using Command=typename Service::Command;
public:
  DdsCommandEventTransport(std::shared_ptr<DdsBackendPlugin> backend,
                           std::shared_ptr<DdsBackendPlugin::Domain> domain,
                           std::chrono::milliseconds retirement_delay={})
      :backend_(std::move(backend)),domain_(std::move(domain)),
       retirement_delay_(retirement_delay) {
    if(retirement_delay_.count()<0 || retirement_delay_>std::chrono::seconds(5))
      throw std::invalid_argument("invalid DDS retirement delay");
    if(!backend_||!domain_)throw std::invalid_argument("missing command DDS backend");
    const UmaaDdsTopicV1 status{Service::status_topic,Service::status_schema,
                                Service::qos_profile,UMAA_DDS_CAP_RELIABLE|
                                UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
    const UmaaDdsTopicV1 ack{Service::ack_topic,Service::ack_schema,
                             Service::qos_profile,UMAA_DDS_CAP_RELIABLE|
                             UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
    status_writer_=domain_->writer(status);
    try{ack_writer_=domain_->writer(ack);}
    catch(...){domain_->close_writer(status_writer_);status_writer_=nullptr;throw;}
  }
  ~DdsCommandEventTransport() override {
    if(ack_writer_)domain_->close_writer(ack_writer_);
    if(status_writer_)domain_->close_writer(status_writer_);
  }
  DdsCommandEventTransport(const DdsCommandEventTransport&)=delete;
  DdsCommandEventTransport& operator=(const DdsCommandEventTransport&)=delete;
  void publish_status(const Identity& id,const Event& event) override {
    submit(status_writer_,id,Service::encode_status(id,event),false);
  }
  void publish_ack(const Identity& id,const Command& cmd) override {
    submit(ack_writer_,id,Service::encode_ack(id,cmd),false);
  }
  // Flush terminal Status and ACK before retiring either instance.
  // Otherwise some DDS readers may observe the lifecycle transition without
  // having consumed the corresponding terminal business result.
  void before_dispose(const Identity&) override {
    wait_for_acks(5000);
    // Test/contract-configured sample retention window. DDS ACK confirms
    // reader cache arrival, NOT that the application has consumed Status.
    if(retirement_delay_.count()>0)std::this_thread::sleep_for(retirement_delay_);
  }
  void dispose_status(const Identity& id) override {
    submit(status_writer_,id,{},true);
  }
  void dispose_ack(const Identity& id,const Command& cmd) override {
    submit(ack_writer_,id,Service::encode_ack(id,cmd),true);
  }
  void wait_for_acks(std::uint32_t timeout_ms) {
    require_active();
    if(domain_->wait_for_acks(status_writer_,timeout_ms)!=UMAA_DDS_OK ||
       domain_->wait_for_acks(ack_writer_,timeout_ms)!=UMAA_DDS_OK) {
      faulted_=true;
      throw std::runtime_error("unconfirmed command event ACK; do not replay");
    }
  }
  bool faulted() const noexcept{return faulted_;}
private:
  void require_active() const {
    if(faulted_)throw std::logic_error("command DDS writer faulted");
  }
  void submit(void* writer,const Identity& id,
              const std::vector<std::uint8_t>& payload,bool disposal) {
    require_active();
    const auto key=Service::instance_key(id);
    if(key.empty()){faulted_=true;throw std::invalid_argument("missing command key");}
    const auto stamp=timestamp_.fetch_add(1,std::memory_order_relaxed);
    if(stamp<=0){faulted_=true;throw std::overflow_error("command timestamp exhausted");}
    UmaaDdsSampleV1 sample{payload.empty()?nullptr:payload.data(),
      static_cast<std::uint32_t>(payload.size()),key.c_str(),stamp,
      disposal?UMAA_DDS_EVENT_DISPOSE:UMAA_DDS_EVENT_DATA};
    if(payload.size()>UINT32_MAX) {
      faulted_=true;throw std::length_error("oversized command event");
    }
    const auto result=disposal?domain_->dispose(writer,sample):domain_->write(writer,sample);
    if(result!=UMAA_DDS_OK) {
      faulted_=true;throw std::runtime_error("ambiguous command DDS delivery; no retry");
    }
  }
  std::chrono::milliseconds retirement_delay_{};
  std::shared_ptr<DdsBackendPlugin> backend_;
  std::shared_ptr<DdsBackendPlugin::Domain> domain_;
  void* status_writer_{};
  void* ack_writer_{};
  std::atomic<std::int64_t> timestamp_{1};
  bool faulted_{false};
};
} // namespace umaa
