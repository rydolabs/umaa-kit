// A separate process tests vendor plugin without linking a DDS SDK.
#include "umaa/dds_backend_loader.hpp"
#include "umaa/component_module.hpp"
#include "umaa/module_api_v2.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
struct Received {bool data{};bool disposed{};int32_t level{};};
void receive(void* context,const UmaaDdsSampleV1* sample) {
 if(sample && sample->instance_key) std::cerr << "callback kind=" << sample->event_kind << " key=" << sample->instance_key << " size=" << sample->payload_size << "\n";
 if(!context||!sample||!sample->instance_key||
    std::string(sample->instance_key)!="17")return;
 auto& out=*static_cast<Received*>(context);
 if(sample->event_kind==UMAA_DDS_EVENT_DATA &&
    sample->payload && sample->payload_size==sizeof(int32_t)) {
    std::memcpy(&out.level,sample->payload,sizeof(int32_t));out.data=true;
 }else if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE)out.disposed=true;
}
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 try {
   umaa::DdsBackendPlugin plugin(argv[2]);
   auto domain=plugin.open_domain(44);
   const UmaaDdsTopicV1 topic{"demo/BatteryStatus","demo:battery-v1",
     "demo-reliable",UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
   const std::string mode=argv[1];
   if(mode=="subscriber"){
     Received observed;
     auto* reader=domain->reader(topic,receive,&observed);
     for(int i=0;i<260 && !(observed.data && observed.disposed);++i){
       if(domain->poll(reader,1)<0)break;
       std::this_thread::sleep_for(std::chrono::milliseconds(25));
     }
     domain->close_reader(reader);
     std::cerr << "final received data=" << observed.data << " disposed=" << observed.disposed << " level=" << observed.level << "\n";
     if(!observed.data || !observed.disposed || observed.level!=80)return 3;
     std::cout<<"PASS real Cyclone DDS data=80 + keyed Dispose\n";return 0;
   }
   if(mode=="publisher"){
     umaa::ComponentModule device(argv[3]);
     std::vector<uint8_t> request,reply;
     int32_t result=-1;
     if(device.call("Demo::EO::BatteryStatus",UMAA_COMPONENT_REPORT,
                    request,reply,result)!=UMAA_COMPONENT_OK ||
        result!=0 || reply.size()!=sizeof(int32_t))return 4;
     auto* writer=domain->writer(topic);
     UmaaDdsSampleV1 sample{reply.data(),static_cast<uint32_t>(reply.size()),
                            "17",1,UMAA_DDS_EVENT_DATA};
     int32_t sent=domain->write(writer,sample);
     int32_t disposed=domain->dispose(writer,sample);
     int32_t ack=domain->wait_for_acks(writer,2000);
     domain->close_writer(writer);
     if(sent||disposed||ack)return 5;
     std::cout<<"PASS separate device component -> DDS vendor plugin\n";return 0;
   }
   return 6;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 7;}
}
