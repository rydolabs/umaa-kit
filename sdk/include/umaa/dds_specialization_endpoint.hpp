// Development-only Generalization and Specialization on independent real DDS
// Topics. GUIDs and revision timestamps are carried INSIDE a synthetic
// payload, not official UMAA multi-key TypeSupport. C ABI v1 unchanged.
#pragma once
#include "umaa/sdk_v1_core.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
class SyntheticSpecializationPacket final {
public:
  struct Packet {
    sdk_v1::Guid128 id{};
    std::int64_t timestamp{};
    bool tombstone{};
    std::vector<std::uint8_t> value;
  };
  static std::vector<std::uint8_t> encode(const Packet& packet){
    if(packet.id.is_nil()||packet.timestamp<=0||
       packet.value.size()>4096||
       (packet.tombstone&&!packet.value.empty())||
       (!packet.tombstone&&packet.value.empty()))
      throw std::invalid_argument("invalid synthetic Specialization packet");
    std::vector<std::uint8_t> out;out.reserve(34+packet.value.size());
    put(out,0x554D5350u,4); // synthetic "PSMU" Little Endian
    put(out,1,1);put(out,packet.tombstone?2:1,1);put(out,0,2);
    out.insert(out.end(),packet.id.bytes.begin(),packet.id.bytes.end());
    std::uint64_t bits;std::memcpy(&bits,&packet.timestamp,sizeof bits);
    put(out,bits,8);put(out,packet.value.size(),2);
    out.insert(out.end(),packet.value.begin(),packet.value.end());
    return out;
  }
  static Packet decode(const std::uint8_t* p,std::size_t n){
    if(!p||n<34||n>4130)throw std::invalid_argument("bad specialization envelope length");
    std::size_t i=0;
    if(get(p,n,i,4)!=0x554D5350u||get(p,n,i,1)!=1)
      throw std::invalid_argument("unknown specialization packet version");
    const auto kind=get(p,n,i,1);
    if((kind!=1&&kind!=2)||get(p,n,i,2)!=0)
      throw std::invalid_argument("invalid specialization packet flags");
    Packet result;
    for(auto& b:result.id.bytes)b=p[i++];
    const auto bits=get(p,n,i,8);
    std::memcpy(&result.timestamp,&bits,sizeof bits);
    const auto count=get(p,n,i,2);
    if(result.id.is_nil()||result.timestamp<=0||count>4096||
       count!=n-i||((kind==2)!=(count==0)))
      throw std::invalid_argument("invalid specialization identity or payload");
    result.tombstone=kind==2;
    result.value.assign(p+i,p+n);
    return result;
  }
private:
  static void put(std::vector<std::uint8_t>& out,std::uint64_t value,
                  std::size_t width){
    for(std::size_t j=0;j<width;++j)
      out.push_back(static_cast<std::uint8_t>(value>>(j*8)));
  }
  static std::uint64_t get(const std::uint8_t* p,std::size_t n,
                           std::size_t& i,std::size_t width){
    if(width>n-i)throw std::invalid_argument("truncated specialization payload");
    std::uint64_t v=0;
    for(std::size_t j=0;j<width;++j)
      v|=std::uint64_t(p[i++])<<(j*8);
    return v;
  }
};

namespace specialization_wire {
template<class Service>
UmaaDdsTopicV1 topic(bool detail){
  const auto* name=detail?Service::specialized_topic:Service::general_topic;
  if(!name||!*name||!Service::schema_id||!*Service::schema_id||
     !Service::qos_profile||!*Service::qos_profile||
     std::string(Service::general_topic)==Service::specialized_topic)
    throw std::invalid_argument("invalid separate Specialization DDS Topics");
  return {name,Service::schema_id,Service::qos_profile,
          UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
}
}

template<class Service> class DdsSpecializationPublisher final {
public:
  using General=typename Service::General;
  using Specialized=typename Service::Specialized;
  using Guid=sdk_v1::Guid128;
  DdsSpecializationPublisher(const std::string& path,std::uint32_t domain,
                             std::string key)
    :backend_(std::make_shared<DdsBackendPlugin>(path)),
     domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
       backend_->open_domain(domain).release())),key_(std::move(key)){
    if(key_.empty())throw std::invalid_argument("missing Specialization key");
    general_writer_=domain_->writer(specialization_wire::topic<Service>(false));
    try{special_writer_=domain_->writer(specialization_wire::topic<Service>(true));}
    catch(...){domain_->close_writer(general_writer_);general_writer_=nullptr;throw;}
  }
  ~DdsSpecializationPublisher(){
    if(special_writer_)domain_->close_writer(special_writer_);
    if(general_writer_)domain_->close_writer(general_writer_);
  }
  DdsSpecializationPublisher(const DdsSpecializationPublisher&)=delete;
  DdsSpecializationPublisher& operator=(const DdsSpecializationPublisher&)=delete;
  void general(Guid id,std::int64_t ts,const General& value){
    publish(general_writer_,{id,ts,false,Service::encode_general(value)});
  }
  void specialization(Guid id,std::int64_t ts,const Specialized& value){
    publish(special_writer_,{id,ts,false,Service::encode_specialized(value)});
  }
  // One tombstone is sufficient to quarantine the referenced pair.
  // Publish on both topics to make independent subscribers aware.
  void dispose(Guid id,std::int64_t ts) {
    publish(general_writer_,{id,ts,true,{}});
    publish(special_writer_,{id,ts,true,{}});
  }
  void finish(std::uint32_t ms=5000){
    require_active();
    try {
      await_ack(ms);
      const UmaaDdsSampleV1 s{nullptr,0,key_.c_str(),1,UMAA_DDS_EVENT_DISPOSE};
      if(domain_->dispose(general_writer_,s)!=UMAA_DDS_OK||
         domain_->dispose(special_writer_,s)!=UMAA_DDS_OK)
        throw std::runtime_error("Specialization channel Dispose unknown");
      await_ack(ms);closed_=true;
    }catch(...){faulted_=true;throw;}
  }
  bool faulted() const noexcept{return faulted_;}
private:
  void require_active() const {
    if(faulted_||closed_)throw std::logic_error("Specialization writer closed/faulted");
  }
  void publish(void* writer,SyntheticSpecializationPacket::Packet packet){
    require_active();
    try {
      const auto bytes=SyntheticSpecializationPacket::encode(packet);
      UmaaDdsSampleV1 sample{bytes.data(),static_cast<std::uint32_t>(bytes.size()),
                             key_.c_str(),packet.timestamp,UMAA_DDS_EVENT_DATA};
      if(domain_->write(writer,sample)!=UMAA_DDS_OK)
        throw std::runtime_error("ambiguous Specialization delivery; no replay");
    }catch(...){faulted_=true;throw;}
  }
  void await_ack(std::uint32_t ms){
    if(domain_->wait_for_acks(general_writer_,ms)!=UMAA_DDS_OK||
       domain_->wait_for_acks(special_writer_,ms)!=UMAA_DDS_OK)
      throw std::runtime_error("unconfirmed Specialization writer ACK");
  }
  std::shared_ptr<DdsBackendPlugin> backend_;
  std::shared_ptr<DdsBackendPlugin::Domain> domain_;
  std::string key_;
  void *general_writer_{},*special_writer_{};
  bool faulted_{},closed_{};
};

template<class Service> class DdsSpecializationConsumer final {
public:
  using General=typename Service::General;
  using Specialized=typename Service::Specialized;
  using Guid=sdk_v1::Guid128;
  using Pair=typename sdk_v1::SpecializationJoin<General,Specialized>::Pair;
  DdsSpecializationConsumer(const std::string& path,std::uint32_t domain,
                            std::string key,std::size_t max=1024)
    :backend_(std::make_shared<DdsBackendPlugin>(path)),
     domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
       backend_->open_domain(domain).release())),
     key_(std::move(key)),join_(max) {
    if(key_.empty())throw std::invalid_argument("missing Specialization key");
    general_ctx_={this,false};special_ctx_={this,true};
    general_reader_=domain_->reader(specialization_wire::topic<Service>(false),
      &DdsSpecializationConsumer::receive,&general_ctx_);
    try {
      special_reader_=domain_->reader(specialization_wire::topic<Service>(true),
        &DdsSpecializationConsumer::receive,&special_ctx_);
    }catch(...){domain_->close_reader(general_reader_);general_reader_=nullptr;throw;}
  }
  ~DdsSpecializationConsumer(){
    if(special_reader_)domain_->close_reader(special_reader_);
    if(general_reader_)domain_->close_reader(general_reader_);
  }
  DdsSpecializationConsumer(const DdsSpecializationConsumer&)=delete;
  DdsSpecializationConsumer& operator=(const DdsSpecializationConsumer&)=delete;
  int poll(std::uint32_t max=16){
    if(faulted_)throw std::logic_error("Specialization reader faulted");
    if(!max)throw std::invalid_argument("empty Specialization poll budget");
    const int a=domain_->poll(general_reader_,max);
    if(a<0||faulted_)return quarantine();
    const int b=domain_->poll(special_reader_,max);
    if(b<0||faulted_)return quarantine();
    return a+b;
  }
  std::optional<Pair> resolve(sdk_v1::SpecializationRef ref) const {
    return faulted_||general_closed_||special_closed_?
      std::nullopt:join_.resolve(ref);
  }
  template<class Projector>
  std::optional<std::vector<Pair>> resolve_collection(
     const std::optional<std::vector<General>>& values,Projector project) const {
    if(faulted_||general_closed_||special_closed_)return std::nullopt;
    return join_.resolve_collection(values,project);
  }
  bool faulted() const noexcept{return faulted_;}
  bool disposed() const noexcept{return general_closed_&&special_closed_;}
private:
  struct Context{DdsSpecializationConsumer* self;bool detail;};
  static void receive(void* ctx,const UmaaDdsSampleV1* sample) noexcept {
    auto* c=static_cast<Context*>(ctx);
    if(!c||!c->self)return;
    auto& self=*c->self;
    try {
      if(!sample||!sample->instance_key||self.key_!=sample->instance_key)
        throw std::runtime_error("Specialization wire key mismatch");
      if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE){
        if(c->detail)self.special_closed_=true;else self.general_closed_=true;
      }else if(sample->event_kind==UMAA_DDS_EVENT_DATA){
        if(self.general_closed_||self.special_closed_)
          throw std::runtime_error("Specialization data after DDS Dispose");
        const auto packet=SyntheticSpecializationPacket::decode(
          sample->payload,sample->payload_size);
        if(packet.tombstone){(void)self.join_.dispose(packet.id,packet.timestamp);}
        else if(c->detail){
          Specialized v{};
          if(!Service::decode_specialized(packet.value.data(),packet.value.size(),v))
            throw std::runtime_error("Specialization detail codec rejected data");
          (void)self.join_.put_specialization(packet.id,packet.timestamp,std::move(v));
        }else{
          General v{};
          if(!Service::decode_general(packet.value.data(),packet.value.size(),v))
            throw std::runtime_error("Specialization general codec rejected data");
          (void)self.join_.put_general(packet.id,packet.timestamp,std::move(v));
        }
      }else throw std::runtime_error("unexpected Specialization lifecycle");
    }catch(...){self.faulted_=true;}
  }
  int quarantine(){
    faulted_=true;throw std::runtime_error("Specialization reader fault or corrupt packet");
  }
  std::shared_ptr<DdsBackendPlugin> backend_;
  std::shared_ptr<DdsBackendPlugin::Domain> domain_;
  std::string key_;
  sdk_v1::SpecializationJoin<General,Specialized> join_;
  Context general_ctx_{},special_ctx_{};
  void *general_reader_{},*special_reader_{};
  bool general_closed_{},special_closed_{},faulted_{};
};
} // namespace umaa
