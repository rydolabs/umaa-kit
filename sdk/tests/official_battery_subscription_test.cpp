// Standalone C++17 host-only SDK BatteryStatus Subscription+Handler lifecycle
// tests. Original IDL is not bundled; genuine DDS E2E is checked separately.
#include "umaa/official_battery_subscription.hpp"

#include <cstdio>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace b = umaa::official_battery;

static b::Guid guid(unsigned char byte) {
  b::Guid g{}; g[0] = byte; return g;
}
static b::ListElement cell(unsigned char list, unsigned char id,
                           int timestamp, double voltage = 3.75) {
  b::ListElement v{};
  v.listID = guid(list);
  v.elementID = guid(id);
  v.elementTimestamp = {1700001000, timestamp};
  v.element = {-0.25, 25.0, voltage};
  return v;
}
static b::Report report(unsigned char source, unsigned char list,
                        unsigned char checkpoint, int timestamp,
                        int seq = 1) {
  b::Report v{};
  v.source.id = guid(source);
  v.source.parentID = guid(0x01);
  v.timeStamp = {1700000000 + seq, 0};
  v.state = b::State::ON;
  v.voltage = 24.5;
  v.cellsListMetadata.listID = guid(list);
  v.cellsListMetadata.size = 1;
  v.cellsListMetadata.startingElementID = guid(checkpoint);
  v.cellsListMetadata.updateElementID = guid(checkpoint);
  v.cellsListMetadata.updateElementTimestamp = b::DateTime{1700001000, timestamp};
  return v;
}
static void check(bool ok, const char* what) {
  if (!ok) throw std::runtime_error(what);
}
static void rejected(const char* what, const std::function<void()>& action) {
  try { action(); }
  catch (const std::exception&) { return; }
  throw std::runtime_error(std::string("missing expected rejection: ") + what);
}

struct Handler final : b::BatteryStatusHandler {
  struct Snapshot { b::Guid list; b::Guid source; std::uint64_t generation;
                    double voltage; b::ReaderEpoch epoch; };
  std::vector<Snapshot> accepted;
  std::vector<std::pair<b::Guid,b::ReaderEpoch>> invalidations;
  std::vector<std::pair<b::Guid,b::ReaderEpoch>> dispositions;
  bool fail_on_callback{false};

  void on_stable_list(const b::StableList& state,
                      b::ReaderEpoch epoch) override {
    if (fail_on_callback) throw std::runtime_error("simulated user Handler failure");
    check(!state.nativeDdsCoherenceProved, "SDK Handler fabricated DDS coherence");
    check(state.ordered.size() == 1, "unexpected Battery Cell count");
    accepted.push_back({state.report.cellsListMetadata.listID,
                        state.report.source.id, state.generation,
                        state.ordered[0].element.voltage, epoch});
  }
  void on_list_invalidated(const b::Guid& list, b::ReaderEpoch epoch) override {
    if (fail_on_callback) throw std::runtime_error("Handler invalidation failure");
    invalidations.emplace_back(list,epoch);
  }
  void on_source_disposed(const b::Identifier& source, const b::Guid&,
                          b::ReaderEpoch epoch) override {
    if (fail_on_callback) throw std::runtime_error("Handler dispose failure");
    dispositions.emplace_back(source.id,epoch);
  }
};

int main() {
  try {
    using S = b::BatteryStatusSubscription;
    const auto ordering = b::LargeListJoiner::ElementOrdering::CallerVerifiedTopicOrdered;
    Handler handler;
    S service(handler, ordering, 2, 4, 3);
    const auto e1 = service.start();
    check(e1 != 0 && service.state() == S::State::Active, "Reader not activated");
    auto a = report(0x33, 0x11, 0x21, 100);
    auto b_report = report(0x44, 0x12, 0x31, 200);
    auto a_cell = cell(0x11, 0x21, 100);
    auto b_cell = cell(0x12, 0x31, 200, 5.0);

    // Interleaved two different original Source/Collection keys, with
    // both Report-first and Element-first arrival.
    check(service.on_element(a_cell, e1), "initial Cell rejected");
    check(service.on_report(b_report, e1), "second source Report rejected");
    check(handler.accepted.empty() && !service.latest(guid(0x12)),
          "premature business Handler event");
    check(service.on_element(b_cell, e1), "second source Cell rejected");
    check(service.on_report(a, e1), "first source Report rejected");
    check(handler.accepted.size() == 2 &&
          handler.accepted[0].source == guid(0x44) &&
          handler.accepted[0].voltage == 5.0 &&
          handler.accepted[1].source == guid(0x33) &&
          handler.accepted[1].generation == 1 &&
          service.active_lists() == 2, "SDK lost interleaved List Handler callbacks");

    // An element update is not a complete Report checkpoint; never
    // continue returning a stale current snapshot to business logic.
    auto update = a_cell;
    update.elementTimestamp.nanoseconds = 101;
    update.element.voltage = 4.25;
    service.on_element(update, e1);
    check(!service.latest(guid(0x11)) &&
          service.latest(guid(0x12)) &&
          handler.invalidations.size() == 1 &&
          handler.invalidations[0].first == guid(0x11),
          "partial update improperly exposed previous stable snapshot");
    service.on_element(cell(0x12, 0x31, 201, 5.25), e1);
    check(!service.latest(guid(0x12)) &&
          handler.invalidations.size() == 2, "second source not independently invalidated");
    auto later_b = report(0x44, 0x12, 0x31, 201, 2);
    service.on_report(later_b, e1);
    check(handler.accepted.size() == 3 &&
          service.latest(guid(0x12)) &&
          !service.latest(guid(0x11)) &&
          handler.accepted.back().generation == 2,
          "second source new checkpoint incorrect");
    auto later_a = report(0x33, 0x11, 0x21, 101, 2);
    service.on_report(later_a, e1);
    check(handler.accepted.size() == 4 &&
          service.latest(guid(0x11)) &&
          service.latest(guid(0x12)) &&
          handler.accepted.back().voltage == 4.25 &&
          handler.accepted.back().generation == 2,
          "first source checkpoint did not recover");

    // A genuine keyed DDS Report Dispose must clear the source's list and
    // trigger distinct business invalidation. No late Element may resurrect.
    check(service.on_source_dispose(b_report.source, e1), "source Dispose ignored");
    check(!service.latest(guid(0x12)) && service.latest(guid(0x11)) &&
          service.active_lists() == 1 && handler.dispositions.size() == 1 &&
          handler.dispositions.back().first == guid(0x44),
          "Report Dispose did not retire exact source/list");
    check(!service.on_element_dispose(guid(0x12), guid(0x31), e1),
          "late disposed Cell incorrectly revived List");
    rejected("source replay after keyed Dispose", [&] {
      service.on_report(b_report, e1);
    });
    check(service.state() == S::State::Faulted &&
          service.active_lists() == 0 && !service.latest(guid(0x11)),
          "invalid Source replay did not fail closed");

    // Explicit fault reset creates a fresh Reader epoch. Old DDS callbacks
    // must NEVER change snapshots after rediscovery.
    service.reset_after_disconnect();
    const auto e2 = service.start();
    check(e2 > e1, "reader epoch was reused");
    check(!service.on_report(a, e1) && !service.on_element(a_cell,e1) &&
          !service.on_source_dispose(a.source, e1) &&
          handler.accepted.size() == 4,
          "stale DDS reader callback reached the business Handler");
    service.on_element(a_cell,e2);
    service.on_report(a,e2);
    check(handler.accepted.size() == 5 &&
          handler.accepted.back().epoch == e2 &&
          handler.accepted.back().generation == 1,
          "full fresh reader reconstruction failed");

    // Source ID collision and wrong Parent ID are untrusted input, not a
    // silently rebinding legitimate Source.
    auto collision = report(0x55, 0x11, 0x21, 100);
    rejected("conflicting Source uses active listID", [&] {
      service.on_report(collision,e2);
    });
    check(service.state() == S::State::Faulted &&
          service.active_lists() == 0, "conflict did not poison reader epoch");
    service.reset_after_disconnect();
    const auto e3 = service.start();
    service.on_report(a, e3);
    auto wrong_parent = a.source;
    wrong_parent.parentID = guid(0x10);
    rejected("DDS Dispose Parent key mismatch", [&] {
      service.on_source_dispose(wrong_parent, e3);
    });
    check(service.state() == S::State::Faulted, "wrong Dispose key accepted");

    // Handler exceptions must never leave a source valid or retry the same
    // publication automatically. Caller explicitly tears down reader.
    service.reset_after_disconnect();
    const auto e4 = service.start();
    handler.fail_on_callback = true;
    service.on_element(a_cell,e4);
    rejected("Handler exception", [&] { service.on_report(a,e4); });
    check(service.state() == S::State::Faulted &&
          service.active_lists() == 0 && !service.latest(guid(0x11)),
          "Handler failure did not poison subscriber");
    handler.fail_on_callback = false;
    service.reset_after_disconnect();
    const auto e5 = service.start();
    service.on_element(a_cell,e5);
    service.on_report(a,e5);
    check(handler.accepted.size() == 6 &&
          handler.accepted.back().epoch == e5,
          "no explicit recovery after Handler failure");
    service.stop();
    check(!service.on_element(a_cell,e5) && !service.latest(guid(0x11)) &&
          service.active_lists() == 0, "stopped Reader accepted callback");

    // Caller asserted QoS cannot grant acceptance while not verified.
    Handler denied_handler;
    S unverified(denied_handler,
       b::LargeListJoiner::ElementOrdering::NotVerified, 1, 2, 1);
    const auto denied_epoch = unverified.start();
    rejected("unverified Presentation", [&] {
      unverified.on_element(a_cell,denied_epoch);
    });
    check(unverified.state() == S::State::Faulted &&
          denied_handler.accepted.empty(),
          "unverified DDS QoS activated service");

    std::puts("UMAA_BATTERY_STATUS_SDK_HANDLER_SUBSCRIPTION_PASS");
    return 0;
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "Battery SDK subscription: %s\n", ex.what());
    return 3;
  }
}
