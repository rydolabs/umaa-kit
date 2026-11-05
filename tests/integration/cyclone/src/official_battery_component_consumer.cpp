// Real AUVSI original two-Topic source -> SDK Cyclone Reader ->
// ABI-v2 dynamically loaded business Component. No DDS calls in the DLL/SO.
#include "umaa/component_module.hpp"
#include "umaa/official_battery_component_bridge.hpp"
#include "umaa/cyclone_official_battery_reader.hpp"

#include <chrono>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>

namespace b=umaa::official_battery;
namespace c=umaa::cyclone::official_battery;

static b::Guid guid(std::uint8_t n){b::Guid g{};g[0]=n;return g;}

int main(int argc,char** argv){
  const bool bad=argc==3 && std::string(argv[2])=="--bad-checkpoint";
  if((argc!=2 && !bad)||!argv[1]||!*argv[1])return 2;
  try {
    umaa::ComponentModule module(argv[1]);
    b::BatteryStatusComponentBridge handler(module);
    if(handler.current_known(guid(0x11)) || handler.current_known(guid(0x12)))
      throw std::runtime_error("unknown Battery List incorrectly marked current");
    c::OriginalBatteryStatusReader reader(handler,DDS_DOMAIN_DEFAULT,2,16);
    const auto first_epoch=reader.epoch();
    for(int tick=0;tick<1400;++tick){
      const bool got=reader.poll_once();
      const auto st=reader.stats();
      if(st.report_samples>=2 && st.element_samples>=4 &&
         st.report_disposes>=2 && st.element_disposes>=4)
        break;
      if(!got)std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    const auto stats=reader.stats();
    if(stats.report_samples!=2||stats.element_samples!=4||
       stats.report_disposes!=2||stats.element_disposes!=4)
      throw std::runtime_error("incomplete original native keyed DDS evidence");
    if(handler.faulted()||handler.active_sources()!=0||
       handler.accepted()!=(bad?1u:2u)||
       handler.disposed()!=(bad?1u:2u))
      throw std::runtime_error("ABI-v2 business Component rejected original source proof");
    if(reader.active_lists()!=0 || reader.latest(guid(0x11)) ||
       reader.latest(guid(0x12)) ||
       handler.current_known(guid(0x11)) ||
       handler.current_known(guid(0x12)))
      throw std::runtime_error("original source Dispose retained stale Reader/ABI List");
    reader.restart();
    if(reader.epoch()==first_epoch || reader.active_lists()!=0)
      throw std::runtime_error("native Reader was not fenced after restart");
    reader.close();
    if(!module.stop())
      throw std::runtime_error("ABI-v2 Component refused safe unload");
    if(bad){
      std::puts("BATTERY_COMPONENT_ABI_BAD_CHECKPOINT_ISOLATED");
      std::puts("BATTERY_TWO_LIST_BAD_CHECKPOINT_REJECTED");
      return 10;
    }
    std::puts("BATTERY_COMPONENT_ABI_TWO_SOURCE_NATIVE_WIRE_PASS");
    std::puts("BATTERY_TWO_LIST_NATIVE_ROUTER_PASS");
    return 0;
  }catch(const std::exception& ex){
    std::fprintf(stderr,"original Battery ABI-v2 bridge failure: %s\n",ex.what());
    return 3;
  }
}
