#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/application.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
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
 if(argc!=2)return 2;
 umaa::Application consumer;
 consumer.use_backend(argv[1],80); // NO component DLL loaded
 auto reports=consumer.consume<BatteryStatus>();
 assert(reports.poll(1)==0);
 assert(!reports.disposed());
 BatteryStatus::Report r{};
 std::string key;
 assert(!reports.latest(r,key));
 std::cout<<"PASS remote-only Report Consumer subscribes with no local Provider DLL\n";
}
