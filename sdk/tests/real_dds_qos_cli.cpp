// Same C++ QoS client for CycloneDDS and FastDDS via optional plugin ABI.
// Synthetic Battery only; intentionally no claim of official UMAA service QoS.
#include "umaa/dds_backend_loader.hpp"
#include "umaa/application.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
struct BatteryStatus {
 static constexpr bool is_command=false;
 static constexpr const char* id="Demo::EO::BatteryStatus";
 static constexpr const char* topic_name="demo/BatteryStatus";
 static constexpr const char* schema_id="demo:battery-v1";
 static constexpr const char* qos_profile="demo-reliable";
 struct Report{int32_t level_pct{};};
 static bool decode_report(const uint8_t* p,size_t n,Report& out){
  if(!p || n!=sizeof(int32_t))return false;
  std::memcpy(&out.level_pct,p,n);return true;
 }
};
std::string config_file(const std::string& mode,const char* backend,const char* component){
 auto path=std::filesystem::current_path()/("umaa_explicit_qos_"+mode+".yml");
 std::ofstream out(path,std::ios::trunc);
 if(!out)throw std::runtime_error("cannot create test runtime config");
 out << "middleware:\n  backend: \""<<std::filesystem::absolute(backend).string()
     <<"\"\n  domain_id: 91\nqos_profiles:\n  demo-reliable:\n"
     <<"    reliability: reliable\n    durability: volatile\n    history: keep_last\n"
     <<"    depth: 8\n    presentation_scope: instance\n"
     <<"    ordered_access: false\n    coherent_access: false\n"
     <<"    partition: umaa-v2-fixture\n";
 if(mode=="publisher")
   out<<"modules:\n  - \""<<std::filesystem::absolute(component).string()<<"\"\n";
 out.close();
 return path.string();
}
struct Seen {bool data=false;bool disposed=false;};
void on_event(void* user,const UmaaDdsSampleV1* s) {
 auto& seen=*static_cast<Seen*>(user);
 if(s->event_kind==UMAA_DDS_EVENT_DATA && s->payload_size==4 && s->instance_key &&
    std::string(s->instance_key)=="17") {
   int32_t level=0;std::memcpy(&level,s->payload,4);
   if(level==80)seen.data=true;
 }else if(s->event_kind==UMAA_DDS_EVENT_DISPOSE)seen.disposed=true;
}
int main(int argc,char** argv) {
 if(argc<4)return 2;
 const std::string mode=argv[1];
 try {
  umaa::DdsBackendPlugin plugin(argv[2]);
  if(!plugin.has_qos_extension())return 3;
  auto domain=plugin.open_domain(91);
  const UmaaDdsTopicV1 topic{"demo/BatteryStatus","demo:battery-v1","demo-reliable",
    UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
  umaa::qos::Policy policy;
  policy.partition="umaa-v2-fixture";
  policy.history=umaa::qos::History::keep_last;
  policy.depth=8;
  policy.durability=umaa::qos::Durability::volatile_data;
  if(argc==5 && std::string(argv[4])=="transient")
    policy.durability=umaa::qos::Durability::transient_local;
  if(argc==5 && std::string(argv[4])=="ordered"){
    policy.presentation=umaa::qos::Presentation::topic;
    policy.ordered_access=true;
  }
  umaa::qos::Requirements requirements;
  requirements.reliability=umaa::qos::Reliability::reliable;
  requirements.minimum_history_depth=4;
  if(policy.durability==umaa::qos::Durability::transient_local)
    requirements.durability=umaa::qos::Durability::transient_local;
  if(policy.ordered_access){
    requirements.presentation=umaa::qos::Presentation::topic;
    requirements.require_ordered_access=true;
  }
  if(argc==5 && std::string(argv[4])=="runtime"){
   const auto config=config_file(mode,argv[2],argv[3]);
   auto app=umaa::Application::from_config(config);
   std::filesystem::remove(config);
   if(mode=="subscriber"){
    auto remote=app.consume<BatteryStatus>();
    bool data=false,disposed=false;
    for(int i=0;i<350 && !disposed;i++){
     if(remote.poll(8)<0)break;
     BatteryStatus::Report report{};
     std::string key;
     if(remote.latest(report,key) && key=="17" && report.level_pct==80)data=true;
     if(remote.disposed())disposed=true;
     std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    if(!data||!disposed)return 14;
    std::cout<<"PASS SDK runtime QoS YAML -> V2 reader + data/dispose\n";
    return 0;
   }
   if(mode=="publisher"){
    auto pub=app.publish<BatteryStatus>("17");
    pub.update();
    std::this_thread::sleep_for(std::chrono::milliseconds(220));
    pub.dispose();
    pub.close();
    std::cout<<"PASS SDK runtime QoS YAML -> V2 writer + data/dispose\n";
    return 0;
   }
   return 15;
  }
  if(mode=="subscriber"){
    Seen seen;
    auto* reader=domain->reader(topic,policy,on_event,&seen,requirements);
    for(int i=0;i<350&&!seen.disposed;i++){
      if(domain->poll(reader,8)<0)break;
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    domain->close_reader(reader);
    if(!seen.data||!seen.disposed)return 4;
    std::cout<<"PASS explicit QoS subscriber + data/dispose\n";return 0;
  }
  if(mode=="publisher"){
    umaa::qos::Policy invalid=policy;
    invalid.ordered_access=true;invalid.presentation=umaa::qos::Presentation::topic;
    if(plugin.name().find("fastdds")!=std::string::npos){
      bool rejected=false;
      try{auto* w=domain->writer(topic,invalid,requirements);domain->close_writer(w);}
      catch(const std::runtime_error&){rejected=true;}
      if(!rejected)return 8; // Fast DDS does not guarantee ordered presentation
    }
    invalid=policy;invalid.reliability=umaa::qos::Reliability::best_effort;
    bool refused=false;
    try{auto* w=domain->writer(topic,invalid,requirements);domain->close_writer(w);}
    catch(const std::exception&){refused=true;}
    if(!refused)return 9;
    auto* writer=domain->writer(topic,policy,requirements);
    int32_t value=80;
    UmaaDdsSampleV1 sample{
      reinterpret_cast<const uint8_t*>(&value),4,"17",1000,UMAA_DDS_EVENT_DATA};
    const int32_t written=domain->write(writer,sample);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const int32_t disposed=domain->dispose(writer,sample);
    const int32_t ack=domain->wait_for_acks(writer,2500);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    domain->close_writer(writer);
    if(written||disposed||ack){
      std::cerr<<"Publisher failure "<<written<<"/"<<disposed<<"/"<<ack<<"\n";
      return 5;
    }
    std::cout<<"PASS explicit QoS publisher + data/dispose\n";return 0;
  }
  return 6;
 } catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 7;}
}
