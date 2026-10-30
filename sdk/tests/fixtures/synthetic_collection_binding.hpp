// Synthetic int32 Collection frame codec; not official UMAA LargeSet.
#pragma once
#include "umaa/collection_transport.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
struct SyntheticCollection {
 using Value=std::int32_t;
 static constexpr umaa::CollectionKind kind=umaa::CollectionKind::Set;
 static constexpr const char* collection_id="demo-collection";
 static constexpr const char* topic_name="demo/CollectionBatch";
 static constexpr const char* schema_id="demo:collection-batch-v1";
 static constexpr const char* qos_profile="demo-reliable";
 static std::vector<std::uint8_t> encode_value(const Value& value){
   std::vector<std::uint8_t> out(sizeof(value));
   std::memcpy(out.data(),&value,sizeof(value));return out;
 }
 static bool decode_value(const std::uint8_t* data,std::size_t n,Value& out){
   if(!data||n!=sizeof(out))return false;
   std::memcpy(&out,data,sizeof(out));return true;
 }
};
struct SyntheticList : SyntheticCollection {
 static constexpr umaa::CollectionKind kind=umaa::CollectionKind::List;
};
