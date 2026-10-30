#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/dds_collection_endpoint.hpp"
#include "fixtures/synthetic_collection_binding.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
template<class F> bool rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 using Pub=umaa::DdsCollectionPublisher<SyntheticCollection>;
 using Op=umaa::CollectionOperation<std::int32_t>;
 const std::vector<umaa::CollectionElement<std::int32_t>> live{{"a",1,10,""}};
 {
   Pub p(argv[1],95,"set");
   assert(rejects([&]{p.snapshot(0,live);}));
   assert(p.faulted()); // bad generation cannot be followed by silent replay
   assert(rejects([&]{p.snapshot(1,live);}));
 }
 {
   Pub p(argv[1],95,"set");
   p.snapshot(1,live);
   assert(!p.faulted());
   assert(rejects([&]{p.revise({Op::dispose("missing",2)});}));
   assert(p.faulted());
   assert(rejects([&]{p.revise({Op::upsert({"a",3,12,""})});}));
 }
 {
   Pub p(argv[1],95,"set");
   p.snapshot(1,live);
   p.revise({Op::upsert({"a",2,11,""})});
   p.finish();
   assert(rejects([&]{p.revise({Op::upsert({"a",3,12,""})});}));
 }
 std::cout<<"PASS bounded Collection errors fence replay and preserve generation\n";
}
