// Vendor-neutral real DDS Collection Set/List atomic frame smoke.
// This is synthetic single-sample application framing, NOT DDS GROUP QoS.
#include "umaa/application.hpp"
#include "fixtures/synthetic_collection_binding.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
using namespace std::chrono_literals;
template<class S> int subscriber(const char* backend){
 try{
   auto consumer=[&] {
     umaa::Application app;
     app.use_backend(backend,46);
     return app.consume_collection<S>("17",8);
   }();
   consumer->expect_snapshot(1);
   bool first=false,delta=false,disposed=false;
   for(int i=0;i<260&&!disposed;++i){
     consumer->poll(1);
     auto data=consumer->snapshot();
     if(consumer->sequence()==1&&data&&data->size()==2)
       first=true;
     if(consumer->sequence()==2&&data){
       if constexpr(S::kind==umaa::CollectionKind::Set)
         delta= data->size()==1&&data->at(0)==15;
       else delta=data->size()==2&&data->at(0)==11&&data->at(1)==20;
     }
     if(consumer->disposed()) {
       disposed=!consumer->snapshot();
     }
     std::this_thread::sleep_for(25ms);
   }
   // A reliable writer can still be awaiting the Reader's ACK for
   // Dispose. Keep this Reader alive briefly before shutting the process.
   if(disposed)std::this_thread::sleep_for(900ms);
   const bool ok=first&&delta&&disposed&&!consumer->faulted();
   std::cout<<"COLLECTION_SUB first="<<first<<" delta="<<delta
            <<" disposed="<<disposed<<"\n";
   return ok?0:3;
 }catch(const std::exception& e){
   std::cerr<<"Collection subscriber: "<<e.what()<<"\n";return 4;
 }
}
template<class S> int publisher(const char* backend){
 try{
   auto sender=[&] {
     umaa::Application app;
     app.use_backend(backend,46);
     return app.publish_collection<S>("17",8);
   }();
   if constexpr(S::kind==umaa::CollectionKind::Set) {
     sender->snapshot(1,{{"a",1,10,""},{"b",2,20,""}});
     std::this_thread::sleep_for(200ms);
     sender->revise({umaa::CollectionOperation<int>::upsert({"a",3,15,""}),
                    umaa::CollectionOperation<int>::dispose("b",4)});
   }else{
     sender->snapshot(1,{{"a",1,10,"b"},{"b",2,20,""}});
     std::this_thread::sleep_for(200ms);
     sender->revise({umaa::CollectionOperation<int>::upsert({"a",3,11,"b"})},"a");
   }
   std::this_thread::sleep_for(200ms);
   sender->finish(5000);
   std::this_thread::sleep_for(200ms);
   std::cout<<"COLLECTION_PUB sequence="<<sender->sequence()
            <<" disposed=1\n";
   return sender->sequence()==2&&!sender->faulted()?0:5;
 }catch(const std::exception& e){
   std::cerr<<"Collection publisher: "<<e.what()<<"\n";return 6;
 }
}
int main(int argc,char** argv){
 if(argc!=4)return 2;
 const std::string action=argv[1],kind=argv[3];
 if(action=="subscriber") {
   if(kind=="set")return subscriber<SyntheticCollection>(argv[2]);
   if(kind=="list")return subscriber<SyntheticList>(argv[2]);
 }
 if(action=="publisher") {
   if(kind=="set")return publisher<SyntheticCollection>(argv[2]);
   if(kind=="list")return publisher<SyntheticList>(argv[2]);
 }
 return 2;
}
