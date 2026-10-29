#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/application.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
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
  struct Report{int32_t level_pct{};};
  static bool decode_report(const uint8_t* p,size_t n,Report& out){
    if(!p||n!=sizeof(int32_t))return false;
    std::memcpy(&out.level_pct,p,n);return true;
  }
};
int main(int argc,char** argv){
  if(argc!=3)return 2;
  auto host=std::make_shared<umaa::internal::ComponentRuntime>();
  host->load_component(argv[1]);host->load_backend(argv[2],75);
  umaa::ReportConsumer<BatteryStatus> consumer(host);
  std::atomic<int> data{0},disposes{0};
  consumer.on_change(
    [&](const BatteryStatus::Report& r,const std::string& key){
      if(r.level_pct==80&&key=="17")data.fetch_add(1);
    },
    [&](const std::string& key){if(key=="17")disposes.fetch_add(1);},
    std::chrono::milliseconds(5));
  bool excluded=false;try{(void)consumer.poll();}catch(const std::logic_error&){excluded=true;}
  assert(excluded);
  auto publisher=host->open_report_fixture(
    "Demo::EO::BatteryStatus","demo/BatteryStatus","demo:battery-v1","17");
  publisher->update();publisher->update();publisher->dispose();publisher->close();
  const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while((data.load()!=2||disposes.load()!=1)&&std::chrono::steady_clock::now()<limit)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  consumer.stop_watch();
  consumer.rethrow_watch_error();
  assert(data.load()==2&&disposes.load()==1);
  std::cout<<"PASS automatic Report callbacks, Dispose and exclusive Reader polling\n";
}
