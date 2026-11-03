// SDK v1 real multi-topic LargeSet/LargeList transport and G/S correlation.
//
// Elements, metadata, generalizations and specializations have INDEPENDENT
// DDS readers/writers. Each topic's TypeBinding comes from the caller's
// reviewed ServiceTraits; this file contains no vendor headers.
//
// The synthetic fixture uses opaque frame fields, not official UMAA IDL.
// DDS GROUP coherent/ordered access and atomic multi-topic commit are NOT
// claimed. Readers expose only fully reconstructed logical snapshots.
#pragma once
#include "umaa/dds_backend_loader.hpp"
#include "umaa/large_collection.hpp"
#include "umaa/sdk_v1_core.hpp"
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>
#include <type_traits>

namespace umaa::sdk_v1 {
namespace native_wire {
inline void u8(std::vector<std::uint8_t>& b,std::uint8_t x){b.push_back(x);}
inline void u32(std::vector<std::uint8_t>& b,std::uint32_t v){
  for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));
}
inline void i64(std::vector<std::uint8_t>& b,std::int64_t v){
  std::uint64_t n=0;std::memcpy(&n,&v,sizeof(v));
  for(unsigned i=0;i<8;++i)b.push_back(static_cast<std::uint8_t>(n>>(8*i)));
}
inline void bytes(std::vector<std::uint8_t>& b,const std::vector<std::uint8_t>& data){
  if(data.size()>60000)throw std::length_error("native fixture payload exceeds bound");
  u32(b,static_cast<std::uint32_t>(data.size()));
  b.insert(b.end(),data.begin(),data.end());
}
inline void str(std::vector<std::uint8_t>& b,const std::string& s){
  if(s.size()>512)throw std::length_error("native fixture identifier exceeds bound");
  u32(b,static_cast<std::uint32_t>(s.size()));
  b.insert(b.end(),s.begin(),s.end());
}
struct Reader {
  const std::uint8_t* p;
  std::size_t n;
  explicit Reader(const UmaaDdsSampleV1& s):p(s.payload),n(s.payload_size){
    if(!p || !n)throw std::invalid_argument("empty native event");
  }
  std::uint8_t u8() {
    if(n<1)throw std::invalid_argument("truncated event");
    auto v=*p++;--n;return v;
  }
  std::uint32_t u32(){
    if(n<4)throw std::invalid_argument("truncated event");
    std::uint32_t v=0;
    for(unsigned i=0;i<4;++i)v|=std::uint32_t(p[i])<<(8*i);
    p+=4;n-=4;return v;
  }
  std::int64_t i64(){
    if(n<8)throw std::invalid_argument("truncated event");
    std::uint64_t v=0;
    for(unsigned i=0;i<8;++i)v|=std::uint64_t(p[i])<<(8*i);
    p+=8;n-=8;std::int64_t result;std::memcpy(&result,&v,8);return result;
  }
  std::string str(){
    const auto size=u32();
    if(size>512||size>n)throw std::invalid_argument("invalid native identifier");
    std::string value(reinterpret_cast<const char*>(p),size);
    p+=size;n-=size;return value;
  }
  std::vector<std::uint8_t> bytes(){
    const auto size=u32();
    if(size>60000||size>n)throw std::invalid_argument("invalid native payload");
    std::vector<std::uint8_t> value(p,p+size);
    p+=size;n-=size;return value;
  }
  void end() const{if(n)throw std::invalid_argument("trailing native payload");}
};
inline UmaaDdsTopicV1 topic(const char* name,const char* schema,const char* qos){
  return {name,schema,qos,UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
}
inline void checked_write(umaa::DdsBackendPlugin::Domain& d,void* writer,
                          const std::string& key,
                          const std::vector<std::uint8_t>& bytes){
  if(key.empty()||bytes.empty()||bytes.size()>UINT32_MAX)
    throw std::invalid_argument("invalid native DDS key or frame");
  UmaaDdsSampleV1 s{bytes.data(),static_cast<std::uint32_t>(bytes.size()),
                    key.c_str(),1,UMAA_DDS_EVENT_DATA};
  if(d.write(writer,s)!=UMAA_DDS_OK)
    throw std::runtime_error("ambiguous native DDS write; no retry");
}
inline void checked_dispose(umaa::DdsBackendPlugin::Domain& d,void* writer,
                            const std::string& key){
  UmaaDdsSampleV1 s{nullptr,0,key.c_str(),1,UMAA_DDS_EVENT_DISPOSE};
  if(d.dispose(writer,s)!=UMAA_DDS_OK)
    throw std::runtime_error("ambiguous native DDS dispose; no retry");
}
inline void checked_acks(umaa::DdsBackendPlugin::Domain& d,void* writer,
                         std::uint32_t timeout){
  if(d.wait_for_acks(writer,timeout)!=UMAA_DDS_OK)
    throw std::runtime_error("native DDS writer ACK unconfirmed");
}
} // namespace native_wire

template<class Service> class DdsNativeCollectionPublisher final {
public:
  using Value=typename Service::Value;
  DdsNativeCollectionPublisher(const std::string& backend,std::uint32_t domain,
                               std::string collection_key)
    :backend_(std::make_shared<umaa::DdsBackendPlugin>(backend)),
     domain_(std::shared_ptr<umaa::DdsBackendPlugin::Domain>(
         backend_->open_domain(domain).release())),
     key_(std::move(collection_key)) {
    if(key_.empty())throw std::invalid_argument("collection key required");
    element_=domain_->writer(native_wire::topic(
        Service::element_topic,Service::element_schema,Service::qos_profile));
    try{
      metadata_=domain_->writer(native_wire::topic(
          Service::metadata_topic,Service::metadata_schema,Service::qos_profile));
    }catch(...){domain_->close_writer(element_);element_=nullptr;throw;}
  }
  ~DdsNativeCollectionPublisher(){
    domain_->close_writer(metadata_);
    domain_->close_writer(element_);
  }
  DdsNativeCollectionPublisher(const DdsNativeCollectionPublisher&)=delete;
  DdsNativeCollectionPublisher& operator=(const DdsNativeCollectionPublisher&)=delete;
  void upsert(const umaa::CollectionElement<Value>& item){
    require_active();
    if(item.element_id.empty()||item.timestamp<=0)
      throw std::invalid_argument("invalid native Collection element");
    try {
      std::vector<std::uint8_t> b;
      native_wire::u8(b,1);native_wire::u8(b,1);
      native_wire::i64(b,item.timestamp);
      native_wire::str(b,item.next_id);
      native_wire::bytes(b,Service::encode_value(item.value));
      native_wire::checked_write(*domain_,element_,item.element_id,b);
    }catch(...){faulted_=true;throw;}
  }
  // Each deletion emits an application-timestamped marker on Element Topic
  // BEFORE the actual DDS Dispose, preserving deterministic tombstone order.
  void dispose_element(const std::string& id,std::int64_t timestamp){
    require_active();
    if(id.empty()||timestamp<=0)throw std::invalid_argument("invalid native Dispose");
    try{
      std::vector<std::uint8_t> b;
      native_wire::u8(b,1);native_wire::u8(b,2);
      native_wire::i64(b,timestamp);
      native_wire::checked_write(*domain_,element_,id,b);
      native_wire::checked_dispose(*domain_,element_,id);
    }catch(...){faulted_=true;throw;}
  }
  void publish_metadata(const umaa::CollectionMetadata& m){
    require_active();
    if(m.collection_id.empty()||m.size>UINT32_MAX)
      throw std::invalid_argument("invalid native Collection metadata");
    try{
      std::vector<std::uint8_t> b;
      native_wire::u8(b,1);
      native_wire::str(b,m.collection_id);
      native_wire::u32(b,static_cast<std::uint32_t>(m.size));
      native_wire::str(b,m.update_element_id);
      native_wire::u8(b,m.update_element_timestamp.has_value()?1:0);
      if(m.update_element_timestamp)native_wire::i64(b,*m.update_element_timestamp);
      native_wire::str(b,m.starting_element_id);
      native_wire::checked_write(*domain_,metadata_,key_,b);
    }catch(...){faulted_=true;throw;}
  }
  void flush(std::uint32_t timeout_ms=5000){
    require_active();
    try{
      native_wire::checked_acks(*domain_,element_,timeout_ms);
      native_wire::checked_acks(*domain_,metadata_,timeout_ms);
    }catch(...){faulted_=true;throw;}
  }
  bool faulted() const noexcept{return faulted_;}
private:
  void require_active() const{
    if(faulted_)throw std::logic_error("native Collection writer quarantined");
  }
  std::shared_ptr<umaa::DdsBackendPlugin> backend_;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain_;
  std::string key_;
  void* element_{},*metadata_{};
  bool faulted_{false};
};

template<class Service> class DdsNativeCollectionConsumer final {
public:
  using Value=typename Service::Value;
  DdsNativeCollectionConsumer(const std::string& backend,std::uint32_t domain,
                              std::string collection_key,std::size_t maximum=1024)
    :backend_(std::make_shared<umaa::DdsBackendPlugin>(backend)),
     domain_(std::shared_ptr<umaa::DdsBackendPlugin::Domain>(
         backend_->open_domain(domain).release())),
     key_(std::move(collection_key)),assembler_(Service::kind,maximum) {
    if(key_.empty()||maximum==0)throw std::invalid_argument("invalid native Collection subscription");
    element_=domain_->reader(native_wire::topic(
      Service::element_topic,Service::element_schema,Service::qos_profile),
      &DdsNativeCollectionConsumer::on_element,this);
    try{
      metadata_=domain_->reader(native_wire::topic(
        Service::metadata_topic,Service::metadata_schema,Service::qos_profile),
        &DdsNativeCollectionConsumer::on_metadata,this);
    }catch(...){domain_->close_reader(element_);element_=nullptr;throw;}
  }
  ~DdsNativeCollectionConsumer(){
    domain_->close_reader(metadata_);
    domain_->close_reader(element_);
  }
  DdsNativeCollectionConsumer(const DdsNativeCollectionConsumer&)=delete;
  DdsNativeCollectionConsumer& operator=(const DdsNativeCollectionConsumer&)=delete;
  int poll(std::uint32_t maximum=32,bool metadata_first=true){
    if(faulted_)throw std::logic_error("native Collection reader quarantined");
    if(!maximum)throw std::invalid_argument("zero native Collection poll budget");
    const auto a=domain_->poll(metadata_first?metadata_:element_,maximum);
    const auto b=domain_->poll(metadata_first?element_:metadata_,maximum);
    if(a<0||b<0||faulted_){
      faulted_=true;
      throw std::runtime_error("native Collection decoder fault: "+error_);
    }
    return a+b;
  }
  std::optional<std::vector<Value>> snapshot() const {
    if(pending_metadata_||faulted_)return std::nullopt;
    return assembler_.snapshot();
  }
  std::uint64_t commit_count() const{return assembler_.commit_count();}
  bool saw_element_dispose() const noexcept{return saw_dispose_;}
  bool faulted() const noexcept{return faulted_;}
private:
  static void on_element(void* ctx,const UmaaDdsSampleV1* sample) noexcept{
    auto* self=static_cast<DdsNativeCollectionConsumer*>(ctx);
    try{self->handle_element(sample);}
    catch(const std::exception& e){self->error_=e.what();self->faulted_=true;}
    catch(...){self->error_="unexpected Element exception";self->faulted_=true;}
  }
  static void on_metadata(void* ctx,const UmaaDdsSampleV1* sample) noexcept{
    auto* self=static_cast<DdsNativeCollectionConsumer*>(ctx);
    try{self->handle_metadata(sample);}
    catch(const std::exception& e){self->error_=e.what();self->faulted_=true;}
    catch(...){self->error_="unexpected Metadata exception";self->faulted_=true;}
  }
  void handle_element(const UmaaDdsSampleV1* s){
    if(!s||!s->instance_key||!*s->instance_key)
      throw std::invalid_argument("Element missing DDS key");
    const std::string key=s->instance_key;
    if(s->event_kind==UMAA_DDS_EVENT_UNREGISTER) {
      // NO_WRITERS is NOT a confirmed deletion. Leave state unresolved.
      return;
    }
    if(s->event_kind==UMAA_DDS_EVENT_DISPOSE){
      // Some DDS stacks surface the instance as NOT_ALIVE_DISPOSED without
      // delivering the preceding marker as a distinct valid_data sample.
      // Accept a genuine DDS Dispose only for a previously observed key and
      // permanently fence it against stale resurrection until trusted resync.
      if(!marked_disposed_.count(key)) {
        if(!observed_keys_.count(key))
          throw std::invalid_argument("DDS Dispose for unknown Element key");
        assembler_.dispose(key);
        retired_keys_.insert(key);
      }
      saw_dispose_=true;
      if(pending_metadata_&&assembler_.commit_count()>pending_commit_)
        pending_metadata_=false;
      return;
    }
    if(s->event_kind!=UMAA_DDS_EVENT_DATA)
      throw std::invalid_argument("unexpected Element DDS lifecycle");
    native_wire::Reader rd(*s);
    if(rd.u8()!=1)throw std::invalid_argument("unsupported native Element version");
    const auto kind=rd.u8();
    const auto timestamp=rd.i64();
    if(timestamp<=0)throw std::invalid_argument("invalid Element timestamp");
    if(kind==2){
      rd.end();
      if(!retired_keys_.count(key) && !assembler_.dispose(key,timestamp))
        throw std::invalid_argument("stale/duplicate Element tombstone");
      marked_disposed_.insert(key);
    }else if(kind==1){
      if(retired_keys_.count(key))
        throw std::invalid_argument("Element reappeared after unpaired DDS Dispose");
      const auto next=rd.str();
      const auto data=rd.bytes();
      rd.end();
      Value value{};
      if(!Service::decode_value(data.data(),data.size(),value))
        throw std::invalid_argument("native Element codec mismatch");
      if(!assembler_.upsert({key,timestamp,std::move(value),next}))
        throw std::invalid_argument("stale/duplicate Element revision");
      observed_keys_.insert(key);
      marked_disposed_.erase(key);
    }else throw std::invalid_argument("unsupported Element operation");
    if(pending_metadata_&&assembler_.commit_count()>pending_commit_)
      pending_metadata_=false;
  }
  void handle_metadata(const UmaaDdsSampleV1* s){
    if(!s||!s->instance_key||key_!=s->instance_key)
      throw std::invalid_argument("invalid native Metadata topic/key event");
    if(s->event_kind==UMAA_DDS_EVENT_UNREGISTER)
      return; // writer loss is not a Collection metadata deletion
    if(s->event_kind!=UMAA_DDS_EVENT_DATA)
      throw std::invalid_argument("unsupported native Metadata lifecycle");
    native_wire::Reader rd(*s);
    if(rd.u8()!=1)throw std::invalid_argument("unsupported native Metadata version");
    umaa::CollectionMetadata m;
    m.collection_id=rd.str();
    m.size=rd.u32();
    m.update_element_id=rd.str();
    const auto timed=rd.u8();
    if(timed>1)throw std::invalid_argument("invalid Metadata timestamp presence");
    if(timed)m.update_element_timestamp=rd.i64();
    m.starting_element_id=rd.str();
    rd.end();
    if(m.collection_id!=Service::collection_id)
      throw std::invalid_argument("unexpected Collection ID");
    pending_commit_=assembler_.commit_count();
    pending_metadata_=true;
    assembler_.metadata(std::move(m));
    if(assembler_.commit_count()>pending_commit_)pending_metadata_=false;
  }
  std::shared_ptr<umaa::DdsBackendPlugin> backend_;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain_;
  std::string key_;
  umaa::CollectionAssembler<Value> assembler_;
  std::unordered_set<std::string> marked_disposed_;
  std::unordered_set<std::string> observed_keys_;
  std::unordered_set<std::string> retired_keys_;
  void* element_{},*metadata_{};
  std::string error_;
  std::uint64_t pending_commit_{};
  bool pending_metadata_{true},faulted_{false},saw_dispose_{false};
};

// Generalization/Specialization: independent Topic samples with explicitly
// encoded full GUID and linked revision. Every payload is checked against the
// DDS sample instance key as specified by the reviewed Service codec.
template<class Service> class DdsNativeSpecializationPublisher final {
public:
  using General=typename Service::General;
  using Specialized=typename Service::Specialized;
  DdsNativeSpecializationPublisher(const std::string& backend,std::uint32_t domain)
    :backend_(std::make_shared<umaa::DdsBackendPlugin>(backend)),
     domain_(std::shared_ptr<umaa::DdsBackendPlugin::Domain>(
       backend_->open_domain(domain).release())) {
    general_=domain_->writer(native_wire::topic(
      Service::general_topic,Service::general_schema,Service::qos_profile));
    try{specialized_=domain_->writer(native_wire::topic(
      Service::specialized_topic,Service::specialized_schema,Service::qos_profile));}
    catch(...){domain_->close_writer(general_);general_=nullptr;throw;}
  }
  ~DdsNativeSpecializationPublisher(){
    domain_->close_writer(specialized_);domain_->close_writer(general_);
  }
  void general(Guid128 id,std::int64_t timestamp,const General& value){
    send(general_,id,timestamp,Service::encode_general(value));
  }
  void specialized(Guid128 id,std::int64_t timestamp,const Specialized& value){
    send(specialized_,id,timestamp,Service::encode_specialized(value));
  }
  void flush(std::uint32_t timeout_ms=5000){
    require_active();
    try{
      native_wire::checked_acks(*domain_,general_,timeout_ms);
      native_wire::checked_acks(*domain_,specialized_,timeout_ms);
    }catch(...){faulted_=true;throw;}
  }
  bool faulted() const noexcept{return faulted_;}
private:
  void require_active() const{
    if(faulted_)throw std::logic_error("specialization writer quarantined");
  }
  void send(void* writer,Guid128 id,std::int64_t time,
            const std::vector<std::uint8_t>& value){
    require_active();
    if(id.is_nil()||time<=0)throw std::invalid_argument("invalid specialization reference");
    try{
      std::vector<std::uint8_t> b;
      native_wire::u8(b,1);
      for(const auto byte:id.bytes)native_wire::u8(b,byte);
      native_wire::i64(b,time);
      native_wire::bytes(b,value);
      native_wire::checked_write(*domain_,writer,Service::instance_key_for(id),b);
    }catch(...){faulted_=true;throw;}
  }
  std::shared_ptr<umaa::DdsBackendPlugin> backend_;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain_;
  void* general_{},*specialized_{};
  bool faulted_{false};
};

template<class Service> class DdsNativeSpecializationConsumer final {
public:
  using General=typename Service::General;
  using Specialized=typename Service::Specialized;
  DdsNativeSpecializationConsumer(const std::string& backend,std::uint32_t domain,
                                  std::size_t maximum=1024)
    :backend_(std::make_shared<umaa::DdsBackendPlugin>(backend)),
     domain_(std::shared_ptr<umaa::DdsBackendPlugin::Domain>(
       backend_->open_domain(domain).release())),join_(maximum) {
    general_=domain_->reader(native_wire::topic(
      Service::general_topic,Service::general_schema,Service::qos_profile),
      &DdsNativeSpecializationConsumer::on_general,this);
    try{specialized_=domain_->reader(native_wire::topic(
      Service::specialized_topic,Service::specialized_schema,Service::qos_profile),
      &DdsNativeSpecializationConsumer::on_specialized,this);}
    catch(...){domain_->close_reader(general_);general_=nullptr;throw;}
  }
  ~DdsNativeSpecializationConsumer(){
    domain_->close_reader(specialized_);domain_->close_reader(general_);
  }
  int poll(std::uint32_t maximum=32,bool reverse=false){
    if(faulted_)throw std::logic_error("specialization reader quarantined");
    if(!maximum)throw std::invalid_argument("zero specialization poll budget");
    const auto a=domain_->poll(reverse?specialized_:general_,maximum);
    const auto b=domain_->poll(reverse?general_:specialized_,maximum);
    if(a<0||b<0||faulted_){
      faulted_=true;
      throw std::runtime_error("specialization correlation failure: "+error_);
    }
    return a+b;
  }
  template<class Project>
  auto resolve_collection(const std::optional<std::vector<General>>& source,
                          Project project) const{
    if(faulted_)return std::optional<std::vector<typename
      SpecializationJoin<General,Specialized>::Pair>>{};
    return join_.resolve_collection(source,project);
  }
  bool faulted() const noexcept{return faulted_;}
private:
  static void on_general(void* ctx,const UmaaDdsSampleV1* s) noexcept{
    auto* self=static_cast<DdsNativeSpecializationConsumer*>(ctx);
    try{self->receive(s,true);}
    catch(const std::exception& e){self->error_=e.what();self->faulted_=true;}
    catch(...){self->error_="unexpected Generalization exception";self->faulted_=true;}
  }
  static void on_specialized(void* ctx,const UmaaDdsSampleV1* s) noexcept{
    auto* self=static_cast<DdsNativeSpecializationConsumer*>(ctx);
    try{self->receive(s,false);}
    catch(const std::exception& e){self->error_=e.what();self->faulted_=true;}
    catch(...){self->error_="unexpected Specialization exception";self->faulted_=true;}
  }
  void receive(const UmaaDdsSampleV1* s,bool is_general){
    if(!s||!s->instance_key)
      throw std::invalid_argument("missing specialization lifecycle key");
    if(s->event_kind==UMAA_DDS_EVENT_UNREGISTER)
      return; // DDS NO_WRITERS is not deletion, not a valid revision
    if(s->event_kind!=UMAA_DDS_EVENT_DATA)
      throw std::invalid_argument("unsupported specialization lifecycle");
    native_wire::Reader rd(*s);
    if(rd.u8()!=1)throw std::invalid_argument("unsupported specialization version");
    Guid128 id;
    for(auto& byte:id.bytes)byte=rd.u8();
    const auto time=rd.i64();
    const auto bytes=rd.bytes();
    rd.end();
    if(id.is_nil()||time<=0||Service::instance_key_for(id)!=s->instance_key)
      throw std::invalid_argument("specialization DDS key/identity mismatch");
    if(is_general){
      General value{};
      if(!Service::decode_general(bytes.data(),bytes.size(),value))
        throw std::invalid_argument("invalid Generalization payload");
      const auto linked=Service::reference(value);
      if(linked.id!=id||linked.timestamp_ns!=time ||
         !join_.put_general(id,time,std::move(value)))
        throw std::invalid_argument("Generalization ID/revision mismatch or replay");
    }else{
      Specialized value{};
      if(!Service::decode_specialized(bytes.data(),bytes.size(),value) ||
         !join_.put_specialization(id,time,std::move(value)))
        throw std::invalid_argument("invalid/stale Specialization");
    }
  }
  std::shared_ptr<umaa::DdsBackendPlugin> backend_;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain_;
  SpecializationJoin<General,Specialized> join_;
  void* general_{},*specialized_{};
  bool faulted_{false};
  std::string error_;
};

template<class Service> class DdsNativeSpecializedCollectionConsumer final{
public:
  using General=typename Service::Value;
  using Specialized=typename Service::Specialized;
  DdsNativeSpecializedCollectionConsumer(const std::string& path,
      std::uint32_t domain,std::string key,std::size_t maximum=1024)
    :collection_(path,domain,std::move(key),maximum),
     join_(path,domain,maximum) {
    static_assert(std::is_same_v<General,typename Service::General>,
       "Collection Value must be the Generalization type");
  }
  int poll(std::uint32_t max_samples=32){
    const auto one=collection_.poll(max_samples,true);
    const auto two=join_.poll(max_samples,true);
    return one+two;
  }
  auto snapshot() const {
    return join_.resolve_collection(collection_.snapshot(),
       [](const General& value){return Service::reference(value);});
  }
  bool saw_element_dispose() const{return collection_.saw_element_dispose();}
  std::uint64_t commits() const{return collection_.commit_count();}
  bool faulted() const{return collection_.faulted()||join_.faulted();}
private:
  DdsNativeCollectionConsumer<Service> collection_;
  DdsNativeSpecializationConsumer<Service> join_;
};
} // namespace umaa::sdk_v1
