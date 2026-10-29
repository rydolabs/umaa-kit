#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/component_module.hpp"
#include "umaa/internal/native_library.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
int main(int argc,char** argv){
 if(argc!=2)return 2;
 umaa::internal::NativeLibrary probe(argv[1]);
 auto inflight=reinterpret_cast<int(*)()>(probe.symbol("umaa_test_inflight"));
 auto stop_count=reinterpret_cast<int(*)()>(probe.symbol("umaa_test_stop_count"));
 auto fail_stop=reinterpret_cast<void(*)(int)>(probe.symbol("umaa_test_fail_stop"));
 assert(inflight&&stop_count&&fail_stop);
 {
  umaa::ComponentModule module(argv[1]);
  std::thread worker([&]{
   std::vector<uint8_t> result;int32_t business=-1;
   assert(module.call("Demo::SlowControl",UMAA_COMPONENT_COMMAND,{},result,business,7)==0);
   assert(business==0);
  });
  // Start stop after the plugin callback actually entered, not just
  // after the worker thread was scheduled.
  auto until=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  while(inflight()==0&&std::chrono::steady_clock::now()<until)std::this_thread::yield();
  assert(inflight()==1);
  const auto t0=std::chrono::steady_clock::now();
  assert(module.stop());
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now()-t0).count();
  worker.join();
  assert(inflight()==0);
  assert(elapsed>=80);
 }
 const auto before=stop_count();
 {
  fail_stop(1);
  umaa::ComponentModule module(argv[1]);
  assert(!module.stop());
  assert(!module.stop()); // never retry an ambiguous shutdown
  assert(stop_count()==before+1);
  bool refused=false;
  try{std::vector<uint8_t> out;int32_t business=-1;
      module.call("Demo::SlowControl",UMAA_COMPONENT_COMMAND,{},out,business,9);}
  catch(const std::runtime_error&){refused=true;}
  assert(refused);
 }
 fail_stop(0);
 std::cout<<"PASS in-flight stop barrier and failed-stop one-shot quarantine\n";
}
