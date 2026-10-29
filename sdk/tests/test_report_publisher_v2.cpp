#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/internal/component_runtime.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
int main(int argc,char** argv) {
  if(argc!=3)return 2;
  umaa::internal::ComponentRuntime runtime;
  runtime.load_component(argv[1]);runtime.load_backend(argv[2],51);
  auto sub=runtime.subscribe_report_fixture("Demo::EO::BatteryStatus",
       "demo/BatteryStatus","demo:battery-v1","demo-reliable");
  auto a=runtime.open_report_fixture("Demo::EO::BatteryStatus",
       "demo/BatteryStatus","demo:battery-v1","17");
  auto b=runtime.open_report_fixture("Demo::EO::BatteryStatus",
       "demo/BatteryStatus","demo:battery-v1","18");
  a->update();a->update();b->update();
  assert(sub->poll(100)==3);
  assert(sub->keys().size()==2);
  std::vector<uint8_t> payload;
  assert(sub->latest_for("17",payload) && payload.size()==sizeof(int32_t));
  int32_t level=0;std::memcpy(&level,payload.data(),sizeof(level));assert(level==80);
  a->dispose();a->close();assert(sub->poll(100)==1);
  assert(sub->disposed_for("17"));
  assert(!sub->latest_for("17",payload));
  assert(sub->latest_for("18",payload));
  assert(sub->keys().size()==1);
  bool refused=false;try{a->update();}catch(const std::logic_error&){refused=true;}
  assert(refused);b->close();
  std::unique_ptr<umaa::internal::ReportPublisher> orphan;
  {
    auto tmp=std::make_unique<umaa::internal::ComponentRuntime>();
    tmp->load_component(argv[1]);tmp->load_backend(argv[2],52);
    orphan=tmp->open_report_fixture("Demo::EO::BatteryStatus",
       "demo/BatteryStatus","demo:battery-v1","19");
  }
  orphan->update();orphan->close();
  std::cout<<"PASS report persistent writer, independent keys, dispose, teardown ownership\\n";
}
