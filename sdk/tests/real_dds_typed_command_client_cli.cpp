// Minimal typed remote Command client, no DDS Reader/Writer or vendor headers.
// Uses synthetic Command IDL for independent-process acceptance only.
#include "umaa/remote_command_client.hpp"
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
  if(argc!=4 || std::string(argv[1])!="client")return 2;
  try{
    umaa::RemoteCommandClient<AsyncEngine> client(argv[2],44,"controller","vehicle");
    client.send({1800},"17");
    const auto outcome=client.wait_for("17",std::chrono::seconds(10));
    if(outcome!=umaa::RemoteCommandResult::Completed){
      std::cerr<<"Remote Command not completed\n";return 3;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    client.finish("17",std::chrono::seconds(10));
    std::cout<<"PASS typed remote Command send/wait/finish over selected DDS backend\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"Typed Command failure: "<<e.what()<<"\n";return 4;
  }
}
