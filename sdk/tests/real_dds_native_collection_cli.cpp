// Real independent-process native Element/Metadata + General/Special topics.
// The same vendor-neutral executable is loaded against Cyclone/Fast DDS.
// Fixture wire type is synthetic; NOT official UMAA 6.0 TypeSupport.
#include "umaa/sdk_v1_application.hpp"
#include "fixtures/synthetic_native_collection_binding.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using namespace std::chrono_literals;

struct NativeSetService{};
struct NativeListService{};
namespace umaa::sdk_v1 {
template<> struct ServiceTraits<NativeSetService>{
  using WireService=SyntheticNativeSet;
  using Element=SyntheticNativeSet::Value;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::EO::NativeContacts","demo:native-metadata-v1",
    FlowKind::QuerylessRequestReply,ServiceKind::Report,CollectionShape::LargeSet,true};
};
template<> struct ServiceTraits<NativeListService>{
  using WireService=SyntheticNativeList;
  using Element=SyntheticNativeList::Value;
  inline static constexpr ServiceDescriptor descriptor{
    "Demo::EO::NativeContacts","demo:native-metadata-v1",
    FlowKind::QuerylessRequestReply,ServiceKind::Report,CollectionShape::LargeList,true};
};
}

template<class Service> int execute(const std::string& mode,const std::string& backend) {
 using namespace umaa::sdk_v1;
 RuntimeApplication app;
 app.use_backend(backend,44);
 auto gid1=Guid128::parse("00000000-0000-0000-0000-000000000065");
 auto gid2=Guid128::parse("00000000-0000-0000-0000-000000000066");
 const bool list=descriptor<Service>().collection==CollectionShape::LargeList;
 NativeContact first{1,{gid1,100}},second{2,{gid2,200}};
 if(mode=="producer"){
   auto collection=app.publish_native_collection<Service>("17");
   auto specialization=app.publish_native_specialization<Service>();
   // Metadata-first delivery may race with independent Element writers.
   collection->publish_metadata({SyntheticNativeSet::collection_id,2,"102",200,
                                 list?"101":""});
   collection->upsert({"101",100,first,list?"102":""});
   collection->upsert({"102",200,second,""});
   specialization->general(gid1,100,first);
   specialization->specialized(gid1,100,NativeSpecialized{11});
   specialization->general(gid2,200,second);
   specialization->specialized(gid2,200,NativeSpecialized{22});
   std::this_thread::sleep_for(500ms);
   NativeContact second_new{3,{gid2,300}};
   // A revised Collection metadata can commit while its corresponding
   // Specialization Topic is delayed. No partial joined snapshot may leak.
   collection->upsert({"102",300,second_new,""});
   collection->publish_metadata({SyntheticNativeSet::collection_id,2,"102",300,
                                 list?"101":""});
   specialization->general(gid2,300,second_new);
   std::this_thread::sleep_for(650ms);
   specialization->specialized(gid2,300,NativeSpecialized{33});
   std::this_thread::sleep_for(450ms);
   // Real DDS Dispose follows an explicit time-stamped tombstone sample,
   // then metadata commits the deletion (List start becomes 102).
   collection->dispose_element("101",400);
   collection->publish_metadata({SyntheticNativeSet::collection_id,1,"101",
                                 std::nullopt,list?"102":""});
   collection->flush(5000);
   specialization->flush(5000);
   std::this_thread::sleep_for(750ms);
   std::cout<<"NATIVE_PRODUCER shape="<<(list?"list":"set")
            <<" metadata_element_separate=1 general_special_separate=1\n";
   return 0;
 }
 auto collection=app.consume_native_specialized_collection<Service>("17");
 bool initial=false,pending=false,revised=false,final=false,saw_dispose=false;
 std::uint64_t last_commit=0;
 const auto end=std::chrono::steady_clock::now()+18s;
 while(std::chrono::steady_clock::now()<end&&!(final&&saw_dispose)){
   collection->poll(32);
   const auto current=collection->snapshot();
   const auto commit=collection->commits();
   if(commit>=2 && !current)pending=true;
   if(current && current->size()==2 &&
      current->at(0).general.value==1 && current->at(1).general.value==2 &&
      current->at(0).specialization.code==11 &&
      current->at(1).specialization.code==22)initial=true;
   if(current && current->size()==2 &&
      current->at(1).general.value==3 &&
      current->at(1).specialization.code==33)revised=true;
   if(current && current->size()==1 &&
      current->at(0).general.value==3 &&
      current->at(0).specialization.code==33)final=true;
   saw_dispose=saw_dispose||collection->saw_element_dispose();
   if(commit>last_commit)last_commit=commit;
   std::this_thread::sleep_for(15ms);
 }
 saw_dispose=saw_dispose||collection->saw_element_dispose();
 const bool ok=initial&&pending&&revised&&final&&saw_dispose&&
               !collection->faulted()&&last_commit>=3;
 std::cout<<"NATIVE_CONSUMER shape="<<(list?"list":"set")
          <<" initial="<<initial<<" pending="<<pending
          <<" revised="<<revised<<" final="<<final
          <<" dispose="<<saw_dispose<<" commits="<<last_commit<<"\n";
 return ok?0:5;
}
int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string mode=argv[1],shape=argv[3];
 if((mode!="producer"&&mode!="consumer")||(shape!="set"&&shape!="list"))return 2;
 try{
   if(shape=="set")return execute<NativeSetService>(mode,argv[2]);
   return execute<NativeListService>(mode,argv[2]);
 }catch(const std::exception& e){
   std::cerr<<"Native Collection/GS DDS: "<<e.what()<<"\n";return 7;
 }
}
