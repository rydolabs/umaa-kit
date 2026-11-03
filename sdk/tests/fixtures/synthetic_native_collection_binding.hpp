// Synthetic multi-topic fixture for actual Cyclone/Fast DDS integration.
// Key aliasing uses the low octet ONLY because DemoCollectionBatch IDL key
// is a 32-bit integer. Never use this mapping for official UMAA GUID keys.
#pragma once
#include "umaa/native_collection_endpoint.hpp"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

struct NativeContact {
  std::int32_t value{};
  umaa::sdk_v1::SpecializationRef ref{};
};
struct NativeSpecialized {
  std::int32_t code{};
};
struct SyntheticNativeSet {
  using Value=NativeContact;
  using General=NativeContact;
  using Specialized=NativeSpecialized;
  static constexpr umaa::CollectionKind kind=umaa::CollectionKind::Set;
  static constexpr const char* service_id="Demo::EO::NativeContacts";
  static constexpr const char* collection_id="demo-native-contacts";
  static constexpr const char* element_topic="demo/NativeElement";
  static constexpr const char* element_schema="demo:native-element-v1";
  static constexpr const char* metadata_topic="demo/NativeMetadata";
  static constexpr const char* metadata_schema="demo:native-metadata-v1";
  static constexpr const char* general_topic="demo/NativeGeneral";
  static constexpr const char* general_schema="demo:native-general-v1";
  static constexpr const char* specialized_topic="demo/NativeSpecial";
  static constexpr const char* specialized_schema="demo:native-special-v1";
  static constexpr const char* qos_profile="demo-reliable";
  static std::string instance_key_for(umaa::sdk_v1::Guid128 id) {
    for(std::size_t i=0;i<15;++i)
      if(id.bytes[i])throw std::invalid_argument("synthetic UUID key must be zero prefixed");
    if(id.bytes[15]==0)throw std::invalid_argument("synthetic nil identity");
    return std::to_string(id.bytes[15]);
  }
  static umaa::sdk_v1::SpecializationRef reference(const Value& v) {return v.ref;}
  static std::vector<std::uint8_t> encode_value(const Value& v){
    namespace w=umaa::sdk_v1::native_wire;
    std::vector<std::uint8_t> out;
    w::u32(out,static_cast<std::uint32_t>(v.value));
    for(const auto x:v.ref.id.bytes)w::u8(out,x);
    w::i64(out,v.ref.timestamp_ns);
    return out;
  }
  static bool decode_value(const std::uint8_t* p,std::size_t n,Value& v) {
    if(!p||n!=28)return false;
    std::uint32_t x=0;
    for(unsigned i=0;i<4;++i)x|=std::uint32_t(p[i])<<(8*i);
    std::memcpy(&v.value,&x,4);
    for(std::size_t i=0;i<16;++i)v.ref.id.bytes[i]=p[4+i];
    std::uint64_t t=0;
    for(unsigned i=0;i<8;++i)t|=std::uint64_t(p[20+i])<<(8*i);
    std::memcpy(&v.ref.timestamp_ns,&t,8);
    return !v.ref.id.is_nil()&&v.ref.timestamp_ns>0;
  }
  static std::vector<std::uint8_t> encode_general(const General& v){
    return encode_value(v);
  }
  static bool decode_general(const std::uint8_t* p,std::size_t n,General& v){
    return decode_value(p,n,v);
  }
  static std::vector<std::uint8_t> encode_specialized(const Specialized& v){
    std::vector<std::uint8_t> out;
    umaa::sdk_v1::native_wire::u32(out,static_cast<std::uint32_t>(v.code));
    return out;
  }
  static bool decode_specialized(const std::uint8_t* p,std::size_t n,Specialized& v){
    if(!p||n!=4)return false;
    std::uint32_t x=0;
    for(unsigned i=0;i<4;++i)x|=std::uint32_t(p[i])<<(8*i);
    std::memcpy(&v.code,&x,4);
    return true;
  }
};
struct SyntheticNativeList: SyntheticNativeSet {
  static constexpr umaa::CollectionKind kind=umaa::CollectionKind::List;
};
