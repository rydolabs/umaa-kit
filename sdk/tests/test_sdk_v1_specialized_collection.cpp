#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/sdk_v1_specialized_collection.hpp"
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
using namespace umaa::sdk_v1;
struct Contact {int number; SpecializationRef ref;};
struct Detail {int code;};
struct ContactSet {};
struct ContactList {};
struct NoSpecialization {};
namespace umaa::sdk_v1 {
template<> struct ServiceTraits<ContactSet> {
  using Element=Contact;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::ContactSet","demo:contacts",FlowKind::QuerylessRequestReply,
    ServiceKind::Report,CollectionShape::LargeSet,true};
};
template<> struct ServiceTraits<ContactList> {
  using Element=Contact;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::ContactList","demo:contacts",FlowKind::QuerylessRequestReply,
    ServiceKind::Report,CollectionShape::LargeList,true};
};
template<> struct ServiceTraits<NoSpecialization> {
  using Element=Contact;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::PlainSet","demo:contacts",FlowKind::QuerylessRequestReply,
    ServiceKind::Report,CollectionShape::LargeSet,false};
};
}
int main(){
  const auto a=Guid128::parse("00000000-0000-0000-0000-0000000000a1");
  const auto b=Guid128::parse("00000000-0000-0000-0000-0000000000b2");
  const auto project=[](const Contact& c){return c.ref;};
  SpecializedCollectionConsumer<ContactSet,Detail> set(3);
  set.metadata({"contacts",2,"b",200,""});
  assert(!set.snapshot(project));
  assert(set.upsert({"a",100,Contact{1,{a,100}},""}));
  assert(set.upsert({"b",200,Contact{2,{b,200}},""}));
  assert(!set.snapshot(project));
  assert(set.general(a,100,Contact{1,{a,100}}));
  assert(set.specialization(a,100,Detail{10}));
  assert(set.general(b,200,Contact{2,{b,200}}));
  assert(!set.snapshot(project)); // no partial collection
  assert(set.specialization(b,200,Detail{20}));
  assert(set.snapshot(project)&&set.snapshot(project)->size()==2);
  assert(set.general(b,300,Contact{3,{b,300}}));
  assert(!set.snapshot(project)); // mismatched revision
  assert(set.specialization(b,300,Detail{30}));
  assert(!set.snapshot(project)); // metadata still references 200
  assert(set.upsert({"b",300,Contact{3,{b,300}},""}));
  set.metadata({"contacts",2,"b",300,""});
  assert(set.snapshot(project)&&set.snapshot(project)->at(1).specialization.code==30);
  assert(set.dispose_specialization(a,400));
  assert(!set.snapshot(project)); // tombstone
  assert(!set.specialization(a,100,Detail{99}));
  assert(!set.snapshot(project));
  set.reset_collection();
  assert(!set.snapshot(project));
  // Ordered List must be assembled in linked order before resolving.
  SpecializedCollectionConsumer<ContactList,Detail> list(3);
  list.metadata({"ordered",2,"b",200,"a"});
  assert(list.upsert({"b",200,Contact{2,{b,200}},""}));
  assert(list.upsert({"a",100,Contact{1,{a,100}},"b"}));
  assert(list.general(a,100,Contact{1,{a,100}}));
  assert(list.specialization(a,100,Detail{10}));
  assert(list.general(b,200,Contact{2,{b,200}}));
  assert(list.specialization(b,200,Detail{20}));
  auto ordered=list.snapshot(project);
  assert(ordered&&ordered->size()==2);
  assert(ordered->at(0).general.number==1);
  assert(ordered->at(1).general.number==2);
  bool invalid=false;
  try{SpecializedCollectionConsumer<NoSpecialization,Detail> wrong; (void)wrong;}
  catch(const std::invalid_argument&){invalid=true;}
  assert(invalid);
}
