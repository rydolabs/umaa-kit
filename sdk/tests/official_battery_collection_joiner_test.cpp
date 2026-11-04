// No DDS or original copyrighted IDL in this test: exact host event-sequence
// oracles for EO ICD 6.0 3.8.1--3.8.5.
#include "umaa/official_battery_collection.hpp"
#include "umaa/official_battery_collection_router.hpp"
#include <cassert>
#include <cstdio>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace b = umaa::official_battery;
using Joiner = b::LargeListJoiner;

static b::Guid guid(unsigned char n) {
  b::Guid result{};
  result[0] = n;
  return result;
}
static b::Report report(unsigned size, unsigned char checkpoint, int nanos,
                        int report_seq = 1, bool has_timestamp = true) {
  b::Report value{};
  value.source.id = guid(0x33);
  value.source.parentID = guid(0);
  value.timeStamp = {1700000000 + report_seq, 10};
  value.state = b::State::ON;
  auto& m = value.cellsListMetadata;
  m.listID = guid(0x11);
  m.updateElementID = guid(checkpoint);
  m.startingElementID = guid(0x21);
  m.size = static_cast<std::int32_t>(size);
  if (has_timestamp) m.updateElementTimestamp = {1700000100, nanos};
  return value;
}
static b::ListElement element(unsigned char id, unsigned char next,
                             int nanos) {
  b::ListElement value{};
  value.listID = guid(0x11);
  value.elementID = guid(id);
  value.elementTimestamp = {1700000100, nanos};
  value.element = {0.5, 25.0, 3.75};
  if (next) value.nextElementID = guid(next);
  return value;
}
static Joiner ready(std::size_t max = 128, std::size_t reports = 8) {
  return Joiner(Joiner::ElementOrdering::CallerVerifiedTopicOrdered, max, reports);
}
static void rejected(const char* name, const std::function<void()>& f) {
  try { f(); }
  catch (const std::exception&) { return; }
  throw std::runtime_error(std::string("Expected fail-closed rejection: ") + name);
}
static void confirmed(const Joiner& j, unsigned size, std::uint64_t generation) {
  if (!j.latest_stable() || j.latest_stable()->ordered.size() != size ||
      j.latest_stable()->generation != generation ||
      j.latest_stable()->nativeDdsCoherenceProved)
    throw std::runtime_error("stable snapshot not consistent with event evidence");
}

int main() {
  try {
    // Metadata can arrive before all element samples; do not commit early.
    auto j = ready();
    j.on_report(report(2, 0x22, 200));
    assert(!j.latest_stable() && j.pending_reports() == 1);
    j.on_element(element(0x21, 0x22, 100));
    assert(!j.latest_stable());
    j.on_element(element(0x22, 0, 200));
    confirmed(j, 2, 1);
    assert(j.latest_stable()->ordered[0].elementID == guid(0x21));
    assert(j.latest_stable()->ordered[1].elementID == guid(0x22));

    // Batch update: one cell changed, another added, checkpoint is last
    // element in the update and allows stable list publication.
    j.on_element(element(0x21, 0x22, 300));
    assert(j.latest_stable()->generation == 1);
    j.on_element(element(0x22, 0x23, 301));
    j.on_report(report(3, 0x23, 302, 2));
    assert(j.pending_reports() == 1);
    j.on_element(element(0x23, 0, 302));
    confirmed(j, 3, 2);

    // Delete-only final checkpoint: required DDS Dispose event must exist,
    // timestamp is absent, and surviving list links must be repaired.
    j.on_element(element(0x21, 0x23, 400));
    j.on_dispose(guid(0x11), guid(0x22));
    j.on_report(report(2, 0x22, 0, 3, false));
    confirmed(j, 2, 3);
    assert(j.latest_stable()->ordered[0].elementID == guid(0x21));
    assert(j.latest_stable()->ordered[1].elementID == guid(0x23));

    // Full delete and empty collection using a disposed element checkpoint.
    j.on_element(element(0x21, 0, 401));
    j.on_dispose(guid(0x11), guid(0x23));
    j.on_report(report(1, 0x23, 0, 4, false));
    confirmed(j, 1, 4);
    j.on_dispose(guid(0x11), guid(0x21));
    j.on_report(report(0, 0x21, 0, 5, false));
    confirmed(j, 0, 5);

    // Initially empty list uses NIL update ID, not a fabricated element.
    auto empty = ready();
    auto initial = report(0, 0, 0, 1, false);
    empty.on_report(initial);
    confirmed(empty, 0, 1);
    initial.timeStamp.seconds += 1;
    initial.cellsListMetadata.updateElementID = guid(0x21);
    auto no_tombstone = ready();
    no_tombstone.on_report(initial);
    if (no_tombstone.latest_stable() || no_tombstone.pending_reports() != 1)
      throw std::runtime_error("timestamp-less non-NIL checkpoint was accepted without Dispose");

    // Elements may arrive before metadata on the *other* topic.
    auto reverse = ready();
    reverse.on_element(element(0x21, 0, 100));
    assert(!reverse.latest_stable());
    reverse.on_report(report(1, 0x21, 100));
    confirmed(reverse, 1, 1);

    // Exactly matching checkpoint (ID AND timestamp) is required;
    // a later corrected element sample may complete a pending report.
    auto mismatch = ready();
    mismatch.on_element(element(0x21, 0, 100));
    mismatch.on_report(report(1, 0x21, 101));
    assert(!mismatch.latest_stable() && mismatch.pending_reports() == 1);
    mismatch.on_element(element(0x21, 0, 101));
    confirmed(mismatch, 1, 1);

    // A rejected oversized/illegal NIL Report MUST NOT poison the pending
    // queue or advance last_report_time_. A valid next Report can still commit.
    auto bounded = ready(1);
    bounded.on_element(element(0x21, 0, 100));
    rejected("oversize Report without state mutation", [&] {
      bounded.on_report(report(2, 0x22, 200));
    });
    if (bounded.pending_reports() != 0 || bounded.buffered_elements() != 1)
      throw std::runtime_error("rejected oversize Report mutated buffered state");
    bounded.on_report(report(1, 0x21, 100));
    confirmed(bounded, 1, 1);
    rejected("NIL Report cannot erase live collection", [&] {
      bounded.on_report(report(0, 0, 0, 2, false));
    });
    if (bounded.pending_reports() != 0)
      throw std::runtime_error("rejected NIL Report poisoned pending queue");
    bounded.on_report(report(1, 0x21, 100, 2));
    confirmed(bounded, 1, 2);

    // Two distinct original listIDs interleave Report/Element publications.
    // The host router isolates their committed snapshots without inventing
    // a Source identity for the separately keyed Collection Elements.
    b::LargeListRouter router(Joiner::ElementOrdering::CallerVerifiedTopicOrdered,
                               2, 4, 4);
    auto a_element = element(0x21, 0, 100);
    auto b_element = element(0x31, 0, 200);
    b_element.listID = guid(0x12);
    auto b_report = report(1, 0x31, 200);
    b_report.source.id = guid(0x44);
    b_report.cellsListMetadata.listID = guid(0x12);
    b_report.cellsListMetadata.startingElementID = guid(0x31);

    router.on_element(a_element); // element arrives before Report for list A
    router.on_report(b_report);   // metadata arrives before element for list B
    if (router.latest_stable(guid(0x11)) || router.latest_stable(guid(0x12)))
      throw std::runtime_error("early multi-list commitment");
    router.on_element(b_element);
    router.on_report(report(1, 0x21, 100));
    if (router.active_lists() != 2 ||
        !router.latest_stable(guid(0x11)) ||
        !router.latest_stable(guid(0x12)) ||
        router.latest_stable(guid(0x11))->generation != 1 ||
        router.latest_stable(guid(0x12))->generation != 1 ||
        router.latest_stable(guid(0x11))->ordered[0].elementID != guid(0x21) ||
        router.latest_stable(guid(0x12))->ordered[0].elementID != guid(0x31) ||
        router.latest_stable(guid(0x11))->nativeDdsCoherenceProved ||
        router.latest_stable(guid(0x12))->nativeDdsCoherenceProved)
      throw std::runtime_error("interleaved Battery lists not independently stable");

    // An update of list B leaves list A's verified generation unchanged.
    auto b_updated = b_element;
    b_updated.elementTimestamp.nanoseconds = 201;
    b_updated.element.temperature = 30;
    router.on_element(b_updated);
    auto b_next_report = b_report;
    ++b_next_report.timeStamp.seconds;
    b_next_report.cellsListMetadata.updateElementTimestamp =
        b_updated.elementTimestamp;
    router.on_report(b_next_report);
    if (router.latest_stable(guid(0x11))->generation != 1 ||
        router.latest_stable(guid(0x12))->generation != 2 ||
        router.latest_stable(guid(0x12))->ordered[0].element.temperature != 30)
      throw std::runtime_error("multi-list cross-contamination");

    // The SAME list ID from a different Source is ambiguous and fail-closed.
    rejected("duplicate ListID from another Source", [&] {
      auto forged_source = report(1, 0x21, 100, 2);
      forged_source.source.id = guid(0x55);
      router.on_report(forged_source);
    });
    if (router.latest_stable(guid(0x11))->generation != 1)
      throw std::runtime_error("wrong-Source Report invalidated stable list");

    // Bound active lists; do not auto-evict or allocate on rejected data.
    auto c_element = element(0x41, 0, 100);
    c_element.listID = guid(0x13);
    rejected("multi-list capacity", [&] { router.on_element(c_element); });
    if (router.active_lists() != 2 || router.latest_stable(guid(0x13)))
      throw std::runtime_error("capacity rejection evicted or allocated a list");
    if (!router.retire_list(guid(0x11)) ||
        router.retire_list(guid(0x11)) ||
        router.latest_stable(guid(0x11)))
      throw std::runtime_error("explicit Battery list retirement invalid");
    router.on_element(c_element);
    auto c_report = report(1, 0x41, 100);
    c_report.cellsListMetadata.listID = guid(0x13);
    c_report.cellsListMetadata.startingElementID = guid(0x41);
    router.on_report(c_report);
    if (router.active_lists() != 2 || !router.latest_stable(guid(0x13)) ||
        router.latest_stable(guid(0x13))->generation != 1 ||
        router.latest_stable(guid(0x12))->generation != 2)
      throw std::runtime_error("explicit list retirement failed to free capacity");

    // Invalid initial events must not consume a slot; unverified native QoS
    // cannot become verified merely because the router exists.
    b::LargeListRouter not_verified(Joiner::ElementOrdering::NotVerified, 1, 4, 1);
    rejected("router unverified DDS QoS", [&] {
      not_verified.on_element(a_element);
    });
    if (not_verified.active_lists() != 0)
      throw std::runtime_error("unverified event allocated a list");
    rejected("router memory budget", [&] {
      b::LargeListRouter invalid(Joiner::ElementOrdering::CallerVerifiedTopicOrdered,
                                 128, 4096, 4);
      (void)invalid;
    });

    router.reset_after_disconnect();
    if (router.active_lists() != 0 || router.latest_stable(guid(0x12)) ||
        router.latest_stable(guid(0x13)))
      throw std::runtime_error("disconnect retained multi-list snapshot");
    router.on_element(b_element);
    router.on_report(b_report);
    if (!router.latest_stable(guid(0x12)) ||
        router.latest_stable(guid(0x12))->generation != 1)
      throw std::runtime_error("multi-list reconstruction after disconnect failed");

    // Wrong QoS, foreign list, replayed element/Report and bad type values.
    Joiner unverified(Joiner::ElementOrdering::NotVerified);
    rejected("unverified element Topic QoS", [&]{ unverified.on_element(element(0x21,0,1)); });
    rejected("foreign list", [&]{
      auto bad = element(0x24, 0, 999); bad.listID = guid(0x90);
      j.on_element(bad);
    });
    rejected("element replay", [&]{ reverse.on_element(element(0x21, 0, 100)); });
    rejected("Report timestamp replay", [&]{ reverse.on_report(report(1, 0x21, 100)); });
    rejected("disposed element reused", [&]{ j.on_element(element(0x22, 0, 999)); });
    rejected("oversize Report", [&]{
      auto limited = ready(1);
      limited.on_report(report(2, 0x22, 200));
    });
    rejected("invalid nanosecond", [&]{
      auto bad = element(0x90, 0, 1000000000);
      auto probe = ready(); probe.on_element(bad);
    });

    // A fully observed checkpoint with a cycle must never be committed.
    rejected("linked-list cycle", [&]{
      auto cycle = ready();
      cycle.on_element(element(0x21, 0x22, 100));
      cycle.on_element(element(0x22, 0x21, 200));
      cycle.on_report(report(2, 0x22, 200));
    });

    // No stale state may cross a DDS disconnect/rejoin boundary.
    reverse.reset_after_disconnect();
    assert(!reverse.latest_stable());
    assert(reverse.buffered_elements() == 0);
    assert(reverse.pending_reports() == 0);
    reverse.on_element(element(0x21, 0, 300));
    reverse.on_report(report(1, 0x21, 300, 8));
    confirmed(reverse, 1, 2);

    std::puts("UMAA_BATTERY_LARGE_LIST_HOST_JOINER_PASS");
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "UMAA_BATTERY_LARGE_LIST_HOST_JOINER_FAIL: " << e.what() << '\n';
    return 3;
  }
}
