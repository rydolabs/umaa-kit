// Typed remote Command client over one reviewed DDS Backend binding.
// The caller uses Service::Command only; no vendor Reader/Writer API.
// This is development-scope transport; generated official Service codecs
// and normative QoS are required before UMAA conformance can be claimed.
#pragma once
#include "umaa/dds_backend_loader.hpp"
#include "umaa/remote_command_monitor.hpp"
#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
namespace umaa {
template<class Service> class RemoteCommandClient final {
public:
 using Command=typename Service::Command;
 explicit RemoteCommandClient(const std::string& backend,std::uint32_t domain_id,
                              std::string source,std::string destination)
   :backend_(std::make_shared<DdsBackendPlugin>(backend)),
    domain_(std::shared_ptr<DdsBackendPlugin::Domain>(
      backend_->open_domain(domain_id).release())),
    source_(std::move(source)),destination_(std::move(destination)) {
   if(source_.empty()||destination_.empty())
     throw std::invalid_argument("Command source and destination are required");
   status_context_={this,false};
   ack_context_={this,true};
   try {
     status_reader_=domain_->reader(topic(Service::status_topic,Service::status_schema),
                                    &RemoteCommandClient::receive,&status_context_);
     ack_reader_=domain_->reader(topic(Service::ack_topic,Service::ack_schema),
                                 &RemoteCommandClient::receive,&ack_context_);
     command_writer_=domain_->writer(topic(Service::command_topic,Service::command_schema));
   }catch(...) {
     domain_->close_writer(command_writer_);
     domain_->close_reader(ack_reader_);
     domain_->close_reader(status_reader_);
     throw;
   }
 }
 ~RemoteCommandClient(){
   domain_->close_writer(command_writer_);
   domain_->close_reader(ack_reader_);
   domain_->close_reader(status_reader_);
 }
 RemoteCommandClient(const RemoteCommandClient&)=delete;
 RemoteCommandClient& operator=(const RemoteCommandClient&)=delete;
 void send(const Command& command,const std::string& session){
   require_active();
   if(session.empty())throw std::invalid_argument("Command session required");
   const auto bytes=Service::encode_command(command);
   if(bytes.empty()||bytes.size()>UINT32_MAX)throw std::length_error("invalid Command payload");
   monitor_.issue({source_,destination_,session,0});
   commands_.emplace(session,command);
   UmaaDdsSampleV1 sample{bytes.data(),static_cast<std::uint32_t>(bytes.size()),
     session.c_str(),1,UMAA_DDS_EVENT_DATA};
   if(domain_->write(command_writer_,sample)!=UMAA_DDS_OK){
     faulted_=true;monitor_.ambiguous_delivery(session);
     throw std::runtime_error("ambiguous Command delivery; never replay");
   }
 }
 std::int32_t poll(std::uint32_t maximum=32){
   require_active();
   if(maximum==0)throw std::invalid_argument("zero Command poll limit");
   auto a=domain_->poll(status_reader_,maximum);
   auto b=domain_->poll(ack_reader_,maximum);
   if(a<0||b<0||faulted_){
     faulted_=true;throw std::runtime_error("Command response reader faulted");
   }
   return a+b;
 }
 RemoteCommandResult result(const std::string& session) const {
   require_active();
   return monitor_.result(session);
 }
 RemoteCommandResult wait_for(const std::string& session,
       std::chrono::milliseconds timeout){
   if(timeout.count()<0)throw std::invalid_argument("negative Command timeout");
   const auto end=std::chrono::steady_clock::now()+timeout;
   do {
     poll();
     const auto value=result(session);
     if(value!=RemoteCommandResult::Pending)return value;
     if(std::chrono::steady_clock::now()>=end)return value;
     std::this_thread::sleep_for(std::chrono::milliseconds(20));
   }while(true);
 }
 // Dispose the initiating Command ONLY after confirmed terminal status+ACK.
 // The provider then disposes its Status and Ack instances. A failed cleanup
 // faults the transport instead of inventing success.
 void finish(const std::string& session,std::chrono::milliseconds timeout){
   require_active();
   if(monitor_.result(session)==RemoteCommandResult::Pending)
     throw std::logic_error("cannot finish unconfirmed Command");
   if(timeout.count()<0)throw std::invalid_argument("negative cleanup timeout");
   if(!disposed_.emplace(session,true).second)
     throw std::logic_error("Command was already disposed");
   UmaaDdsSampleV1 request{nullptr,0,session.c_str(),1,UMAA_DDS_EVENT_DISPOSE};
   if(domain_->dispose(command_writer_,request)!=UMAA_DDS_OK ||
      domain_->wait_for_acks(command_writer_,
        static_cast<std::uint32_t>(timeout.count()))!=UMAA_DDS_OK){
     faulted_=true;throw std::runtime_error("unconfirmed Command disposal");
   }
   const auto end=std::chrono::steady_clock::now()+timeout;
   for(;;){
     poll();
     if(monitor_.release(session)) {
       commands_.erase(session);
       return;
     }
     if(std::chrono::steady_clock::now()>=end){
       faulted_=true;throw std::runtime_error("missing provider response disposal");
     }
     std::this_thread::sleep_for(std::chrono::milliseconds(20));
   }
 }
 bool faulted() const noexcept{return faulted_;}
private:
 struct CallbackContext{RemoteCommandClient* self{};bool ack{};};
 static UmaaDdsTopicV1 topic(const char* name,const char* schema){
   return {name,schema,Service::qos_profile,
     UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
 }
 static void receive(void* user,const UmaaDdsSampleV1* s) noexcept {
   auto* ctx=static_cast<CallbackContext*>(user);
   if(!ctx||!ctx->self)return;
   auto& self=*ctx->self;
   try {
     if(!s||!s->instance_key||!s->instance_key[0])
       throw std::runtime_error("invalid Command response key");
     const std::string session=s->instance_key;
     if(s->event_kind==UMAA_DDS_EVENT_DISPOSE){
       const bool accepted=ctx->ack?
         self.monitor_.disposed_ack(session,self.destination_):
         self.monitor_.disposed_status(session,self.destination_);
       if(!accepted)throw std::runtime_error("unexpected Command disposal");
     }else if(s->event_kind==UMAA_DDS_EVENT_DATA){
       if(!s->payload)throw std::runtime_error("missing Command data");
       bool accepted=false;
       if(ctx->ack){
         const auto it=self.commands_.find(session);
         if(it==self.commands_.end() ||
            !Service::ack_matches(s->payload,s->payload_size,it->second))
           throw std::runtime_error("Command Ack correlation failed");
         accepted=self.monitor_.ack(session,self.destination_,s->source_timestamp_ns);
       }else{
         const auto status=Service::decode_status(s->payload,s->payload_size);
         if(!status)throw std::runtime_error("invalid Command Status codec");
         accepted=self.monitor_.status(session,self.destination_,*status,
                                         s->source_timestamp_ns);
       }
       if(!accepted)throw std::runtime_error("stale/conflicting Command response");
     }else throw std::runtime_error("unexpected Command event kind");
   }catch(...){self.faulted_=true;}
 }
 void require_active() const {
   if(faulted_)throw std::logic_error("remote Command channel faulted");
 }
 std::shared_ptr<DdsBackendPlugin> backend_;
 std::shared_ptr<DdsBackendPlugin::Domain> domain_;
 std::string source_,destination_;
 RemoteCommandMonitor monitor_;
 std::unordered_map<std::string,Command> commands_;
 std::unordered_map<std::string,bool> disposed_;
 CallbackContext status_context_,ack_context_;
 void* status_reader_{},*ack_reader_{},*command_writer_{};
 bool faulted_{false};
};
} // namespace umaa
