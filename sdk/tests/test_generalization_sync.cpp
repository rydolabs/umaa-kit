#include "umaa/generalization_sync.hpp"
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

#define VERIFY(...) do { if (!(__VA_ARGS__)) { \
  throw std::runtime_error("generalization verification failed at line " + std::to_string(__LINE__)); \
} } while (false)
int main() {
  VERIFY(rejects([] { Sync bad("", 2); }));
  VERIFY(rejects([] { Sync bad("TopicA", 0); }));
  Sync receiver("SpecialTopic", 2);
  VERIFY(rejects([&] { receiver.accept_general("Unknown", "a", 1, {1}); }));
  VERIFY(rejects([&] { receiver.accept_specialization("", 1, {1}); }));
  // Different arrival orders must yield the same complete pair.
  VERIFY(!receiver.accept_specialization("a", 1, {12}));
  VERIFY(!receiver.latest("a"));
  VERIFY(receiver.accept_general("SpecialTopic", "a", 1, {3}));
  VERIFY(receiver.latest("a")->general.status == 3);
  VERIFY(receiver.latest("a")->specialization.speed == 12);
  // A one-sided update must not make inconsistent data visible.
  VERIFY(!receiver.accept_general("SpecialTopic", "a", 2, {4}));
  VERIFY(receiver.latest("a")->specialization_timestamp == 1);
  VERIFY(!receiver.accept_specialization("a", 0, {0}));
  VERIFY(receiver.accept_specialization("a", 2, {15}));
  VERIFY(receiver.latest("a")->specialization_timestamp == 2);
  VERIFY(receiver.latest("a")->general.status == 4);
  VERIFY(!receiver.accept_general("SpecialTopic", "a", 2, {100}));
  VERIFY(receiver.latest("a")->general.status == 4);
  // Independent IDs cannot be mixed or overflow the bounded receiver.
  VERIFY(!receiver.accept_general("SpecialTopic", "b", 4, {7}));
  VERIFY(receiver.accept_specialization("b", 4, {19}));
  VERIFY(receiver.latest("b")->specialization.speed == 19);
  VERIFY(rejects([&] { receiver.accept_specialization("c", 5, {20}); }));
  VERIFY(receiver.instance_count() == 2);
  // Both DDS topics must Dispose before a new relationship can be accepted.
  VERIFY(!receiver.dispose_general("a"));
  VERIFY(!receiver.latest("a"));
  VERIFY(receiver.removal_pending("a"));
  VERIFY(rejects([&] { receiver.accept_specialization("a", 3, {30}); }));
  VERIFY(receiver.dispose_specialization("a"));
  VERIFY(!receiver.removal_pending("a"));
  // Old, delayed DDS events cannot resurrect the removed relationship.
  VERIFY(!receiver.accept_general("SpecialTopic", "a", 2, {8}));
  VERIFY(!receiver.accept_specialization("a", 1, {8}));
  VERIFY(!receiver.latest("a"));
  VERIFY(!receiver.accept_general("SpecialTopic", "a", 3, {9}));
  VERIFY(receiver.accept_specialization("a", 3, {31}));
  VERIFY(receiver.latest("a")->general.status == 9);
  VERIFY(receiver.latest("b")->specialization.speed == 19);
  // Removing opposite order has same invalidation and two-topic barrier.
  VERIFY(!receiver.dispose_specialization("b"));
  VERIFY(!receiver.latest("b"));
  VERIFY(receiver.dispose_general("b"));
  // Explicit resync may reclaim bounded tombstone slots.
  receiver.clear_after_resync("b");
  VERIFY(receiver.instance_count() == 1);
  return 0;
}
