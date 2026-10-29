#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/application.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
namespace demo {
struct EngineControl {
  static constexpr bool is_command=true;
  static constexpr const char* id="Demo::EO::EngineControl";
  struct Command {int32_t target_rpm=0;};
  static std::vector<uint8_t> encode_command(const Command& c){
    std::vector<uint8_t> p(sizeof(c.target_rpm));
    std::memcpy(p.data(),&c.target_rpm,p.size());return p;
  }
};
struct AsyncEngineControl : EngineControl {
  static constexpr const char* id="Demo::EO::AsyncEngineControl";
};
struct BatteryStatus {
  static constexpr bool is_command=false;
  static constexpr const char* id="Demo::EO::BatteryStatus";
  struct Report {int32_t level_pct=0;};
  static bool decode_report(const uint8_t* p,size_t n,Report& out){
    if(!p||n!=sizeof(int32_t))return false;
    std::memcpy(&out.level_pct,p,n);return true;
  }
};
}
int main(int argc,char** argv) {
  if(argc!=2)return 2;
  umaa::Application app;app.load_component(argv[1]);
  auto engine=app.consume<demo::EngineControl>();
  assert(engine.send({1500}).wait()==umaa::Result::Completed);
  assert(engine.send({-1}).wait()==umaa::Result::Rejected);
  auto async_client=app.consume<demo::AsyncEngineControl>();
  auto pending=async_client.send({1500});
  assert(pending.session_id()!=0 && pending.state()==umaa::Result::Pending);
  bool refused=false;
  try{(void)pending.wait();}catch(const std::logic_error&){refused=true;}
  assert(refused);
  assert(pending.poll()==umaa::Result::Pending);
  assert(pending.wait_for(std::chrono::milliseconds(200))==umaa::Result::Completed);
  assert(pending.wait()==umaa::Result::Completed);
  auto cancelled=async_client.send({1500});
  assert(cancelled.cancel()==umaa::Result::Failed);
  assert(cancelled.terminal());
  assert(async_client.send({-1}).state()==umaa::Result::Rejected);
  auto report_client=app.consume<demo::BatteryStatus>();
  assert(report_client.keys().empty());
  demo::BatteryStatus::Report absent{};
  assert(!report_client.latest_for("missing",absent));
  assert(!report_client.disposed_for("missing"));
  assert(report_client.read_local().level_pct==80);
  std::cout<<"PASS typed host-local consumer/command handle; no DDS wire claim\n";
}
