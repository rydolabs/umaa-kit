#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/internal/component_runtime.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
int main(int argc,char** argv){
  if(argc!=3)return 2;
  umaa::internal::ComponentRuntime runtime;
  runtime.load_component(argv[1]);runtime.load_backend(argv[2],77);
  auto sub=runtime.subscribe_report_fixture(
     "Demo::EO::BatteryStatus","demo/BatteryStatus","demo:battery-v1","demo-reliable");
  auto publisher=runtime.open_report_fixture(
     "Demo::EO::BatteryStatus","demo/BatteryStatus","demo:battery-v1","17");
  publisher->start_periodic(std::chrono::milliseconds(10));
  int count=0;
  auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
  while(count<3&&std::chrono::steady_clock::now()<deadline){
    auto n=sub->poll(64);
    assert(n>=0);
    count+=n;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  assert(count>=3);
  publisher->stop_periodic();
  publisher->rethrow_periodic_error();
  assert(!publisher->faulted());
  publisher->dispose();
  publisher->close();
  assert(sub->poll(64)>=1);
  assert(sub->disposed_for("17"));
  bool refused=false;
  try{publisher->start_periodic(std::chrono::milliseconds(10));}
  catch(const std::logic_error&){refused=true;}
  assert(refused);
  std::cout<<"PASS automatic Report scheduled updates, Dispose and teardown\n";
}
