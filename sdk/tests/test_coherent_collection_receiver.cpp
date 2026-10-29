#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/collection_transport.hpp"
#include <cassert>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using R=umaa::CoherentCollectionReceiver<int>;
using B=umaa::CoherentCollectionBatch<int>;
using O=umaa::CollectionOperation<int>;
template<class F> bool rejected(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
 R receiver(umaa::CollectionKind::Set,3);
 B first{1,1,{"c",2,"b",20,""},{
   O::upsert({"a",10,100,""}),O::upsert({"b",20,200,""})}};
 assert(rejected([&]{receiver.apply_delta(first);}));
 receiver.expect_snapshot(1);
 assert(!receiver.snapshot());
 assert(receiver.apply_snapshot(first));
 assert(receiver.snapshot() && receiver.snapshot()->size()==2);
 assert(receiver.sequence()==1);
 B second{1,2,{"c",2,"a",30,""},{
   O::upsert({"a",30,300,""})}};
 assert(receiver.apply_delta(second));
 const auto second_values=receiver.snapshot();
 assert(second_values && std::find(second_values->begin(),second_values->end(),300)!=second_values->end());
 B gap{1,4,{"c",2,"b",40,""},{
   O::upsert({"b",40,400,""})}};
 assert(rejected([&]{receiver.apply_delta(gap);}));
 assert(receiver.state()==R::State::ResyncRequired);
 assert(!receiver.snapshot());
 assert(rejected([&]{receiver.apply_delta(second);}));
 assert(rejected([&]{receiver.expect_snapshot(1);}));
 receiver.expect_snapshot(2);
 B recovered{2,1,{"c",1,"z",50,""},{
   O::upsert({"z",50,700,""})}};
 assert(receiver.apply_snapshot(recovered));
 assert(receiver.snapshot()->size()==1);
 assert(receiver.snapshot()->at(0)==700);
 B missing{2,2,{"c",3,"z",60,""},{
   O::upsert({"z",60,701,""})}};
 assert(rejected([&]{receiver.apply_delta(missing);}));
 assert(!receiver.snapshot());
 receiver.expect_snapshot(3);
 B disposed{3,1,{"c",0,"",std::nullopt,""},{}};
 assert(receiver.apply_snapshot(disposed));
 assert(receiver.snapshot() && receiver.snapshot()->empty());
 receiver.fail_unknown_delivery();
 assert(!receiver.snapshot());
 receiver.expect_snapshot(4);
 B bounded{4,1,{"c",4,"z",1,""},{}};
 assert(rejected([&]{receiver.apply_snapshot(bounded);}));
 R list(umaa::CollectionKind::List,3);
 list.expect_snapshot(1);
 B ordered{1,1,{"list",2,"b",2,"a"},{
   O::upsert({"b",2,22,""}),O::upsert({"a",1,11,"b"})}};
 assert(list.apply_snapshot(ordered));
 assert(list.snapshot()->at(0)==11 && list.snapshot()->at(1)==22);
 B cycle{1,2,{"list",2,"b",3,"a"},{
   O::upsert({"b",3,23,"a"})}};
 assert(rejected([&]{list.apply_delta(cycle);}));
 assert(!list.snapshot());
 std::cout<<"PASS collection atomic batches, bounds, list, gaps and resync\n";
}
