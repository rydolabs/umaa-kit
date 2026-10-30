#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/application.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
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
 if(argc!=3)return 2;
 const auto path=std::filesystem::current_path()/"umaa_runtime_test.yml";
 auto write=[&](const std::string& content){
   std::ofstream out(path,std::ios::trunc);
   assert(bool(out));out<<content;out.flush();assert(bool(out));
 };
 auto config=[&](bool provider){
   return std::string("middleware:\n  backend: \"")+argv[2]+
      "\"\n  domain_id: 81\nmodules:\n"+
      (provider?std::string("  - \"")+argv[1]+"\"\n":std::string{});
 };
 write(config(true));
 {
   auto app=umaa::Application::from_config(path.string());
   auto publisher=app.publish<BatteryStatus>("17");
   publisher.update();publisher.close();
   assert(!app.unload_component("nonexistent"));
   assert(app.unload_component("vehicle-component"));
 }
 write(config(false));
 {
   auto app=umaa::Application::from_config(path.string());
   auto report=app.consume<BatteryStatus>();
   assert(report.poll()==0); // valid remote-only client config
 }
 const std::vector<std::string> rejected={
   "middleware:\n  backend: fake\n  domain_id: 999\n",
   "middleware:\n  backend: fake\n  backend: fake\n  domain_id: 81\n",
   "middleware:\n  backend: fake\n  domain_id: abc\n",
   "middleware:\n  backend: fake\n  domain_id: 81\n  unsupported: yes\n",
   "middleware:\n  backend: fake\n  domain_id: 81\nmodules:\n    - bad\n",
   "modules:\n  - fake\n",
 };
 for(const auto& bad:rejected){
   write(bad);
   bool refused=false;
   try{(void)umaa::internal::RuntimeConfig::read(path.string());}
   catch(const std::exception&){refused=true;}
   assert(refused);
 }
 std::filesystem::remove(path);
 std::cout<<"PASS strict config + component/provider and remote-only consumer setup\n";
}
