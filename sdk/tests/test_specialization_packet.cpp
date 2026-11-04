#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/dds_specialization_endpoint.hpp"
#include "fixtures/synthetic_specialization_binding.hpp"
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <vector>
int main(){
  using Codec=umaa::SyntheticSpecializationPacket;
  using Guid=umaa::sdk_v1::Guid128;
  const auto id=Guid::parse("11000000-0000-0000-0000-000000000001");
  auto data=Codec::encode({id,100,false,SyntheticSpecialization::encode_general(123)});
  auto parsed=Codec::decode(data.data(),data.size());
  assert(parsed.id==id&&parsed.timestamp==100&&!parsed.tombstone);
  std::int32_t value=0;
  assert(SyntheticSpecialization::decode_general(parsed.value.data(),parsed.value.size(),value));
  assert(value==123);
  auto tomb=Codec::encode({id,200,true,{}});
  auto decoded=Codec::decode(tomb.data(),tomb.size());
  assert(decoded.tombstone&&decoded.timestamp==200&&decoded.value.empty());
  auto invalid=data;
  invalid[0]^=0x10;
  bool failed=false;
  try{Codec::decode(invalid.data(),invalid.size());}
  catch(const std::invalid_argument&){failed=true;}
  assert(failed);
  invalid=data;invalid[5]=2; // tombstone with nonempty payload
  failed=false;
  try{Codec::decode(invalid.data(),invalid.size());}
  catch(const std::invalid_argument&){failed=true;}
  assert(failed);
  failed=false;
  try{Codec::decode(data.data(),data.size()-1);}
  catch(const std::invalid_argument&){failed=true;}
  assert(failed);
  failed=false;
  try{Codec::encode({Guid{},100,false,{1}});}
  catch(const std::invalid_argument&){failed=true;}
  assert(failed);
}
