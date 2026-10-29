// Vendor-independent inbound Command reader, device FSM, Status/Ack writer.
// The user supplies reviewed Service codecs and a Component DLL/SO only.
// This synthetic development slice does NOT establish official UMAA QoS.
#pragma once
#include "umaa/component_command_provider.hpp"
#include "umaa/dds_command_event_transport.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace umaa {
template<class Service> class DdsCommandRequestServer final {
public:
  using Command=typename Service::Command;

  DdsCommandRequestServer(const std::string& backend_path,std::uint32_t domain_id,
      std::shared_ptr<internal::ComponentRuntime> runtime,
      std::string source,std::string provider,
      std::chrono::milliseconds retirement_delay={})
      :backend_(std::make_shared<DdsBackendPlugin>(backend_path)),
       domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
           backend_->open_domain(domain_id).release())),
       runtime_(std::move(runtime)),source_(std::move(source)),
       provider_id_(std::move(provider)) {
    if(!runtime_||source_.empty()||provider_id_.empty())
      throw std::invalid_argument("Command server requires runtime and identities");
    const UmaaDdsTopicV1 command{Service::command_topic,Service::command_schema,
      Service::qos_profile,
      UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
    reader_=domain_->reader(command,&DdsCommandRequestServer::receive,this);
    try {
      // Reader MUST exist before opening response writers: some adapters
      // wait for a remote matched reader before allowing a writer.
      wire_=std::make_unique<DdsCommandEventTransport<Service>>(
        backend_,domain_,retirement_delay);
      provider_=std::make_unique<ComponentCommandProvider<Service>>(
        provider_id_,runtime_,*wire_);
    }catch(...){
      provider_.reset();wire_.reset();
      domain_->close_reader(reader_);reader_=nullptr;
      throw;
    }
  }
  DdsCommandRequestServer(const DdsCommandRequestServer&)=delete;
  DdsCommandRequestServer& operator=(const DdsCommandRequestServer&)=delete;
  ~DdsCommandRequestServer(){
    // Callback objects and writers must outlive the reader's final callback.
    if(reader_)domain_->close_reader(reader_);
    provider_.reset();wire_.reset();
  }

  // Drive incoming DDS requests and pending device sessions. A cooperative
  // scheduler can temporarily disable device polling while still receiving
  // requests; the default always polls. No hidden DDS API in user code.
  int poll(std::uint32_t max_samples=16,bool poll_device=true) {
    require_active();
    if(!max_samples)throw std::invalid_argument("invalid Command poll budget");
    const int seen=domain_->poll(reader_,max_samples);
    if(seen<0||faulted_) {
      faulted_=true;
      throw std::runtime_error("remote Command Reader failed or callback faulted");
    }
    if(poll_device){
      const std::vector<std::string> pending(active_.begin(),active_.end());
      for(const auto& session:pending){
        if(!active_.count(session))continue;
        try {
          const auto events=provider_->poll_device(session);
          for(const auto& e:events){
            if(e.kind==Event::Kind::Status &&
                (e.detail=="COMPLETED"||e.detail=="FAILED"||e.detail=="CANCELED")){
              ++terminal_count_;
              active_.erase(session);
              break;
            }
          }
        }catch(...){faulted_=true;throw;}
      }
    }
    return seen;
  }
  void wait_for_acks(std::uint32_t timeout_ms=5000) {
    require_active();
    try{wire_->wait_for_acks(timeout_ms);}
    catch(...){faulted_=true;throw;}
  }
  std::size_t active_sessions() const noexcept{return active_.size();}
  std::size_t accepted_requests() const noexcept{return accepted_count_;}
  std::size_t retired_sessions() const noexcept{return retired_count_;}
  std::size_t terminal_commands() const noexcept{return terminal_count_;}
  std::size_t canceled_commands() const noexcept{return canceled_count_;}
  bool faulted() const noexcept {
    return faulted_||(provider_&&provider_->faulted())||(wire_&&wire_->faulted());
  }
private:
  static void receive(void* ctx,const UmaaDdsSampleV1* sample) noexcept {
    auto* self=static_cast<DdsCommandRequestServer*>(ctx);
    if(!self)return;
    try{self->handle(sample);}
    catch(...){self->faulted_=true;} // never throw across DDS C callback ABI
  }
  void handle(const UmaaDdsSampleV1* sample) {
    if(!sample||!sample->instance_key||!*sample->instance_key)
      throw std::runtime_error("malformed remote Command identity");
    const std::string session=sample->instance_key;
    if(sample->event_kind==UMAA_DDS_EVENT_DATA){
      if(!sample->payload||!sample->payload_size)
        throw std::runtime_error("missing Command payload");
      Command command{};
      if(!Service::decode_command(sample->payload,sample->payload_size,command))
        throw std::runtime_error("invalid Command TypeBinding");
      const auto events=provider_->receive(
        {source_,provider_id_,session,sample->source_timestamp_ns},command);
      bool accepted=false,terminal=false;
      for(const auto& e:events) {
        if(e.kind!=Event::Kind::Ignored)accepted=true;
        if(e.kind==Event::Kind::Status &&
           (e.detail=="COMPLETED"||e.detail=="FAILED"||e.detail=="CANCELED"))
          terminal=true;
      }
      if(accepted){
        ++accepted_count_;
        if(terminal){active_.erase(session);++terminal_count_;}
        else active_.insert(session);
      }
    }else if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE){
      const auto events=provider_->command_disposed(source_,session);
      bool terminated=false,disposed=false;
      for(const auto& e:events) {
        if(e.kind==Event::Kind::Status && e.detail=="CANCELED"){
          ++canceled_count_;++terminal_count_;terminated=true;
        }
        if(e.kind==Event::Kind::DisposeAck)disposed=true;
      }
      if(disposed) {
        active_.erase(session);++retired_count_;
      }else if(terminated)active_.erase(session);
      // A refusal to cancel is deliberately NOT a terminal status:
      // the pending device remains tracked until genuine completion.
    }else{
      // Unregister is not proof of device cancellation and must not be
      // translated into a successful terminal state.
      throw std::runtime_error("unsupported Command lifecycle event");
    }
  }
  void require_active() const {
    if(faulted())throw std::logic_error("remote Command server quarantined");
  }
  std::shared_ptr<DdsBackendPlugin> backend_;
  std::shared_ptr<DdsBackendPlugin::Domain> domain_;
  std::shared_ptr<internal::ComponentRuntime> runtime_;
  std::string source_,provider_id_;
  void* reader_{};
  std::unique_ptr<DdsCommandEventTransport<Service>> wire_;
  std::unique_ptr<ComponentCommandProvider<Service>> provider_;
  std::unordered_set<std::string> active_;
  std::size_t accepted_count_{},retired_count_{},terminal_count_{},canceled_count_{};
  bool faulted_{false};
};
} // namespace umaa
