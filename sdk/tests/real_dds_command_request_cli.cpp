// Independent DDS Command request -> device Component -> Status/Ack -> client.
// SYNTHETIC DemoCommandEvent IDL. Not official UMAA TypeSupport/QoS.
#include "umaa/component_command_provider.hpp"
#include "umaa/dds_command_event_transport.hpp"
#include "umaa/remote_command_monitor.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
struct Service {
 static constexpr const char* id="Demo::EO::AsyncEngineControl";
 static constexpr const char* qos_profile="demo-reliable";
 static constexpr const char* status_topic="demo/AsyncEngineStatus";
 static constexpr const char* status_schema="demo:async-status-v1";
 static constexpr const char* ack_topic="demo/AsyncEngineAck";
 static constexpr const char* ack_schema="demo:async-ack-v1";
 struct Command {int32_t rpm{};};
 static std::vector<uint8_t> encode_command(const Command& c) {
   std::vector<uint8_t> out(4);std::memcpy(out.data(),&c.rpm,4);return out;
 }
 static std::vector<uint8_t> encode_status(const umaa::Identity&,
                                            const umaa::Event& e) {
   const std::string names[]={"ISSUED","COMMANDED","EXECUTING","COMPLETED","FAILED","CANCELED"};
   int32_t code=-1;
   for(int32_t i=0;i<6;i++)if(names[i]==e.detail)code=i;
   if(code<0)throw std::runtime_error("invalid Command Status");
   std::vector<uint8_t> out(4);std::memcpy(out.data(),&code,4);return out;
 }
 static std::vector<uint8_t> encode_ack(const umaa::Identity&,
                                         const Command& c){return encode_command(c);}
 static std::string instance_key(const umaa::Identity& id){return id.session;}
};
constexpr const char* request_topic="demo/AsyncEngineCommand";
constexpr const char* request_schema="demo:async-command-v1";
static UmaaDdsTopicV1 binding(const char* topic,const char* schema) {
 return {topic,schema,Service::qos_profile,
    UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
}
struct ProviderReceiver {
 umaa::ComponentCommandProvider<Service>* provider{};
 bool received{false},completed{false},disposed{false},failed{false};
 bool cancel_mode{false},update_mode{false},updated{false};
 static void on_command(void* ctx,const UmaaDdsSampleV1* s) {
   auto* p=static_cast<ProviderReceiver*>(ctx);
   if(!s||!s->instance_key||std::string(s->instance_key)!="17") {
     p->failed=true;return;
   }
   try {
     if(s->event_kind==UMAA_DDS_EVENT_DISPOSE){
       const auto events=p->provider->command_disposed("controller","17");
       if(p->cancel_mode) {
         if(events.size()!=3 || events[0].detail!="CANCELED")p->failed=true;
         else p->completed=true;
       }else if(!p->completed || events.size()!=2) p->failed=true;
       p->disposed=true;
     }else if(s->event_kind==UMAA_DDS_EVENT_DATA&&!p->received &&
              s->payload&&s->payload_size==sizeof(int32_t)){
       int32_t rpm=0;std::memcpy(&rpm,s->payload,sizeof(rpm));
       const auto events=p->provider->receive(
         {"controller","vehicle","17",s->source_timestamp_ns},{rpm});
       if(events.size()!=4)p->failed=true;
       p->received=true;
     }else if(s->event_kind==UMAA_DDS_EVENT_DATA&&p->received&&
              p->update_mode&&!p->updated&&s->payload&&
              s->payload_size==sizeof(int32_t)){
       int32_t rpm=0;std::memcpy(&rpm,s->payload,sizeof(rpm));
       const auto events=p->provider->receive(
         {"controller","vehicle","17",s->source_timestamp_ns},{rpm});
       if(rpm!=1900||events.size()!=4)p->failed=true;
       p->updated=true;
     }else p->failed=true;
   }catch(...){p->failed=true;}
 }
};
struct ClientObservation {
 umaa::RemoteCommandMonitor* sessions{};
 bool is_ack{};
 int data{0},disposes{0};
 bool failed{false};
 static void on_response(void* ctx,const UmaaDdsSampleV1* s){
   auto* p=static_cast<ClientObservation*>(ctx);
   if(!s||!s->instance_key||std::string(s->instance_key)!="17"){
     p->failed=true;return;
   }
   if(s->event_kind==UMAA_DDS_EVENT_DISPOSE){
     bool ok=p->is_ack?p->sessions->disposed_ack("17","vehicle"):
                       p->sessions->disposed_status("17","vehicle");
     if(!ok)p->failed=true;
     ++p->disposes;return;
   }
   if(s->event_kind!=UMAA_DDS_EVENT_DATA||!s->payload||
      s->payload_size!=sizeof(int32_t)){p->failed=true;return;}
   int32_t value=0;std::memcpy(&value,s->payload,sizeof(value));
   if(p->is_ack){
     if(value!=1800||!p->sessions->ack("17","vehicle",s->source_timestamp_ns))
       p->failed=true;
   }else{
     const umaa::State kinds[]={umaa::State::ISSUED,umaa::State::COMMANDED,
       umaa::State::EXECUTING,umaa::State::COMPLETED};
     if(value<0||value>3 ||
        !p->sessions->status("17","vehicle",kinds[value],s->source_timestamp_ns))
       p->failed=true;
   }
   ++p->data;
 }
};
int main(int argc,char** argv){
 if(argc!=4)return 2;
 try{
   const auto mode=std::string(argv[1]);
   auto plugin=std::make_shared<umaa::DdsBackendPlugin>(argv[2]);
   auto domain=std::shared_ptr<umaa::DdsBackendPlugin::Domain>(
     plugin->open_domain(44).release());
   if(mode=="server"||mode=="server-cancel"||mode=="server-update"){
     auto runtime=std::make_shared<umaa::internal::ComponentRuntime>();
     runtime->load_component(argv[3]);
     // Synthetic fixture's consumer polls; keep terminal data available
     // briefly before Dispose rather than assuming reliable ACK means read().
     umaa::DdsCommandEventTransport<Service> wire(
       plugin,domain,std::chrono::milliseconds(180));
     umaa::ComponentCommandProvider<Service> provider("vehicle",runtime,wire);
     ProviderReceiver receiver{&provider};
     receiver.cancel_mode=(mode=="server-cancel");
     receiver.update_mode=(mode=="server-update");
     auto* request_reader=domain->reader(binding(request_topic,request_schema),
                                         &ProviderReceiver::on_command,&receiver);
     for(int i=0;i<480&&!receiver.disposed&&!receiver.failed;i++){
       if(domain->poll(request_reader,20)<0){receiver.failed=true;break;}
       if(receiver.received&&!receiver.completed&&!receiver.cancel_mode&&
          (!receiver.update_mode||receiver.updated)){
         auto events=provider.poll_device("17");
         if(!events.empty()&&events[0].detail=="COMPLETED")receiver.completed=true;
       }
       std::this_thread::sleep_for(std::chrono::milliseconds(15));
     }
     bool ok=receiver.received&&receiver.completed&&receiver.disposed&&!receiver.failed&&
             (!receiver.update_mode||receiver.updated);
     if(ok)wire.wait_for_acks(5000);
     std::this_thread::sleep_for(std::chrono::milliseconds(250));
     domain->close_reader(request_reader);
     std::cout<<"SERVER cancel_mode="<<receiver.cancel_mode
              <<" update_mode="<<receiver.update_mode<<" updated="<<receiver.updated
              <<" command="<<receiver.received<<" completed="<<receiver.completed
              <<" request_dispose="<<receiver.disposed<<"\n";
     return ok?0:3;
   }
   if(mode=="client"){
     umaa::RemoteCommandMonitor sessions;
     sessions.issue({"controller","vehicle","17",0});
     ClientObservation status{&sessions,false},ack{&sessions,true};
     auto* status_reader=domain->reader(binding(Service::status_topic,Service::status_schema),
                                         &ClientObservation::on_response,&status);
     auto* ack_reader=domain->reader(binding(Service::ack_topic,Service::ack_schema),
                                      &ClientObservation::on_response,&ack);
     auto* command_writer=domain->writer(binding(request_topic,request_schema));
     int32_t rpm=1800;
     UmaaDdsSampleV1 request{reinterpret_cast<const uint8_t*>(&rpm),4,
                             "17",1,UMAA_DDS_EVENT_DATA};
     if(domain->write(command_writer,request)!=UMAA_DDS_OK)return 4;
     bool terminal=false;
     for(int i=0;i<320&&!terminal&&!status.failed&&!ack.failed;i++){
       if(domain->poll(status_reader,20)<0||domain->poll(ack_reader,20)<0)
         return 5;
       terminal=sessions.result("17")==umaa::RemoteCommandResult::Completed;
       std::this_thread::sleep_for(std::chrono::milliseconds(20));
     }
     if(!terminal)return 6;
     std::this_thread::sleep_for(std::chrono::milliseconds(150));
     if(domain->dispose(command_writer,request)!=UMAA_DDS_OK)return 7;
     // The final reliable writer ACK checks all request samples including Dispose.
     const int32_t final_ack=domain->wait_for_acks(command_writer,5000);
     for(int i=0;i<320 && (status.disposes==0||ack.disposes==0);i++){
       if(domain->poll(status_reader,20)<0||domain->poll(ack_reader,20)<0)return 8;
       std::this_thread::sleep_for(std::chrono::milliseconds(20));
     }
     const bool ok=final_ack==UMAA_DDS_OK&&!status.failed&&!ack.failed&&
       status.data==4&&ack.data==1&&status.disposes==1&&ack.disposes==1&&
       sessions.result("17")==umaa::RemoteCommandResult::Completed&&
       sessions.release("17");
     std::this_thread::sleep_for(std::chrono::milliseconds(200));
     domain->close_writer(command_writer);
     domain->close_reader(status_reader);domain->close_reader(ack_reader);
     std::cout<<"CLIENT complete="<<terminal<<" status="<<status.data
              <<" ack="<<ack.data<<" disposes="<<(status.disposes+ack.disposes)
              <<" request_final_ack="<<final_ack<<"\n";
     return ok?0:9;
   }
   return 10;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 11;}
}
