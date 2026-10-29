// A separate process tests vendor plugin without linking a DDS SDK.
#include "umaa/dds_backend_loader.hpp"
#include "umaa/application.hpp"
#include "umaa/component_module.hpp"
#include "umaa/module_api_v2.h"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
struct BatteryStatus {
  static constexpr bool is_command=false;
  static constexpr const char* id="Demo::EO::BatteryStatus";
  static constexpr const char* schema_id="demo:battery-v1";
  static constexpr const char* topic_name="demo/BatteryStatus";
  static constexpr const char* qos_profile="demo-reliable";
  struct Report {int32_t level_pct{};};
  static bool decode_report(const uint8_t* p,size_t n,Report& r) {
    if(!p||n!=sizeof(int32_t))return false;
    std::memcpy(&r.level_pct,p,n);return true;
  }
};
int main(int argc,char** argv) {
 if(argc!=4)return 2;
 try {
   const std::string mode=argv[1];
   if(mode=="subscriber"){
     umaa::Application app;
     app.load_component(argv[3]);
     app.use_backend(argv[2],44);
     auto battery=app.consume<BatteryStatus>();
     bool seen_data=false,seen_dispose=false;
     for(int i=0;i<260&&!seen_dispose;++i){
       if(battery.poll(1)<0)break;
       BatteryStatus::Report report{};
       std::string key;
       if(battery.latest(report,key) && key=="17" && report.level_pct==80)
         seen_data=true;
       if(battery.disposed())seen_dispose=true;
       std::this_thread::sleep_for(std::chrono::milliseconds(25));
     }
     if(!seen_data||!seen_dispose){
       std::cerr<<"Subscriber incomplete: seen_data="<<seen_data
                <<" seen_dispose="<<seen_dispose<<" (vendor interoperability unverified)\n";
       return 3;
     }
     std::cout<<"PASS typed consume<BatteryStatus>() over real DDS vendor + Dispose\n";
     return 0;
   }
   const UmaaDdsTopicV1 topic{"demo/BatteryStatus","demo:battery-v1",
     "demo-reliable",UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
   if(mode=="publisher"){
     umaa::DdsBackendPlugin plugin(argv[2]);
     auto domain=plugin.open_domain(44);
     umaa::ComponentModule device(argv[3]);
     std::vector<uint8_t> request,reply;
     int32_t result=-1;
     if(device.call("Demo::EO::BatteryStatus",UMAA_COMPONENT_REPORT,
                    request,reply,result)!=UMAA_COMPONENT_OK ||
        result!=0 || reply.size()!=sizeof(int32_t))return 4;
     // Real vendor must refuse official/unreviewed types and unsupported QoS.
     UmaaDdsTopicV1 unapproved=topic;
     unapproved.schema_id="official:unreviewed";
     bool rejected=false;
     try {auto* unexpected=domain->writer(unapproved);domain->close_writer(unexpected);}
     catch(const std::runtime_error&){rejected=true;}
     if(!rejected)return 8;
     UmaaDdsTopicV1 unsupported=topic;
     unsupported.required_features=UMAA_DDS_CAP_COHERENT_GROUP;
     rejected=false;
     try {auto* unexpected=domain->writer(unsupported);domain->close_writer(unexpected);}
     catch(const std::runtime_error&){rejected=true;}
     if(!rejected)return 9;
     auto* writer=domain->writer(topic);
     UmaaDdsSampleV1 sample{reply.data(),static_cast<uint32_t>(reply.size()),
                            "17",1,UMAA_DDS_EVENT_DATA};
     auto bad_key=sample;bad_key.instance_key="not-a-dds-key";
     if(domain->write(writer,bad_key)!=UMAA_DDS_INVALID)return 10;
     int32_t sent=domain->write(writer,sample);
     // Test-only pacing is isolated to the synthetic real-wire fixture.
     // Do not retry commands after an unknown delivery outcome.
     int32_t first_ack=domain->wait_for_acks(writer,2000);
     std::this_thread::sleep_for(std::chrono::milliseconds(200));
     int32_t disposed=domain->dispose(writer,sample);
     int32_t last_ack=domain->wait_for_acks(writer,2000);
     std::this_thread::sleep_for(std::chrono::milliseconds(200));
     domain->close_writer(writer);
     std::cerr << "writer result data=" << sent << " first_ack=" << first_ack
               << " dispose=" << disposed << " last_ack=" << last_ack << "\n";
     // This Report writer is not required to ACK each intermediate update
     // before sending Dispose. The final reliable ACK checks all outstanding
     // samples in order; an earlier query remains diagnostic only.
     if(sent||disposed||last_ack)return 5;
     std::cout<<"PASS separate device component -> DDS vendor plugin\n";return 0;
   }
   return 6;
 }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 7;}
}
