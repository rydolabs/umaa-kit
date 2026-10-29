// Synthetic Command binding shared by typed client and typed host provider.
// The 4-byte codec is NOT official UMAA TypeSupport or reviewed QoS.
#pragma once
#include "umaa/command_engine.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
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
  static bool decode_command(const std::uint8_t* p,std::size_t n,Command& out){
    if(!p||n!=sizeof(std::int32_t))return false;
    std::memcpy(&out.rpm,p,sizeof(out.rpm));return true;
  }
  static std::vector<std::uint8_t> encode_status(const umaa::Identity&,
                                                 const umaa::Event& event) {
    const std::string names[]={"ISSUED","COMMANDED","EXECUTING","COMPLETED","FAILED","CANCELED"};
    std::int32_t code=-1;
    for(std::int32_t i=0;i<6;++i)if(names[i]==event.detail)code=i;
    if(code<0)throw std::invalid_argument("unrecognized Command Status");
    std::vector<std::uint8_t> out(4);std::memcpy(out.data(),&code,4);return out;
  }
  static std::vector<std::uint8_t> encode_ack(const umaa::Identity&,const Command& c){
    return encode_command(c);
  }
  static std::string instance_key(const umaa::Identity& id){return id.session;}
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
