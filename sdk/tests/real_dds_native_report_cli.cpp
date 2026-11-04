// Generated native Report traits -> SDK v1 -> real vendor TypeSupport.
// Exactly the same executable runs with Cyclone DDS and Fast DDS plugins.
// No vendor DDS headers or manual C ABI calls in business-facing code.
#include "umaa/sdk_v1_application.hpp"
#include "generated/native_report_runtime.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

using Service=umaa_native_runtime::BatteryStatusTag;
using Traits=umaa::sdk_v1::ServiceTraits<Service>;
using namespace std::chrono_literals;

int main(int argc,char** argv){
  if(argc!=4)return 2;
  try{
    const std::string action=argv[1];
    umaa::sdk_v1::RuntimeApplication app;
    app.use_backend(argv[2],44);
    if(action=="subscriber"){
      auto consumer=app.consume_report<Service>();
      bool saw_report=false,saw_dispose=false;
      for(int i=0;i<260&&!saw_dispose;++i){
        if(consumer.poll(8)<0)
          throw std::runtime_error("native Report DDS poll failed");
        Traits::Report report{};
        std::string key;
        if(consumer.latest(report,key)&&key=="17"&&report.level_pct==80)
          saw_report=true;
        if(consumer.disposed())saw_dispose=true;
        std::this_thread::sleep_for(25ms);
      }
      if(!saw_report||!saw_dispose){
        std::cerr<<"native Report missing data/dispose report="
                 <<saw_report<<" dispose="<<saw_dispose<<"\n";
        return 3;
      }
      std::cout<<"NATIVE_SDK_V1_REPORT_SUB value=80 key=17 dispose=1\n";
      return 0;
    }
    if(action!="publisher")return 2;
    // Exactly the existing user Business Handler module. No native DDS
    // type or vendor code is included, instantiated or called here.
    app.load_component(argv[3]);
    auto publisher=app.publish_report<Service>("17");
    publisher.update();
    std::this_thread::sleep_for(220ms);
    publisher.dispose();
    publisher.close();
    if(publisher.faulted())
      throw std::runtime_error("native Report Publisher faulted");
    std::cout<<"NATIVE_SDK_V1_REPORT_PUB value=80 key=17 dispose=1\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"native SDK v1 Report: "<<e.what()<<"\n";
    return 7;
  }
}
