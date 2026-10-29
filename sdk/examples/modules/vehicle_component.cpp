// Business code has zero DDS headers and zero UMAA protocol state transitions.
#include "umaa/component.hpp"
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>
namespace demo {
struct EngineControl {
  static constexpr const char* id="Demo::EO::EngineControl";
  static constexpr const char* schema_id="demo:engine-v1";
  struct Command { int32_t target_rpm{}; };
  static bool decode_command(const uint8_t* p, size_t n, Command& c) {
    if(n!=sizeof(c.target_rpm)||!p)return false;
    std::memcpy(&c.target_rpm,p,sizeof(c.target_rpm));return true;
  }
};
struct AsyncEngineControl {
  static constexpr const char* id="Demo::EO::AsyncEngineControl";
  static constexpr const char* schema_id="demo:async-engine-v1";
  using Command=EngineControl::Command;
  static bool decode_command(const uint8_t* p,size_t n,Command& c) {
    return EngineControl::decode_command(p,n,c);
  }
};
struct BatteryStatus {
  static constexpr const char* id="Demo::EO::BatteryStatus";
  static constexpr const char* schema_id="demo:battery-v1";
  struct Report { int32_t level_pct{}; };
  static std::vector<uint8_t> encode_report(const Report& r) {
    std::vector<uint8_t> bytes(sizeof(r.level_pct));
    std::memcpy(bytes.data(),&r.level_pct,sizeof(r.level_pct));return bytes;
  }
};
class Engine final:public umaa::CommandHandler<EngineControl> {
public:
  umaa::Result execute(const EngineControl::Command& cmd) override {
    if(cmd.target_rpm<0||cmd.target_rpm>10000)return umaa::Result::Rejected;
    last_rpm_=cmd.target_rpm;return umaa::Result::Completed;
  }
  umaa::Result cancel() override {last_rpm_=0;return umaa::Result::Completed;}
private: int32_t last_rpm_{};
};
class AsyncEngine final:public umaa::CommandHandler<AsyncEngineControl> {
public:
  umaa::Result execute(const AsyncEngineControl::Command&) override {return umaa::Result::Pending;}
  umaa::Result execute_session(const AsyncEngineControl::Command& c,uint64_t id) override {
    if(c.target_rpm<0)return umaa::Result::Rejected;
    sessions_[id]=0;return umaa::Result::Pending;
  }
  umaa::Result poll_session(uint64_t id) override {
    auto it=sessions_.find(id);
    if(it==sessions_.end())return umaa::Result::Failed;
    if(++it->second<3)return umaa::Result::Pending;
    sessions_.erase(it);return umaa::Result::Completed;
  }
  umaa::Result cancel_session(uint64_t id) override {
    // Completed means hardware stop was confirmed; Failed never proves that.
    return sessions_.erase(id)?umaa::Result::Completed:umaa::Result::Rejected;
  }
private:
  std::unordered_map<uint64_t,int> sessions_;
};
class Battery final:public umaa::ReportSource<BatteryStatus> {
public: BatteryStatus::Report read() override {return {80};}
};
void register_services(umaa::Component& component) {
  static Engine engine;
  static Battery battery;
  static AsyncEngine async_engine;
  component.provide<EngineControl>(engine);
  component.provide<BatteryStatus>(battery);
  component.provide<AsyncEngineControl>(async_engine);
}
}
UMAA_EXPORT_COMPONENT_V2("vehicle-component", demo::register_services)
