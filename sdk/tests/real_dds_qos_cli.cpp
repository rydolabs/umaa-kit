// Same C++ QoS client for CycloneDDS and FastDDS via optional plugin ABI.
// Synthetic Battery only; intentionally no claim of official UMAA service QoS.
#include "umaa/dds_backend_loader.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
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
 if(argc<3)return 2;
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
  umaa::qos::Requirements requirements;
  requirements.reliability=umaa::qos::Reliability::reliable;
  requirements.minimum_history_depth=4;
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
