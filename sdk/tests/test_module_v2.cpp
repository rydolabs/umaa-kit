#include "umaa/component_module.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
int main(int argc,char** argv){
 if(argc!=2)return 2;
 umaa::ComponentModule m(argv[1]);
 assert(m.name()=="vehicle-component");
 assert(m.services().size()==2);
 assert(m.services()[0].schema=="demo:engine-v1");
 assert(m.services()[1].schema=="demo:battery-v1");
 int32_t rpm=1500, business=-1;
 std::vector<uint8_t> input(sizeof(rpm)), out;
 std::memcpy(input.data(),&rpm,sizeof(rpm));
 assert(m.call("Demo::EO::EngineControl",UMAA_COMPONENT_COMMAND,input,out,business)==0);
 assert(business==0 && out.empty());
 rpm=-1;input.resize(sizeof(rpm));std::memcpy(input.data(),&rpm,sizeof(rpm));
 assert(m.call("Demo::EO::EngineControl",UMAA_COMPONENT_COMMAND,input,out,business)==0);
 assert(business==1);
 input.clear();
 assert(m.call("Demo::EO::EngineControl",UMAA_COMPONENT_CANCEL,input,out,business)==0);
 assert(business==0);
 assert(m.call("Demo::EO::BatteryStatus",UMAA_COMPONENT_REPORT,input,out,business)==0);
 assert(out.size()==sizeof(int32_t));
 int32_t battery=0;std::memcpy(&battery,out.data(),sizeof(battery));assert(battery==80);
 assert(m.call("Demo::EO::BatteryStatus",UMAA_COMPONENT_COMMAND,input,out,business)==UMAA_COMPONENT_NOT_FOUND);
 assert(m.stop());
 std::cout<<"PASS component ABI v2 multi-service, typed handler, report, cancel, unload\n";
}
