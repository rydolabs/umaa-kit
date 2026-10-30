// Synthetic independent-process Command/Ack/Status facade test.
// This tests real DDS transport only; it is NOT official UMAA TypeSupport.
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
struct AsyncCommand {
 static constexpr const char* id="Demo::EO::AsyncEngineControl";
 static constexpr const char* status_topic="demo/AsyncEngineStatus";
 static constexpr const char* status_schema="demo:async-status-v1";
 static constexpr const char* ack_topic="demo/AsyncEngineAck";
 static constexpr const char* ack_schema="demo:async-ack-v1";
 static constexpr const char* qos_profile="demo-reliable";
 struct Command {int32_t rpm{};};
 static std::vector<uint8_t> encode_command(const Command& c) {
   std::vector<uint8_t> p(4);std::memcpy(p.data(),&c.rpm,4);return p;
 }
 static std::string instance_key(const umaa::Identity& id){return id.session;}
 static std::vector<uint8_t> encode_status(const umaa::Identity&,
                                             const umaa::Event& e) {
   const std::string states[]={"ISSUED","COMMANDED","EXECUTING","COMPLETED","FAILED","CANCELED"};
   int32_t value=-1;
   for(int32_t i=0;i<6;++i)if(e.detail==states[i])value=i;
   if(value<0)throw std::invalid_argument("unknown synthetic Status");
   std::vector<uint8_t> out(4);std::memcpy(out.data(),&value,4);return out;
 }
 static std::vector<uint8_t> encode_ack(const umaa::Identity&,
                                          const Command& c){return encode_command(c);}
};
struct Receiver {
 umaa::RemoteCommandMonitor* sessions;
 int statuses{0},acks{0},disposed{0};
 bool bad{false};
 bool ack_topic{false};
 Receiver(bool is_ack,umaa::RemoteCommandMonitor& monitor)
   :sessions(&monitor),ack_topic(is_ack) {}

 static void receive(void* context,const UmaaDdsSampleV1* sample){
   auto& self=*static_cast<Receiver*>(context);
   if(sample)std::cerr<<"COMMAND_RX "<<(self.ack_topic?"ACK":"STATUS")
     <<" event="<<sample->event_kind
     <<" key="<<(sample->instance_key?sample->instance_key:"(null)")
     <<" ts="<<sample->source_timestamp_ns<<"\n";
   if(!sample||!sample->instance_key||std::string(sample->instance_key)!="17"){
     self.bad=true;return;
   }
   if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE){
     const bool accepted=self.ack_topic
       ?self.sessions->disposed_ack("17","vehicle")
       :self.sessions->disposed_status("17","vehicle");
     if(!accepted)self.bad=true;
     ++self.disposed;return;
   }
   if(sample->event_kind!=UMAA_DDS_EVENT_DATA||!sample->payload||
      sample->payload_size!=sizeof(int32_t)){self.bad=true;return;}
   int32_t code=0;std::memcpy(&code,sample->payload,sizeof(code));
   if(self.ack_topic){
     if(code!=1800 || !self.sessions->ack("17","vehicle",sample->source_timestamp_ns))
       self.bad=true;
     ++self.acks;
   }else{
     if(code<0||code>3){self.bad=true;return;}
     const umaa::State states[]={
       umaa::State::ISSUED,umaa::State::COMMANDED,
       umaa::State::EXECUTING,umaa::State::COMPLETED};
     if(!self.sessions->status("17","vehicle",states[code],sample->source_timestamp_ns))
       self.bad=true;
     ++self.statuses;
   }
 }
};
int main(int argc,char** argv){
 if(argc!=4)return 2;
 try{
   const std::string mode=argv[1];
   if(mode=="publisher"){
     auto runtime=std::make_shared<umaa::internal::ComponentRuntime>();
     runtime->load_component(argv[3]);
     auto plugin=std::make_shared<umaa::DdsBackendPlugin>(argv[2]);
     auto domain=std::shared_ptr<umaa::DdsBackendPlugin::Domain>(plugin->open_domain(44).release());
     // Both tested vendor bindings must explicitly reject unverified
     // GROUP coherent/ordered presentation instead of silently downgrading.
     const UmaaDdsTopicV1 required{
       AsyncCommand::status_topic,AsyncCommand::status_schema,
       AsyncCommand::qos_profile,
       UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_COHERENT_GROUP|UMAA_DDS_CAP_ORDERED_ACCESS};
     bool rejected=false;
     try {auto* w=domain->writer(required);domain->close_writer(w);}
     catch(const std::runtime_error&){rejected=true;}
     if(!rejected)return 12;
     umaa::DdsCommandEventTransport<AsyncCommand> wire(plugin,domain);
     umaa::ComponentCommandProvider<AsyncCommand> provider("vehicle",runtime,wire);
     auto events=provider.receive({"controller","vehicle","17",0},{1800});
     if(events.size()!=4)return 3;
     if(provider.poll_device("17")[0].kind!=umaa::Event::Kind::Ignored)return 4;
     if(provider.poll_device("17")[0].kind!=umaa::Event::Kind::Ignored)return 5;
     if(provider.poll_device("17")[0].detail!="COMPLETED")return 6;
     std::this_thread::sleep_for(std::chrono::milliseconds(150));
     if(provider.command_disposed("controller","17").size()!=2)return 7;
     wire.wait_for_acks(5000);
     std::this_thread::sleep_for(std::chrono::milliseconds(200));
     std::cout<<"PASS synthetic CommandEngine host events over selected DDS Backend\n";return 0;
   }
   if(mode=="subscriber"){
     auto plugin=std::make_shared<umaa::DdsBackendPlugin>(argv[2]);
     auto domain=std::shared_ptr<umaa::DdsBackendPlugin::Domain>(plugin->open_domain(44).release());
     umaa::RemoteCommandMonitor monitor;
     monitor.issue({"controller","vehicle","17",0});
     Receiver status(false,monitor),ack(true,monitor);
     const UmaaDdsTopicV1 st{AsyncCommand::status_topic,AsyncCommand::status_schema,
       AsyncCommand::qos_profile,UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
     const UmaaDdsTopicV1 ak{AsyncCommand::ack_topic,AsyncCommand::ack_schema,
       AsyncCommand::qos_profile,UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
     auto* r1=domain->reader(st,&Receiver::receive,&status);
     auto* r2=domain->reader(ak,&Receiver::receive,&ack);
     for(int i=0;i<280 && (status.disposed<1||ack.disposed<1);++i){
       if(domain->poll(r1,20)<0||domain->poll(r2,20)<0)break;
       std::this_thread::sleep_for(std::chrono::milliseconds(25));
     }
     bool success=!status.bad&&!ack.bad&&status.statuses==4&&ack.acks==1&&
                  status.disposed==1&&ack.disposed==1&&
                  monitor.result("17")==umaa::RemoteCommandResult::Completed &&
                  monitor.release("17");
     std::this_thread::sleep_for(std::chrono::milliseconds(300));
     domain->close_reader(r1);domain->close_reader(r2);
     std::cout<<"SUBSCRIBER synthetic command statuses="<<status.statuses
              <<" ack="<<ack.acks<<" disposes="<<(status.disposed+ack.disposed)<<"\n";
     return success?0:8;
   }
   return 9;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 10;}
}
