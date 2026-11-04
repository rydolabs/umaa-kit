// Synthetic int32 Generalization / Specialization data over independent
// physical DDS Topics. GUID128 + timestamp live inside application envelope.
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
struct SyntheticSpecialization {
  using General=std::int32_t;
  using Specialized=std::int32_t;
  static constexpr const char* general_topic="demo/SpecializationGeneral";
  static constexpr const char* specialized_topic="demo/SpecializationDetail";
  static constexpr const char* schema_id="demo:collection-batch-v1";
  static constexpr const char* qos_profile="demo-reliable";
  static std::vector<std::uint8_t> encode_general(General v){return pack(v);}
  static std::vector<std::uint8_t> encode_specialized(Specialized v){return pack(v);}
  static bool decode_general(const std::uint8_t* p,std::size_t n,General& value){
    return unpack(p,n,value);
  }
  static bool decode_specialized(const std::uint8_t* p,std::size_t n,Specialized& value){
    return unpack(p,n,value);
  }
private:
  static std::vector<std::uint8_t> pack(std::int32_t v){
    std::vector<std::uint8_t> out(sizeof v);
    std::memcpy(out.data(),&v,sizeof v);return out;
  }
  static bool unpack(const std::uint8_t* data,std::size_t n,std::int32_t& v){
    if(!data||n!=sizeof v)return false;
    std::memcpy(&v,data,n);return true;
  }
};
