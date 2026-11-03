// Two independently matched DDS Topics using an explicitly reviewed synthetic
// wire binding. Logical atomicity is guarded at the SDK joiner, not DDS GROUP.
// The adapter must provide Service::{elements_topic,metadata_topic,schema_id,
// qos_profile,collection_id,kind} and value codec; no TypeSupport is inferred.
#pragma once
#include "umaa/collection_batch_builder.hpp"
#include "umaa/collection_frame_codec.hpp"
#include "umaa/split_collection_joiner.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace umaa {
namespace split_wire {
template<class Service> UmaaDdsTopicV1 topic(const char* name){
  if(!name||!*name||std::string(name)==Service::elements_topic &&
      std::string(Service::elements_topic)==Service::metadata_topic)
    throw std::invalid_argument("missing/identical split DDS Topic");
  return {name,Service::schema_id,Service::qos_profile,
          UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
}
}
template<class Service> class DdsSplitCollectionPublisher final {
public:
  using Value=typename Service::Value;
  using Element=CollectionElement<Value>;
  using Operation=CollectionOperation<Value>;
  using Batch=CoherentCollectionBatch<Value>;
  DdsSplitCollectionPublisher(const std::string& plugin,std::uint32_t domain,
                               std::string key,std::size_t maximum=1024)
    :backend_(std::make_shared<DdsBackendPlugin>(plugin)),
     domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
       backend_->open_domain(domain).release())),
     key_(std::move(key)),
     builder_(Service::kind,Service::collection_id,maximum) {
    if(key_.empty())throw std::invalid_argument("missing split Collection key");
    elements_writer_=domain_->writer(split_wire::topic<Service>(Service::elements_topic));
    try{
      metadata_writer_=domain_->writer(split_wire::topic<Service>(Service::metadata_topic));
    }catch(...){domain_->close_writer(elements_writer_);elements_writer_=nullptr;throw;}
  }
  DdsSplitCollectionPublisher(const DdsSplitCollectionPublisher&)=delete;
  DdsSplitCollectionPublisher& operator=(const DdsSplitCollectionPublisher&)=delete;
  ~DdsSplitCollectionPublisher(){
    if(metadata_writer_)domain_->close_writer(metadata_writer_);
    if(elements_writer_)domain_->close_writer(elements_writer_);
  }
  void snapshot(std::uint64_t generation,const std::vector<Element>& elements){
    require_active();
    try{transmit(builder_.snapshot(generation,elements));}
    catch(...){faulted_=true;throw;}
  }
  void revise(const std::vector<Operation>& changes,const std::string& list_start=""){
    require_active();
    try{transmit(builder_.revise(changes,list_start));}
    catch(...){faulted_=true;throw;}
  }
  void finish(std::uint32_t timeout_ms=5000){
    require_active();
    try {
      // All data and commit-fence samples must be acknowledged before Dispose.
      await_ack(timeout_ms);
      const UmaaDdsSampleV1 s{nullptr,0,key_.c_str(),1,UMAA_DDS_EVENT_DISPOSE};
      if(domain_->dispose(elements_writer_,s)!=UMAA_DDS_OK||
         domain_->dispose(metadata_writer_,s)!=UMAA_DDS_OK)
        throw std::runtime_error("split Collection Dispose unconfirmed");
      await_ack(timeout_ms);
      finished_=true;
    }catch(...){faulted_=true;throw;}
  }
  std::uint64_t sequence() const noexcept{return builder_.sequence();}
  bool faulted() const noexcept{return faulted_;}
private:
  void require_active() const {
    if(faulted_||finished_)throw std::logic_error("split Collection writer closed/faulted");
  }
  void await_ack(std::uint32_t timeout){
    if(domain_->wait_for_acks(elements_writer_,timeout)!=UMAA_DDS_OK ||
       domain_->wait_for_acks(metadata_writer_,timeout)!=UMAA_DDS_OK)
      throw std::runtime_error("split Collection reliable DDS ACK unconfirmed");
  }
  void transmit(const Batch& batch){
    // The two frames carry identical metadata so the receiver can prove
    // correlation. Metadata's operations must be empty. This synthetic
    // envelope is NOT an official UMAA per-Topic representation.
    if(batch.sequence>static_cast<std::uint64_t>(INT64_MAX))
      throw std::overflow_error("split Collection timestamp overflow");
    auto meta=batch;meta.operations.clear();
    const auto elem_bytes=CollectionFrameCodec<Service>::encode(batch);
    const auto meta_bytes=CollectionFrameCodec<Service>::encode(meta);
    const auto send=[&](void* writer,const std::vector<std::uint8_t>& bytes){
      const UmaaDdsSampleV1 s{bytes.data(),static_cast<std::uint32_t>(bytes.size()),
        key_.c_str(),static_cast<std::int64_t>(batch.sequence),UMAA_DDS_EVENT_DATA};
      if(domain_->write(writer,s)!=UMAA_DDS_OK)
        throw std::runtime_error("ambiguous split Collection delivery; no replay");
    };
    send(elements_writer_,elem_bytes);
    send(metadata_writer_,meta_bytes);
  }
  std::shared_ptr<DdsBackendPlugin> backend_;
  std::shared_ptr<DdsBackendPlugin::Domain> domain_;
  std::string key_;
  CollectionBatchBuilder<Value> builder_;
  void *elements_writer_{},*metadata_writer_{};
  bool faulted_{},finished_{};
};

template<class Service> class DdsSplitCollectionConsumer final {
public:
  using Value=typename Service::Value;
  using Batch=CoherentCollectionBatch<Value>;
  DdsSplitCollectionConsumer(const std::string& plugin,std::uint32_t domain,
                              std::string key,std::size_t maximum=1024)
    :backend_(std::make_shared<DdsBackendPlugin>(plugin)),
     domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
       backend_->open_domain(domain).release())),
     key_(std::move(key)),joiner_(maximum) {
    if(key_.empty())throw std::invalid_argument("missing split Collection key");
    element_ctx_={this,false};metadata_ctx_={this,true};
    elements_reader_=domain_->reader(split_wire::topic<Service>(Service::elements_topic),
      &DdsSplitCollectionConsumer::receive,&element_ctx_);
    try {
      metadata_reader_=domain_->reader(split_wire::topic<Service>(Service::metadata_topic),
        &DdsSplitCollectionConsumer::receive,&metadata_ctx_);
    }catch(...){domain_->close_reader(elements_reader_);elements_reader_=nullptr;throw;}
  }
  ~DdsSplitCollectionConsumer(){
    if(metadata_reader_)domain_->close_reader(metadata_reader_);
    if(elements_reader_)domain_->close_reader(elements_reader_);
  }
  DdsSplitCollectionConsumer(const DdsSplitCollectionConsumer&)=delete;
  DdsSplitCollectionConsumer& operator=(const DdsSplitCollectionConsumer&)=delete;
  void expect_snapshot(std::uint64_t generation){
    joiner_.expect_snapshot(generation);faulted_=false;last_error_.clear();
  }
  int poll(std::uint32_t limit=16){
    if(faulted_)throw std::logic_error("split Collection needs trusted resync");
    if(!limit)throw std::invalid_argument("zero DDS poll budget");
    auto a=domain_->poll(elements_reader_,limit);
    if(a<0||faulted_)return quarantine();
    auto b=domain_->poll(metadata_reader_,limit);
    if(b<0||faulted_)return quarantine();
    return a+b;
  }
  std::optional<std::vector<Value>> snapshot() const{return joiner_.snapshot();}
  std::uint64_t sequence() const noexcept{return joiner_.sequence();}
  bool pending() const noexcept{return joiner_.pending();}
  bool disposed() const noexcept{return joiner_.disposed();}
  bool faulted() const noexcept{return faulted_||joiner_.faulted();}
private:
  struct Context{DdsSplitCollectionConsumer* self;bool metadata;};
  static void receive(void* user,const UmaaDdsSampleV1* sample) noexcept {
    auto* context=static_cast<Context*>(user);
    if(!context||!context->self)return;
    auto& self=*context->self;
    try{
      if(!sample||!sample->instance_key||
         self.key_!=sample->instance_key)
        throw std::runtime_error("split Collection DDS instance key mismatch");
      if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE)
        self.joiner_.dispose_part(context->metadata);
      else if(sample->event_kind==UMAA_DDS_EVENT_DATA){
        auto batch=CollectionFrameCodec<Service>::decode(
          sample->payload,sample->payload_size);
        if(context->metadata)self.joiner_.ingest_metadata(std::move(batch));
        else self.joiner_.ingest_elements(std::move(batch));
      }else throw std::runtime_error("unexpected split Collection lifecycle");
    }catch(const std::exception& e){self.faulted_=true;self.last_error_=e.what();}
    catch(...){self.faulted_=true;self.last_error_="split callback unknown error";}
  }
  int quarantine(){
    faulted_=true;
    throw std::runtime_error("split Collection DDS reader fault: "+last_error_);
  }
  std::shared_ptr<DdsBackendPlugin> backend_;
  std::shared_ptr<DdsBackendPlugin::Domain> domain_;
  std::string key_;
  SplitCollectionJoiner<Service> joiner_;
  Context element_ctx_{},metadata_ctx_{};
  void *elements_reader_{},*metadata_reader_{};
  bool faulted_{};
  std::string last_error_;
};
} // namespace umaa
