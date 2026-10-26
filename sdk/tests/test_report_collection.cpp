#include "umaa/large_collection.hpp"
#include "umaa/report_engine.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
struct H : umaa::ReportHandler<int> { int read_report() const override { return 1200; } };
int main() {
  H h;
  umaa::ReportProvider<int> rp("battery", h);
  umaa::ReportCache<int> cache;
  assert(cache.receive(rp.publish(10)));
  assert(cache.latest("battery")->data == 1200);
  assert(!cache.receive({"battery",9,999}));
  assert(cache.dispose("battery"));
  assert(!cache.latest("battery").has_value());

  umaa::CollectionAssembler<int> set(umaa::CollectionKind::Set, 3);
  // Metadata first, last element second.
  set.metadata({"id",2,"b",2,""});
  assert(!set.snapshot());
  set.upsert({"a",1,10,""});
  assert(!set.snapshot());
  set.upsert({"b",2,20,""});
  assert(set.snapshot() && set.snapshot()->size()==2);
  assert(set.commit_count()==1);
  // Element first, metadata second; ensure no intermediate commit on mutation.
  set.upsert({"a",3,11,""});
  assert(set.snapshot()->at(0)==10);
  set.metadata({"id",2,"a",3,""});
  assert(set.snapshot()->at(0)==11);
  set.dispose("b");
  set.metadata({"id",1,"b",std::nullopt,""});
  assert(set.snapshot()->size()==1 && set.snapshot()->at(0)==11);
  bool raised = false;
  try { set.upsert({"c",4,1,""}); set.upsert({"d",5,2,""}); set.upsert({"e",6,3,""}); }
  catch (const std::length_error&) { raised = true; }
  assert(raised);
  umaa::CollectionAssembler<int> list(umaa::CollectionKind::List);
  list.upsert({"b",2,20,""});
  list.upsert({"a",1,10,"b"});
  list.metadata({"list",2,"b",2,"a"});
  assert(list.snapshot() && list.snapshot()->at(0)==10 && list.snapshot()->at(1)==20);
  // Multiple collection IDs must never be silently interleaved on one
  // assembler; reset is explicit and testable.
  bool bad_collection = false;
  try { set.metadata({"wrong-id",1,"a",3,""}); }
  catch (const std::invalid_argument&) { bad_collection = true; }
  assert(bad_collection);
  // Timed DDS disposal guards against out-of-order element resurrection.
  umaa::CollectionAssembler<int> guarded(umaa::CollectionKind::Set, 3);
  assert(guarded.upsert({"x", 10, 7, ""}));
  assert(!guarded.dispose("x", 9));
  assert(guarded.dispose("x", 11));
  assert(!guarded.upsert({"x", 10, 70, ""}));
  assert(!guarded.upsert({"x", 11, 70, ""}));
  assert(!guarded.dispose("x", 11));
  assert(guarded.upsert({"x", 12, 12, ""}));
  guarded.metadata({"new", 1, "x", 12, ""});
  assert(guarded.snapshot() && guarded.snapshot()->at(0) == 12);
  assert(!guarded.dispose("x", 11));
  assert(guarded.snapshot()->at(0) == 12);
  guarded.clear();
  guarded.metadata({"another", 0, "", std::nullopt, ""});
  assert(guarded.snapshot() && guarded.snapshot()->empty());
  // Explicit memory cap instead of unlimited tombstone growth.
  umaa::CollectionAssembler<int> bounded(umaa::CollectionKind::Set, 2);
  assert(bounded.dispose("a", 1));
  assert(bounded.dispose("b", 1));
  bool over_cap = false;
  try { bounded.dispose("c", 1); }
  catch (const std::length_error&) { over_cap = true; }
  assert(over_cap && bounded.buffered_elements() == 0);
  // Cyclic links must not generate a new committed list.
  list.upsert({"b",3,21,"a"});
  list.metadata({"list",2,"b",3,"a"});
  assert(list.commit_count()==1);
}
