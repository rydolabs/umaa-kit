#include "umaa/sdk_v1_core.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
using namespace umaa::sdk_v1;

struct ControlService{};
struct ContactService{};
struct Contact {int value;SpecializationRef ref;};
struct RadarDetail {int range;};
namespace umaa::sdk_v1 {
template<> struct ServiceTraits<ControlService> {
  inline static constexpr ServiceDescriptor descriptor{
    "demo:engine-control", "demo:engine-v1",FlowKind::CommandResponse,ServiceKind::Control};
};
template<> struct ServiceTraits<ContactService> {
  inline static constexpr ServiceDescriptor descriptor{
    "demo:contact", "demo:contact-v1",FlowKind::QuerylessRequestReply,
    ServiceKind::Report,CollectionShape::LargeSet,true};
};
}

template<class F> bool throws(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main(){
  auto a=Guid128::parse("00112233-4455-6677-8899-aabbccddeeff");
  auto b=Guid128::parse("00112233-4455-6677-8899-aabbccddeef0");
  assert(a.str()=="00112233-4455-6677-8899-aabbccddeeff");
  assert(a!=b && b<a);
  assert(throws([]{Guid128::parse("xyz");}));
  assert(throws([]{Guid128::parse("00112233-4455-6677-8899-aabbccddeezz");}));
  assert(Guid128{}.is_nil());
  CommandIdentity identity{{a,b},{b,a},a,100};
  assert(identity.valid());
  identity.session_id={};assert(!identity.valid());

  ServiceCatalog catalog;
  catalog.add<ControlService>();catalog.add<ContactService>();
  assert(catalog.size()==2);
  assert(catalog.at("demo:contact").collection==CollectionShape::LargeSet);
  assert(catalog.contains("demo:engine-control"));
  assert(throws([&]{catalog.add<ContactService>();}));
  assert(throws([&]{catalog.at("unknown");}));

  SpecializationJoin<Contact,RadarDetail> join(2);
  Contact first{10,{a,100}};
  Contact second{20,{b,200}};
  assert(join.put_general(a,100,first));
  assert(!join.resolve(first.ref));
  assert(join.put_specialization(a,100,RadarDetail{123}));
  assert(join.resolve(first.ref)->specialization.range==123);
  assert(!join.put_specialization(a,99,RadarDetail{999}));
  auto snapshot=join.resolve_collection(std::optional<std::vector<Contact>>{{first}},
    [](const Contact& v){return v.ref;});
  assert(snapshot && snapshot->size()==1 && snapshot->front().general.value==10);
  assert(!join.resolve_collection(std::optional<std::vector<Contact>>{{first,second}},
    [](const Contact& v){return v.ref;}));
  assert(join.put_general(b,200,second));
  assert(join.put_specialization(b,200,RadarDetail{200}));
  snapshot=join.resolve_collection(std::optional<std::vector<Contact>>{{first,second}},
    [](const Contact& v){return v.ref;});
  assert(snapshot && snapshot->size()==2);
  assert(join.put_general(a,300,Contact{30,{a,300}}));
  assert(!join.resolve(first.ref));
  assert(!join.resolve({a,300}));
  assert(join.put_specialization(a,300,RadarDetail{300}));
  assert(join.resolve({a,300})->general.value==30);
  assert(join.dispose(a,400));
  assert(!join.resolve({a,300}));
  assert(!join.put_general(a,399,Contact{39,{a,399}}));
  assert(join.put_general(a,401,Contact{40,{a,401}}));
  assert(join.put_specialization(a,401,RadarDetail{401}));
  assert(join.resolve({a,401}).has_value());
  assert(throws([&]{join.put_general(Guid128{},1,first);}));
  assert(throws([&]{join.put_general(Guid128::parse("00000000-0000-0000-0000-000000000001"),100,first);}));
  assert(!join.resolve_collection(std::optional<std::vector<Contact>>{},
    [](const Contact& v){return v.ref;}));
  assert(join.resolve_collection(std::optional<std::vector<Contact>>(std::vector<Contact>{}),
    [](const Contact& v){return v.ref;})->empty());
}
