// Synthetic transport envelope for whole atomic Collection transactions.
// A complete revision travels in ONE bounded DDS data sample; it does NOT
// claim RTPS GROUP coherent presentation or official UMAA LargeSet/List wire.
#pragma once
#include "umaa/collection_transport.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
template<class Service> class CollectionFrameCodec final {
public:
 using Value=typename Service::Value;
 using Batch=CoherentCollectionBatch<Value>;
 using Operation=CollectionOperation<Value>;
 static constexpr std::size_t max_bytes=65536;
 static constexpr std::size_t max_operations=1024;
 static std::vector<std::uint8_t> encode(const Batch& batch){
   Writer w;
   w.number(0x554D4342u,4); // "BCMU" in little endian; fixed magic
   w.number(1,2);
   w.number(Service::kind==CollectionKind::List?2:1,1);
   w.number(0,1); // reserved; never silently accept future flags
   w.number(batch.generation,8);w.number(batch.sequence,8);
   if(batch.operations.size()>max_operations||
      batch.metadata.size>max_operations)
     throw std::length_error("collection frame exceeds operation cap");
   w.number(batch.operations.size(),4);
   w.string(batch.metadata.collection_id);
   w.number(batch.metadata.size,4);
   w.string(batch.metadata.update_element_id);
   w.number(batch.metadata.update_element_timestamp.has_value()?1:0,1);
   if(batch.metadata.update_element_timestamp)
     w.signed_number(*batch.metadata.update_element_timestamp);
   w.string(batch.metadata.starting_element_id);
   for(const auto& op:batch.operations){
     const bool upsert=op.kind==Operation::Kind::Upsert;
     if(!upsert&&op.kind!=Operation::Kind::Dispose)
       throw std::invalid_argument("invalid collection mutation kind");
     w.number(upsert?1:2,1);
     if(upsert){
       w.string(op.element.element_id);
       w.signed_number(op.element.timestamp);
       w.string(op.element.next_id);
       const auto payload=Service::encode_value(op.element.value);
       if(payload.size()>4096||payload.empty())
         throw std::length_error("invalid Collection value payload");
       w.number(payload.size(),4);
       w.bytes(payload.data(),payload.size());
     }else{
       w.string(op.disposed_id);w.signed_number(op.dispose_timestamp);
     }
   }
   if(!batch.generation||!batch.sequence||batch.metadata.collection_id.empty())
     throw std::invalid_argument("invalid Collection frame identity");
   return std::move(w.output);
 }
 static Batch decode(const std::uint8_t* bytes,std::size_t n) {
   if(!bytes||n<32||n>max_bytes)
     throw std::invalid_argument("invalid Collection frame size");
   Reader r{bytes,n};
   if(r.number(4)!=0x554D4342u||r.number(2)!=1 ||
      r.number(1)!=(Service::kind==CollectionKind::List?2u:1u) ||
      r.number(1)!=0)throw std::invalid_argument("unknown Collection frame version/kind");
   Batch batch;
   batch.generation=r.number(8);batch.sequence=r.number(8);
   const auto count=r.number(4);
   if(count>max_operations)throw std::length_error("Collection frame operation cap");
   batch.metadata.collection_id=r.string();
   batch.metadata.size=static_cast<std::size_t>(r.number(4));
   if(batch.metadata.size>max_operations)
     throw std::length_error("Collection frame element cap");
   batch.metadata.update_element_id=r.string();
   const auto has_ts=r.number(1);
   if(has_ts>1)throw std::invalid_argument("invalid timestamp presence");
   if(has_ts)batch.metadata.update_element_timestamp=r.signed_number();
   batch.metadata.starting_element_id=r.string();
   batch.operations.reserve(static_cast<std::size_t>(count));
   for(std::uint64_t i=0;i<count;++i){
     const auto op=r.number(1);
     if(op==1){
       CollectionElement<Value> element;
       element.element_id=r.string();
       element.timestamp=r.signed_number();
       element.next_id=r.string();
       const auto size=r.number(4);
       if(!size||size>4096||size>r.remaining())
         throw std::invalid_argument("invalid Collection value length");
       Value value{};
       if(!Service::decode_value(r.data()+r.cursor(),static_cast<std::size_t>(size),value))
         throw std::invalid_argument("Collection value codec rejected payload");
       r.skip(static_cast<std::size_t>(size));
       element.value=std::move(value);
       batch.operations.push_back(Operation::upsert(std::move(element)));
     }else if(op==2){
       auto key=r.string();
       const auto stamp=r.signed_number();
       batch.operations.push_back(Operation::dispose(std::move(key),stamp));
     }else throw std::invalid_argument("unknown Collection mutation");
   }
   if(r.remaining()!=0||!batch.generation||!batch.sequence||
      batch.metadata.collection_id.empty())
     throw std::invalid_argument("trailing or incomplete Collection frame");
   return batch;
 }
private:
 struct Writer {
   std::vector<std::uint8_t> output;
   void bytes(const std::uint8_t* p,std::size_t n){
     if(n>max_bytes-output.size())throw std::length_error("Collection frame too large");
     output.insert(output.end(),p,p+n);
   }
   void number(std::uint64_t value,std::size_t width){
     if(width!=1&&width!=2&&width!=4&&width!=8)
       throw std::invalid_argument("invalid integer width");
     if(width<8 && value> ((std::uint64_t(1)<<(8*width))-1))
       throw std::length_error("Collection integer overflow");
     for(std::size_t i=0;i<width;++i) {
       if(output.size()==max_bytes)throw std::length_error("Collection frame too large");
       output.push_back(static_cast<std::uint8_t>(value>>(8*i)));
     }
   }
   void signed_number(std::int64_t value){
     std::uint64_t bits=0;std::memcpy(&bits,&value,sizeof(bits));number(bits,8);
   }
   void string(const std::string& value){
     if(value.size()>256)throw std::length_error("Collection key exceeds 256 bytes");
     number(value.size(),2);
     bytes(reinterpret_cast<const std::uint8_t*>(value.data()),value.size());
   }
 };
 struct Reader {
   const std::uint8_t* input;
   std::size_t length,position{0};
   std::size_t remaining() const{return length-position;}
   const std::uint8_t* data() const{return input;}
   std::size_t cursor() const{return position;}
   void skip(std::size_t n){
     if(n>remaining())throw std::invalid_argument("truncated Collection frame");
     position+=n;
   }
   std::uint64_t number(std::size_t n){
     if(n>remaining())throw std::invalid_argument("truncated Collection frame integer");
     std::uint64_t value=0;
     for(std::size_t i=0;i<n;++i)value|=std::uint64_t(input[position++])<<(8*i);
     return value;
   }
   std::int64_t signed_number(){
     const auto bits=number(8);
     std::int64_t value=0;std::memcpy(&value,&bits,sizeof(value));return value;
   }
   std::string string(){
     const auto len=number(2);
     if(len>256||len>remaining())throw std::invalid_argument("truncated Collection key");
     const char* start=reinterpret_cast<const char*>(input+position);
     position+=static_cast<std::size_t>(len);
     return {start,static_cast<std::size_t>(len)};
   }
 };
};
} // namespace umaa
