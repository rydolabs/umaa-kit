#include "umaa/generalization_sync.hpp"
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {
struct General { int status; };
struct Special { int speed; };
using Sync = umaa::GeneralizationSynchronizer<General, Special>;
template<class Fn> bool rejects(Fn f) {
  try { f(); } catch (const std::exception&) { return true; }
  return false;
}
}

int main() {
  assert(rejects([] { Sync bad("", 2); }));
  assert(rejects([] { Sync bad("TopicA", 0); }));
  Sync receiver("SpecialTopic", 2);
  assert(rejects([&] { receiver.accept_general("Unknown", "a", 1, {1}); }));
  assert(rejects([&] { receiver.accept_specialization("", 1, {1}); }));
  // Different arrival orders must yield the same complete pair.
  assert(!receiver.accept_specialization("a", 1, {12}));
  assert(!receiver.latest("a"));
  assert(receiver.accept_general("SpecialTopic", "a", 1, {3}));
  assert(receiver.latest("a")->general.status == 3);
  assert(receiver.latest("a")->specialization.speed == 12);
  // A one-sided update must not make inconsistent data visible.
  assert(!receiver.accept_general("SpecialTopic", "a", 2, {4}));
  assert(receiver.latest("a")->specialization_timestamp == 1);
  assert(!receiver.accept_specialization("a", 0, {0}));
  assert(receiver.accept_specialization("a", 2, {15}));
  assert(receiver.latest("a")->specialization_timestamp == 2);
  assert(receiver.latest("a")->general.status == 4);
  assert(!receiver.accept_general("SpecialTopic", "a", 2, {100}));
  assert(receiver.latest("a")->general.status == 4);
  // Independent IDs cannot be mixed or overflow the bounded receiver.
  assert(!receiver.accept_general("SpecialTopic", "b", 4, {7}));
  assert(receiver.accept_specialization("b", 4, {19}));
  assert(receiver.latest("b")->specialization.speed == 19);
  assert(rejects([&] { receiver.accept_specialization("c", 5, {20}); }));
  assert(receiver.instance_count() == 2);
  // Both DDS topics must Dispose before a new relationship can be accepted.
  assert(!receiver.dispose_general("a"));
  assert(!receiver.latest("a"));
  assert(receiver.removal_pending("a"));
  assert(rejects([&] { receiver.accept_specialization("a", 3, {30}); }));
  assert(receiver.dispose_specialization("a"));
  assert(!receiver.removal_pending("a"));
  // Old, delayed DDS events cannot resurrect the removed relationship.
  assert(!receiver.accept_general("SpecialTopic", "a", 2, {8}));
  assert(!receiver.accept_specialization("a", 1, {8}));
  assert(!receiver.latest("a"));
  assert(!receiver.accept_general("SpecialTopic", "a", 3, {9}));
  assert(receiver.accept_specialization("a", 3, {31}));
  assert(receiver.latest("a")->general.status == 9);
  assert(receiver.latest("b")->specialization.speed == 19);
  // Removing opposite order has same invalidation and two-topic barrier.
  assert(!receiver.dispose_specialization("b"));
  assert(!receiver.latest("b"));
  assert(receiver.dispose_general("b"));
  // Explicit resync may reclaim bounded tombstone slots.
  receiver.clear_after_resync("b");
  assert(receiver.instance_count() == 1);
  return 0;
}
