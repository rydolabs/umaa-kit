#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/application.hpp"
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
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
 umaa::Application app;
 app.load_component(argv[1]);app.use_backend(argv[2],79);
 {
   auto published=app.publish<BatteryStatus>("17");
   published.start_periodic(std::chrono::milliseconds(10));
   auto consumed=app.consume<BatteryStatus>();
   int received=0;
   auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
   while(received<3&&std::chrono::steady_clock::now()<deadline) {
     const auto n=consumed.poll(32);assert(n>=0);received+=n;
     std::this_thread::sleep_for(std::chrono::milliseconds(5));
   }
   assert(received>=3);
   assert(!app.unload_component("vehicle-component")); // writer still alive
   published.stop_periodic();
   published.rethrow_periodic_error();
   published.dispose();
   published.close();
   assert(consumed.poll(32)>=1);
   assert(consumed.disposed());
 }
 assert(app.unload_component("vehicle-component"));
 app.load_component(argv[1]);
 {
   auto publisher=app.publish<BatteryStatus>("18");
   publisher.update();
   publisher.close();
 }
 assert(app.unload_component("vehicle-component"));
 std::cout<<"PASS typed publish/consume without DDS headers and safe component reload\n";
}
