#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/split_collection_joiner.hpp"
#include "fixtures/synthetic_split_collection_binding.hpp"
#include "umaa/collection_batch_builder.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
template<class S> void run(){
  using T=typename S::Value;
  using Batch=umaa::CoherentCollectionBatch<T>;
  umaa::CollectionBatchBuilder<T> builder(S::kind,S::collection_id,8);
  auto snapshot=[&]{
    if constexpr(S::kind==umaa::CollectionKind::List)
      return builder.snapshot(1,{{"a",1,10,"b"},{"b",2,20,""}});
    else return builder.snapshot(1,{{"a",1,10,""},{"b",2,20,""}});
  }();
  auto meta=snapshot;meta.operations.clear();
  umaa::SplitCollectionJoiner<S> join(8);
  join.expect_snapshot(1);
  join.ingest_metadata(meta);
  assert(join.pending()&&!join.snapshot());
  join.ingest_elements(snapshot);
  assert(!join.pending()&&join.snapshot()&&join.snapshot()->size()==2);
  auto revision=[&] {
    if constexpr(S::kind==umaa::CollectionKind::List)
      return builder.revise({umaa::CollectionOperation<T>::upsert({"a",3,11,"b"})},"a");
    else return builder.revise({umaa::CollectionOperation<T>::upsert({"a",3,15,""}),
                               umaa::CollectionOperation<T>::dispose("b",4)});
  }();
  auto meta2=revision;meta2.operations.clear();
  join.ingest_elements(revision);
  assert(join.pending()&&!join.snapshot()); // no partial exposure
  join.ingest_metadata(meta2);
  assert(join.sequence()==2&&join.snapshot());
  join.dispose_part(true);
  assert(!join.disposed()&&!join.snapshot());
  join.dispose_part(false);
  assert(join.disposed()&&!join.snapshot());
  // A half-only revision cannot be publicly committed.
  umaa::SplitCollectionJoiner<S> lost(8);
  lost.expect_snapshot(1);
  lost.ingest_elements(snapshot);
  assert(!lost.snapshot());
  bool rejected=false;
  try{lost.ingest_elements(snapshot);}
  catch(const std::runtime_error&){rejected=true;}
  assert(rejected&&lost.faulted()&&!lost.snapshot());
  // Metadata mismatch never commits the tempting Elements snapshot.
  umaa::SplitCollectionJoiner<S> corrupt(8);
  corrupt.expect_snapshot(1);
  corrupt.ingest_elements(snapshot);
  meta.metadata.size=7;
  rejected=false;
  try{corrupt.ingest_metadata(meta);}
  catch(const std::runtime_error&){rejected=true;}
  assert(rejected&&corrupt.faulted()&&!corrupt.snapshot());
  // An unexpected sequence/generation fails closed.
  umaa::SplitCollectionJoiner<S> gap(8);
  gap.expect_snapshot(1);
  rejected=false;
  try{gap.ingest_metadata(meta2);}
  catch(const std::runtime_error&){rejected=true;}
  assert(rejected&&gap.faulted());
  gap.expect_snapshot(2);
  assert(!gap.faulted()&&!gap.snapshot());
  // Old generation cannot resurrect a missing pair after trusted reset.
  rejected=false;
  try{gap.ingest_elements(snapshot);}
  catch(const std::runtime_error&){rejected=true;}
  assert(rejected&&gap.faulted());
}
int main(){
  run<SyntheticSplitSet>();
  run<SyntheticSplitList>();
}
