#ifdef NDEBUG
#undef NDEBUG  // Keep ABI checks active in Release-mode CI.
#endif
#include "umaa/internal/component_runtime.hpp"
#include "umaa/module_api_v2.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
int main(int argc,char** argv){
  if(argc!=3)return 2;
  umaa::internal::ComponentRuntime runtime;
  runtime.load_component(argv[1]);
  runtime.load_backend(argv[2],42);
  assert(runtime.services().size()==2);
  bool refused=false;
  try{runtime.load_component(argv[1]);}catch(const std::runtime_error&){refused=true;}
  assert(refused);
  int32_t rpm=1300,business=-1;
  std::vector<uint8_t> in(sizeof(rpm)),out;
  std::memcpy(in.data(),&rpm,sizeof(rpm));
  assert(runtime.call("Demo::EO::EngineControl",UMAA_COMPONENT_COMMAND,in,out,business)==0);
  assert(business==0);
  runtime.publish_report_fixture("Demo::EO::BatteryStatus", "demo/BatteryStatus",
                                 "demo:battery-v1", "vehicle-a");
  refused=false;
  try{runtime.publish_report_fixture("Demo::EO::BatteryStatus", "demo/BatteryStatus",
                                     "demo:incorrect-v1", "vehicle-a");}
  catch(const std::runtime_error&){refused=true;}
  assert(refused);
  std::cout<<"PASS shared UMAA runtime loads device+backend modules and checks schema\n";
}
