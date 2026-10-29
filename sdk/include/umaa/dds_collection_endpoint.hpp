// Development-only atomic Collection frames over reliable DDS samples.
// Each logical revision is one bounded sample; no native DDS GROUP coherence
// or official UMAA LargeSet/LargeList wire conformance is claimed.
#pragma once
#include "umaa/collection_batch_builder.hpp"
#include "umaa/collection_frame_codec.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
namespace collection_wire {
template<class Service> UmaaDdsTopicV1 topic(){
 return {Service::topic_name,Service::schema_id,Service::qos_profile,
         UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
}
} // namespace collection_wire

template<class Service> class DdsCollectionPublisher final {
public:
 using Value=typename Service::Value;
 using Batch=CoherentCollectionBatch<Value>;
 using Operation=CollectionOperation<Value>;
 DdsCollectionPublisher(const std::string& path,std::uint32_t domain,
                        std::string key,std::size_t maximum=1024)
  :backend_(std::make_shared<DdsBackendPlugin>(path)),
   domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
     backend_->open_domain(domain).release())),
   key_(std::move(key)),builder_(Service::kind,Service::collection_id,maximum) {
   if(key_.empty())throw std::invalid_argument("Collection key missing");
   writer_=domain_->writer(collection_wire::topic<Service>());
 }
 ~DdsCollectionPublisher(){domain_->close_writer(writer_);}
 DdsCollectionPublisher(const DdsCollectionPublisher&)=delete;
 DdsCollectionPublisher& operator=(const DdsCollectionPublisher&)=delete;
 void snapshot(std::uint64_t generation,
               const std::vector<CollectionElement<Value>>& elements){
   require_active();
   transmit(builder_.snapshot(generation,elements));
 }
 void revise(const std::vector<Operation>& operations,
             const std::string& list_start=""){
   require_active();
   transmit(builder_.revise(operations,list_start));
 }
 void finish(std::uint32_t timeout_ms=5000){
   require_active();
   UmaaDdsSampleV1 sample{nullptr,0,key_.c_str(),1,UMAA_DDS_EVENT_DISPOSE};
   if(domain_->dispose(writer_,sample)!=UMAA_DDS_OK||
      domain_->wait_for_acks(writer_,timeout_ms)!=UMAA_DDS_OK){
     faulted_=true;throw std::runtime_error("unconfirmed Collection Dispose");
   }
   finished_=true;
 }
 std::uint64_t sequence() const noexcept{return builder_.sequence();}
 bool faulted() const noexcept{return faulted_;}
private:
 void require_active() const{
   if(faulted_||finished_)throw std::logic_error("Collection publisher closed or faulted");
 }
 void transmit(const Batch& batch){
   const auto data=CollectionFrameCodec<Service>::encode(batch);
   UmaaDdsSampleV1 sample{data.data(),
      static_cast<std::uint32_t>(data.size()),key_.c_str(),1,UMAA_DDS_EVENT_DATA};
   if(domain_->write(writer_,sample)!=UMAA_DDS_OK){
     faulted_=true;
     throw std::runtime_error("ambiguous Collection transaction delivery; no replay");
   }
 }
 std::shared_ptr<DdsBackendPlugin> backend_;
 std::shared_ptr<DdsBackendPlugin::Domain> domain_;
 std::string key_;
 CollectionBatchBuilder<Value> builder_;
 void* writer_{};
 bool faulted_{false},finished_{false};
};

template<class Service> class DdsCollectionConsumer final {
public:
 using Value=typename Service::Value;
 explicit DdsCollectionConsumer(const std::string& path,std::uint32_t domain,
                                std::string key,std::size_t maximum=1024)
  :backend_(std::make_shared<DdsBackendPlugin>(path)),
   domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
      backend_->open_domain(domain).release())),
   key_(std::move(key)),assembler_(Service::kind,maximum) {
   if(key_.empty())throw std::invalid_argument("Collection key missing");
   reader_=domain_->reader(collection_wire::topic<Service>(),
                           &DdsCollectionConsumer::on_sample,this);
 }
 ~DdsCollectionConsumer(){domain_->close_reader(reader_);}
 DdsCollectionConsumer(const DdsCollectionConsumer&)=delete;
 DdsCollectionConsumer& operator=(const DdsCollectionConsumer&)=delete;
 // A full snapshot generation MUST be trusted by the caller explicitly.
 // This call is also the only way out of a corrupted/gapped transaction.
 void expect_snapshot(std::uint64_t generation){
   assembler_.expect_snapshot(generation);
   faulted_=false;disposed_=false;
 }
 int poll(std::uint32_t max_samples=1){
   if(faulted_)throw std::logic_error("Collection stream needs trusted resync");
   if(!max_samples)throw std::invalid_argument("invalid Collection poll budget");
   const auto received=domain_->poll(reader_,max_samples);
   if(received<0||faulted_){
     assembler_.fail_unknown_delivery();
     faulted_=true;
     throw std::runtime_error("Collection frame invalid, gap or DDS reader failure");
   }
   return received;
 }
 std::optional<std::vector<Value>> snapshot() const {
   return assembler_.snapshot();
 }
 std::uint64_t sequence() const noexcept{return assembler_.sequence();}
 bool disposed() const noexcept{return disposed_;}
 bool faulted() const noexcept{return faulted_;}
private:
 static void on_sample(void* ctx,const UmaaDdsSampleV1* sample) noexcept {
   auto* self=static_cast<DdsCollectionConsumer*>(ctx);
   if(!self)return;
   try{
     if(!sample||!sample->instance_key||
        self->key_!=sample->instance_key)
       throw std::runtime_error("Collection key mismatch");
     if(sample->event_kind==UMAA_DDS_EVENT_DATA){
       const auto batch=CollectionFrameCodec<Service>::decode(
           sample->payload,sample->payload_size);
       if(batch.metadata.collection_id!=Service::collection_id)
         throw std::runtime_error("Collection type identity mismatch");
       if(batch.sequence==1)self->assembler_.apply_snapshot(batch);
       else self->assembler_.apply_delta(batch);
     }else if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE){
       self->disposed_=true;
       self->assembler_.fail_unknown_delivery();
     }else throw std::runtime_error("unrecognized Collection lifecycle");
   }catch(...){
     self->faulted_=true;
     self->assembler_.fail_unknown_delivery();
   }
 }
 std::shared_ptr<DdsBackendPlugin> backend_;
 std::shared_ptr<DdsBackendPlugin::Domain> domain_;
 std::string key_;
 CoherentCollectionReceiver<Value> assembler_;
 void* reader_{};
 bool faulted_{false},disposed_{false};
};
} // namespace umaa
