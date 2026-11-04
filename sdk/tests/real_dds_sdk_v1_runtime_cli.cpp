// Cross-process test of SDK v1 traits -> Application -> ComponentRuntime ->
// real DDS plugin. Two genuine DDS plugins are used: synthetic Command and
// synthetic Report TypeSupport. Official UMAA multi-key wire use is NOT tested.
#include "umaa/sdk_v1_application.hpp"
#include "fixtures/synthetic_async_engine_binding.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace std::chrono_literals;
struct BatteryWire {
  static constexpr bool is_command=false;
  static constexpr const char* id="Demo::EO::BatteryStatus";
  static constexpr const char* schema_id="demo:battery-v1";
  static constexpr const char* topic_name="demo/BatteryStatus";
  static constexpr const char* qos_profile="demo-reliable";
  struct Report {std::int32_t level_pct{};};
  static bool decode_report(const std::uint8_t* data,std::size_t n,Report& out){
    if(!data||n!=sizeof(out.level_pct))return false;
    std::memcpy(&out.level_pct,data,n);return true;
  }
};
struct EngineControlV1 {};
struct BatteryStatusV1 {};
} // namespace

namespace umaa::sdk_v1 {
template<> struct ServiceTraits<EngineControlV1> {
  using WireService=AsyncEngine;
  using Command=AsyncEngine::Command;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::EO::AsyncEngineControl","demo:async-command-v1",
    FlowKind::CommandResponse,ServiceKind::Control};
};
template<> struct ServiceTraits<BatteryStatusV1> {
  using WireService=BatteryWire;
  using Report=BatteryWire::Report;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::EO::BatteryStatus","demo:battery-v1",
    FlowKind::QuerylessRequestReply,ServiceKind::Status};
};
}

int main(int argc,char** argv) {
  if(argc!=6)return 2;
  const std::string mode=argv[1],scenario=argv[5];
  if((mode!="server"&&mode!="client")||
     (scenario!="normal"&&scenario!="cancel"&&scenario!="update"))
    return 2;
  try {
    umaa::sdk_v1::RuntimeApplication command_app,report_app;
    command_app.use_backend(argv[2],44);
    report_app.use_backend(argv[3],44);
    if(mode=="server"){
      // The *same* device DLL/SO handles both Command and Report business
      // callbacks. Neither that module nor this CLI imports DDS vendor APIs.
      command_app.load_component(argv[4]);
      report_app.load_component(argv[4]);
      // Normal runs the SDK-owned background scheduler; cancel/update
      // control device polling to prevent an artificial early completion.
      auto command=scenario=="normal"
          ?command_app.provide_command<EngineControlV1>(
              "controller","vehicle",180ms,15ms)
          :command_app.serve_command<EngineControlV1>(
              "controller","vehicle",180ms);
      auto battery=report_app.publish_report<BatteryStatusV1>("17");
      bool published=false;
      const auto end=std::chrono::steady_clock::now()+16s;
      while(std::chrono::steady_clock::now()<end &&
            !command->faulted()&&command->retired_sessions()==0){
        if(scenario!="normal"){
          const bool poll_device=scenario=="update"?
            command->accepted_requests()>=2:false;
          command->poll(20,poll_device);
        }
        if(command->accepted_requests()>=1 && !published){
          battery.update();
          std::this_thread::sleep_for(350ms); // synthetic pacing for observation
          battery.dispose();
          battery.close();
          published=true;
        }
        std::this_thread::sleep_for(15ms);
      }
      command->stop_background();
      command->rethrow_background_error();
      const bool ok=published&&!command->faulted()&&
        command->accepted_requests()==(scenario=="update"?2u:1u)&&
        command->terminal_commands()==1&&command->retired_sessions()==1&&
        command->canceled_commands()==(scenario=="cancel"?1u:0u)&&
        command->active_sessions()==0;
      if(ok)command->wait_for_acks(5000);
      std::cout<<"SDK_V1_SERVER scenario="<<scenario
               <<" accepted="<<command->accepted_requests()
               <<" retired="<<command->retired_sessions()
               <<" canceled="<<command->canceled_commands()
               <<" report_published="<<published<<"\n";
      return ok?0:3;
    }

    auto battery=report_app.consume_report<BatteryStatusV1>();
    if(battery.poll(1)<0)throw std::runtime_error("Report Reader init failed");
    auto command=command_app.issue_command<EngineControlV1>(
      AsyncEngine::Command{1800},"17","controller","vehicle");
    bool saw_report=false,saw_dispose=false,sent_cancel=false,sent_update=false;
    const auto expected=scenario=="cancel"?umaa::RemoteCommandResult::Canceled:
                                       umaa::RemoteCommandResult::Completed;
    const auto end=std::chrono::steady_clock::now()+16s;
    while(std::chrono::steady_clock::now()<end){
      command.poll();
      if(battery.poll(1)<0)throw std::runtime_error("Report Reader fault");
      BatteryWire::Report report{};
      std::string key;
      if(battery.latest(report,key)&&key=="17"&&report.level_pct==80)
        saw_report=true;
      if(battery.disposed_for("17"))saw_dispose=true;
      if(scenario=="update"&&!sent_update&&command.ready_for_update()){
        command.update(AsyncEngine::Command{1900});
        sent_update=true;
      }
      if(scenario=="cancel"&&!sent_cancel&&command.acknowledged()){
        command.request_cancel(5s);
        sent_cancel=true;
      }
      if(command.state()==expected && saw_report && saw_dispose &&
         (scenario!="update"||sent_update) &&
         (scenario!="cancel"||sent_cancel))
        break;
      std::this_thread::sleep_for(15ms);
    }
    const bool ok=command.state()==expected&&saw_report&&saw_dispose&&
      (scenario!="update"||sent_update)&&(scenario!="cancel"||sent_cancel);
    if(ok)command.finish(10s);
    std::cout<<"SDK_V1_CLIENT scenario="<<scenario<<" terminal="<<ok
             <<" report_data="<<saw_report<<" report_dispose="<<saw_dispose
             <<" updated="<<sent_update<<" canceled="<<sent_cancel<<"\n";
    return ok?0:4;
  }catch(const std::exception& e){
    std::cerr<<"SDK v1 real DDS runtime failure: "<<e.what()<<"\n";
    return 5;
  }
}
