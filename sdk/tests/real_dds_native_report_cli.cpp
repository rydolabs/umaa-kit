// ONE vendor-agnostic SDK client for genuine idlc and fastddsgen TypeSupport.
// Linked against no DDS middleware. Synthetic native Report fixture only.
#include "umaa/application.hpp"
#include "umaa/dds_backend_loader.hpp"
#include "umaa/component_module.hpp"
#include "umaa/module_api_v2.h"
#include "generated/native_report_runtime.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using Service=umaa_native_runtime::Report;
using namespace std::chrono_literals;

int main(int argc,char** argv){
  if(argc!=4)return 2;
  try {
    const std::string mode=argv[1],backend=argv[2];
    constexpr std::uint32_t domain=44;
    if(mode=="subscriber"){
      umaa::Application app;
      app.use_backend(backend,domain);
      auto consumer=app.consume<Service>();
      bool data=false,disposed=false;
      for(int i=0;i<280&&!disposed;++i){
        if(consumer.poll(8)<0)throw std::runtime_error("DDS poll failure");
        Service::Report report{};
        std::string key;
        if(consumer.latest(report,key)&&key=="17"&&report.level_pct==80)
          data=true;
        if(consumer.disposed())disposed=true;
        std::this_thread::sleep_for(25ms);
      }
      if(!data||!disposed){
        std::cerr<<"Native Report incomplete data="<<data
                 <<" disposed="<<disposed<<"\n";
        return 3;
      }
      std::cout<<"NATIVE_REPORT_SUB value=80 key=17 dispose=1\n";
      return 0;
    }
    if(mode!="publisher")return 2;
    umaa::DdsBackendPlugin plugin(backend);
    auto participant=plugin.open_domain(domain);
    umaa::ComponentModule device(argv[3]);
    std::vector<std::uint8_t> request,response;
    std::int32_t result=-1;
    if(device.call(Service::id,UMAA_COMPONENT_REPORT,request,response,result)!=
        UMAA_COMPONENT_OK || result || response.size()!=sizeof(std::int32_t))
      throw std::runtime_error("business Handler returned invalid Report");
    UmaaDdsTopicV1 topic{Service::topic_name,Service::schema_id,
                        Service::qos_profile,
                        UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
    auto bogus=topic;bogus.schema_id="not-reviewed";
    bool rejected=false;
    try{auto* bad=participant->writer(bogus);participant->close_writer(bad);}
    catch(const std::runtime_error&){rejected=true;}
    if(!rejected)throw std::runtime_error("unreviewed TypeBinding was accepted");
    auto* writer=participant->writer(topic);
    const UmaaDdsSampleV1 sample{
      response.data(),static_cast<std::uint32_t>(response.size()),
      "17",1,UMAA_DDS_EVENT_DATA};
    auto wrong=sample;wrong.instance_key="not-a-32bit-key";
    if(participant->write(writer,wrong)!=UMAA_DDS_INVALID)
      throw std::runtime_error("wrong Key accepted");
    const int wrote=participant->write(writer,sample);
    std::this_thread::sleep_for(250ms);
    const int disposed=participant->dispose(writer,sample);
    const int acknowledged=participant->wait_for_acks(writer,4500);
    std::this_thread::sleep_for(150ms);
    participant->close_writer(writer);
    if(wrote||disposed||acknowledged)
      throw std::runtime_error("native DDS write/dispose ACK unconfirmed");
    std::cout<<"NATIVE_REPORT_PUB value=80 key=17 dispose=1\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"native Report integration: "<<e.what()<<"\n";
    return 7;
  }
}
