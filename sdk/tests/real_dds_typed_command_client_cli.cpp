// Minimal typed remote Command client, no DDS Reader/Writer or vendor headers.
// Uses synthetic Command IDL for independent-process acceptance only.
#include "umaa/application.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
struct AsyncEngine {
  static constexpr bool is_command=true;
  static constexpr const char* id="Demo::EO::AsyncEngineControl";
  static constexpr const char* command_topic="demo/AsyncEngineCommand";
  static constexpr const char* command_schema="demo:async-command-v1";
  static constexpr const char* status_topic="demo/AsyncEngineStatus";
  static constexpr const char* status_schema="demo:async-status-v1";
  static constexpr const char* ack_topic="demo/AsyncEngineAck";
  static constexpr const char* ack_schema="demo:async-ack-v1";
  static constexpr const char* qos_profile="demo-reliable";
  struct Command{std::int32_t rpm{};};
  static std::vector<std::uint8_t> encode_command(const Command& c) {
    std::vector<std::uint8_t> bytes(sizeof(c.rpm));
    std::memcpy(bytes.data(),&c.rpm,bytes.size());return bytes;
  }
  static std::optional<umaa::State> decode_status(const std::uint8_t* p,std::size_t n) {
    if(!p||n!=sizeof(std::int32_t))return std::nullopt;
    std::int32_t code=0;std::memcpy(&code,p,sizeof(code));
    if(code<0||code>5)return std::nullopt;
    const umaa::State states[]={umaa::State::ISSUED,umaa::State::COMMANDED,
      umaa::State::EXECUTING,umaa::State::COMPLETED,umaa::State::FAILED,
      umaa::State::CANCELED};
    return states[code];
  }
  static bool ack_matches(const std::uint8_t* p,std::size_t n,const Command& c){
    if(!p||n!=sizeof(std::int32_t))return false;
    std::int32_t value=0;std::memcpy(&value,p,sizeof(value));
    return value==c.rpm;
  }
};
int main(int argc,char** argv){
  if(argc!=4 || (std::string(argv[1])!="client" &&
                 std::string(argv[1])!="client-cancel" &&
                 std::string(argv[1])!="client-update"))return 2;
  try{
    umaa::Application app;
    app.use_backend(argv[2],44);
    auto cmd=app.issue_remote<AsyncEngine>({1800},"17","controller","vehicle");
    const bool cancel_mode=std::string(argv[1])=="client-cancel";
    const bool update_mode=std::string(argv[1])=="client-update";
    if(update_mode) {
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
      while(!cmd.ready_for_update()&&std::chrono::steady_clock::now()<deadline){
        cmd.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      if(!cmd.ready_for_update())return 6;
      cmd.update({1900});
    }
    if(cancel_mode) {
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
      while(!cmd.acknowledged() && std::chrono::steady_clock::now()<deadline) {
        cmd.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      if(!cmd.acknowledged() ||
         cmd.state()!=umaa::RemoteCommandResult::Pending) return 5;
      cmd.request_cancel(std::chrono::seconds(5));
    }
    const auto outcome=cmd.wait_for(std::chrono::seconds(10));
    const auto expected=cancel_mode?umaa::RemoteCommandResult::Canceled:
                                    umaa::RemoteCommandResult::Completed;
    if(outcome!=expected){
      std::cerr<<"Remote Command did not reach expected terminal Status\n";return 3;
    }
    if(!cancel_mode)std::this_thread::sleep_for(std::chrono::milliseconds(150));
    cmd.finish(std::chrono::seconds(10));
    std::cout<<"PASS typed remote Command "
             <<(cancel_mode?"cancellation":update_mode?"revision update":"completion")
             <<" after real Status/Ack and Dispose\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"Typed Command failure: "<<e.what()<<"\n";return 4;
  }
}
