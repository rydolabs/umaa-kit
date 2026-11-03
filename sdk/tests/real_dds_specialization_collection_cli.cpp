// Four independent genuine DDS Topics (Elements / Metadata / Generalization /
// Specialization) with a SINGLE unchanged typed executable for two vendors.
// Payloads and key mapping are synthetic; not official UMAA 6.0 TypeSupport.
#include "umaa/sdk_v1_application.hpp"
#include "umaa/dds_specialization_endpoint.hpp"
#include "fixtures/synthetic_split_collection_binding.hpp"
#include "fixtures/synthetic_specialization_binding.hpp"
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
using namespace std::chrono_literals;
struct SetV1 {};
struct ListV1 {};
namespace umaa::sdk_v1 {
template<> struct ServiceTraits<SetV1> {
  using Element=std::int32_t;
  using WireService=SyntheticSplitSet;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::MultiTopicSet","demo:collection-batch-v1",
    FlowKind::QuerylessRequestReply,ServiceKind::Status,CollectionShape::LargeSet,true};
};
template<> struct ServiceTraits<ListV1> {
  using Element=std::int32_t;
  using WireService=SyntheticSplitList;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::MultiTopicList","demo:collection-batch-v1",
    FlowKind::QuerylessRequestReply,ServiceKind::Status,CollectionShape::LargeList,true};
};
}
const auto id_a=umaa::sdk_v1::Guid128::parse("00000000-0000-0001-0000-000000001111");
const auto id_b=umaa::sdk_v1::Guid128::parse("00000000-0000-0002-0000-000000002222");
template<class Service> int subscriber(const char* plugin){
  try {
    umaa::sdk_v1::RuntimeApplication app;
    app.use_backend(plugin,46);
    auto collection=app.consume_split_collection<Service>("17",8);
    collection->expect_snapshot(1);
    umaa::DdsSpecializationConsumer<SyntheticSpecialization> detail(plugin,46,"17",8);
    bool saw_full=false,saw_hidden=false,saw_revision=false,saw_tombstone=false;
    const auto project=[](std::int32_t value)->umaa::sdk_v1::SpecializationRef {
      if(value==10)return {id_a,100};
      if(value==20)return {id_b,200};
      if(value==25)return {id_b,300};
      return {};
    };
    for(int i=0;i<550&&!(collection->disposed()&&detail.disposed());++i){
      collection->poll(4);
      detail.poll(4);
      auto resolved=detail.resolve_collection(collection->snapshot(),project);
      if(collection->sequence()==1 && resolved&&resolved->size()==2&&
         resolved->at(0).specialization==1000 &&
         resolved->at(1).specialization==2000)saw_full=true;
      if(saw_full&&collection->sequence()==1&&!resolved)saw_hidden=true;
      if(collection->sequence()==2&&resolved&&resolved->size()==2&&
         resolved->at(1).general==25 &&
         resolved->at(1).specialization==2500)saw_revision=true;
      if(saw_revision&&!resolved)saw_tombstone=true;
      std::this_thread::sleep_for(25ms);
    }
    if(saw_tombstone)std::this_thread::sleep_for(800ms);
    const bool ok=saw_full&&saw_hidden&&saw_revision&&saw_tombstone&&
         !collection->faulted()&&!detail.faulted()&&
         collection->disposed()&&detail.disposed();
    std::cout<<"SPECIALIZATION_SUB full="<<saw_full<<" skew_hidden="<<saw_hidden
             <<" updated="<<saw_revision<<" tombstone="<<saw_tombstone
             <<" disposed="<<(collection->disposed()&&detail.disposed())<<"\n";
    return ok?0:3;
  }catch(const std::exception& e){
    std::cerr<<"Specialization reader: "<<e.what()<<"\n";return 4;
  }
}
template<class Service> int publisher(const char* plugin){
  using Wire=typename umaa::sdk_v1::ServiceTraits<Service>::WireService;
  try{
    umaa::sdk_v1::RuntimeApplication app;
    app.use_backend(plugin,46);
    auto collection=app.publish_split_collection<Service>("17",8);
    umaa::DdsSpecializationPublisher<SyntheticSpecialization> detail(plugin,46,"17");
    // A snapshot is not visible until BOTH collection halves arrive, then
    // all independent Generalization/Specialization halves are current.
    if constexpr(Wire::kind==umaa::CollectionKind::List){
      collection->snapshot(1,{{"a",100,10,"b"},{"b",200,20,""}});
    }else{
      collection->snapshot(1,{{"a",100,10,""},{"b",200,20,""}});
    }
    std::this_thread::sleep_for(200ms);
    detail.general(id_a,100,10);
    detail.general(id_b,200,20);
    std::this_thread::sleep_for(180ms);
    detail.specialization(id_a,100,1000);
    std::this_thread::sleep_for(180ms);
    detail.specialization(id_b,200,2000);
    std::this_thread::sleep_for(430ms);
    // A one-sided change hides the previously resolved collection.
    detail.general(id_b,300,25);
    std::this_thread::sleep_for(370ms);
    detail.specialization(id_b,300,2500);
    std::this_thread::sleep_for(180ms);
    if constexpr(Wire::kind==umaa::CollectionKind::List){
      collection->revise({umaa::CollectionOperation<std::int32_t>::upsert(
         {"b",300,25,""})},"a");
    }else {
      collection->revise({umaa::CollectionOperation<std::int32_t>::upsert(
         {"b",300,25,""})});
    }
    std::this_thread::sleep_for(440ms);
    detail.dispose(id_a,400);
    std::this_thread::sleep_for(330ms);
    collection->finish(5000);
    detail.finish(5000);
    std::this_thread::sleep_for(200ms);
    const bool ok=collection->sequence()==2&&!collection->faulted()&&!detail.faulted();
    std::cout<<"SPECIALIZATION_PUB sequence="<<collection->sequence()
             <<" tombstone=1 disposed=1\n";
    return ok?0:5;
  }catch(const std::exception& e){
    std::cerr<<"Specialization writer: "<<e.what()<<"\n";return 6;
  }
}
int main(int argc,char** argv){
  if(argc!=4)return 2;
  const std::string action=argv[1],kind=argv[3];
  if(action=="subscriber"){
    if(kind=="set")return subscriber<SetV1>(argv[2]);
    if(kind=="list")return subscriber<ListV1>(argv[2]);
  }else if(action=="publisher"){
    if(kind=="set")return publisher<SetV1>(argv[2]);
    if(kind=="list")return publisher<ListV1>(argv[2]);
  }
  return 2;
}
