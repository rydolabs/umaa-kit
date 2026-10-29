#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/internal/component_runtime.hpp"
#include "umaa/module_api_v2.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
int main(int argc,char** argv){
 if(argc!=3)return 2;
 umaa::internal::ComponentRuntime host;
 host.load_component(argv[1]);host.load_backend(argv[2],78);
 assert(host.services().size()==3);
 int32_t rpm=1200,business=-1;
 std::vector<uint8_t> cmd(sizeof(rpm)),out;
 std::memcpy(cmd.data(),&rpm,sizeof(rpm));
 assert(host.call("Demo::EO::AsyncEngineControl",UMAA_COMPONENT_COMMAND,
    cmd,out,business,987)==UMAA_COMPONENT_OK);
 assert(business==2);
 assert(!host.unload_component("vehicle-component")); // pending
 assert(host.services().size()==3);
 for(int i=0;i<3;i++)
   assert(host.call("Demo::EO::AsyncEngineControl",UMAA_COMPONENT_COMMAND_POLL,
     {},out,business,987)==UMAA_COMPONENT_OK);
 assert(business==0);
 auto publisher=host.open_report_fixture(
   "Demo::EO::BatteryStatus","demo/BatteryStatus","demo:battery-v1","17");
 assert(!host.unload_component("vehicle-component")); // publisher owns module
 publisher->update();publisher->close();publisher.reset();
 assert(host.unload_component("vehicle-component"));
 assert(host.services().empty());
 assert(host.call("Demo::EO::EngineControl",UMAA_COMPONENT_COMMAND,
    cmd,out,business)==UMAA_COMPONENT_NOT_FOUND);
 host.load_component(argv[1]);
 assert(host.services().size()==3);
 assert(host.call("Demo::EO::EngineControl",UMAA_COMPONENT_COMMAND,
    cmd,out,business)==UMAA_COMPONENT_OK);
 assert(business==0);
 assert(host.unload_component("vehicle-component"));
 std::cout<<"PASS cooperative unload rejects Pending/publisher and supports reload\n";
}
