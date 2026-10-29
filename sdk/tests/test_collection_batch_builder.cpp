#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/collection_batch_builder.hpp"
#include <cassert>
#include <stdexcept>
#include <vector>
template<class F> bool bad(F f){try{f();}catch(const std::exception&){return true;}return false;}
int main(){
 using O=umaa::CollectionOperation<int>;
 umaa::CollectionBatchBuilder<int> b(umaa::CollectionKind::Set,"battery",4);
 umaa::CoherentCollectionReceiver<int> r(umaa::CollectionKind::Set,4);
 auto first=b.snapshot(1,{{"a",1,10,""},{"b",2,20,""}});
 r.expect_snapshot(1);
 assert(r.apply_snapshot(first)&&r.snapshot()->size()==2);
 auto changed=b.revise({O::upsert({"a",3,15,""})});
 assert(changed.sequence==2);
 assert(r.apply_delta(changed));
 assert(r.snapshot()->size()==2 && r.snapshot()->at(0)==15);
 auto removed=b.revise({O::dispose("b",4)});
 assert(r.apply_delta(removed));
 assert(r.snapshot()->size()==1&&r.snapshot()->at(0)==15);
 assert(bad([&]{(void)b.revise({O::dispose("a",1)});}));
 assert(bad([&]{(void)b.snapshot(1,{});}));
 assert(b.sequence()==3);
 auto empty=b.revise({O::dispose("a",5)});
 assert(r.apply_delta(empty)&&r.snapshot()->empty());
 r.fail_unknown_delivery();
 assert(!r.snapshot());
 auto recovered=b.snapshot(2,{{"z",6,99,""}});
 r.expect_snapshot(2);
 assert(r.apply_snapshot(recovered)&&r.snapshot()->at(0)==99);
 assert(bad([&]{(void)b.snapshot(3,{{"x",1,1,""},{"x",2,2,""}});}));
 umaa::CollectionBatchBuilder<int> list(umaa::CollectionKind::List,"ordered",3);
 umaa::CoherentCollectionReceiver<int> sink(umaa::CollectionKind::List,3);
 auto full=list.snapshot(1,{{"a",1,10,"b"},{"b",2,20,""}});
 sink.expect_snapshot(1);
 assert(sink.apply_snapshot(full));
 assert(sink.snapshot()->at(0)==10&&sink.snapshot()->at(1)==20);
 auto delta=list.revise({O::upsert({"a",3,11,"b"})},"a");
 assert(sink.apply_delta(delta)&&sink.snapshot()->at(0)==11);
 assert(bad([&]{(void)list.revise({O::upsert({"b",4,21,"a"})},"a");}));
 assert(list.sequence()==2);
 return 0;
}
