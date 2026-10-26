// Mock DDS boundary tests: verify binding semantics, NOT on-wire conformance.
#include "umaa/cyclone_command_transport.hpp"
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

struct Payload { int value; };
struct Sample { int key; int value; };
struct Mapper {
  Sample make_status(const umaa::Identity& id, const umaa::Event& ev) const {
    return {std::stoi(id.session), ev.detail == "EXECUTING" ? 2 : 1};
  }
  Sample make_ack(const umaa::Identity& id, const Payload& p) const {
    return {std::stoi(id.session), p.value};
  }
};
struct Device final : umaa::Handler<Payload> {
  umaa::Outcome execute(const Payload&) override { return umaa::Outcome::Pending; }
  bool cancel(const Payload&) override { return true; }
};
struct WireEvent { bool dispose; int writer; Sample value; };
std::vector<WireEvent> observations;
int fail_writer = 0;
int fail_dispose_writer = 0;
extern "C" dds_return_t dds_write(dds_entity_t writer, const void* value) {
  if (writer == fail_writer) return -1;
  observations.push_back({false, writer, *static_cast<const Sample*>(value)});
  return 0;
}
extern "C" dds_return_t dds_dispose(dds_entity_t writer, const void* value) {
  if (writer == fail_dispose_writer) return -1;
  observations.push_back({true, writer, *static_cast<const Sample*>(value)});
  return 0;
}
extern "C" const char* dds_strretcode(int32_t) { return "mock failure"; }

template<class Fn> void expect_throw(Fn f) {
  bool threw = false;
  try { f(); } catch (const std::exception&) { threw = true; }
  assert(threw);
}
int main() {
  using Writer = umaa::cyclone::CommandWriter<Payload, Sample, Sample, Mapper>;
  expect_throw([] { Writer invalid(0, 3, Mapper{}); });
  Device device;
  Writer wire(3, 4, Mapper{});
  umaa::BoundCommandProvider<Payload> bound("host", device, wire);
  bound.receive({"control-A", "host", "11", 1}, {100});
  bound.receive({"control-B", "host", "22", 2}, {200});
  assert(wire.published_status_instances() == 2);
  bound.receive({"control-A", "host", "11", 3}, {150});
  bound.complete("11", true);
  bound.command_disposed("control-B", "22");
  bound.command_disposed("control-A", "11");
  assert(wire.published_status_instances() == 0);
  assert(observations.size() == 18); // 4+4+4+1+3+2
  // The two independent status disposes carry their own keyed instance.
  int disposed_11 = 0, disposed_22 = 0, ack_11 = 0;
  for (const auto& ev : observations) {
    if (!ev.dispose) continue;
    if (ev.writer == 3 && ev.value.key == 11) ++disposed_11;
    if (ev.writer == 3 && ev.value.key == 22) ++disposed_22;
    if (ev.writer == 4 && ev.value.key == 11 && ev.value.value == 150) ++ack_11;
  }
  assert(disposed_11 == 1 && disposed_22 == 1 && ack_11 == 1);

  // A negative dds_write result is an unknown delivery outcome, not a retry.
  fail_writer = 4;
  Writer broken(3, 4, Mapper{});
  umaa::BoundCommandProvider<Payload> rejected("host", device, broken);
  const auto before = observations.size();
  expect_throw([&] { rejected.receive({"c", "host", "33", 1}, {1}); });
  assert(rejected.faulted());
  expect_throw([&] { rejected.receive({"c", "host", "34", 2}, {2}); });
  assert(observations.size() == before + 1); // status issued, then failed ack
  fail_writer = 0;
  // Disposing an unpublished instance must be rejected without a DDS call.
  expect_throw([&] { broken.dispose_status({"c", "host", "notfound", 3}); });

  // The adapter must preserve the status key on failed DDS Dispose and
  // cause the owning provider to refuse any further protocol operations.
  for (const int fault_writer : {3, 4}) {
    Writer failing(3, 4, Mapper{});
    umaa::BoundCommandProvider<Payload> host("host", device, failing);
    host.receive({"c", "host", "44", 4}, {17});
    fail_dispose_writer = fault_writer;
    const auto n = observations.size();
    expect_throw([&] { host.command_disposed("c", "44"); });
    assert(host.faulted());
    // A failed Status Dispose retains its sample; a successful Status
    // Dispose followed by a failed Ack Dispose may erase it.
    assert(failing.published_status_instances() == (fault_writer == 3 ? 1u : 0u));
    fail_dispose_writer = 0;
    expect_throw([&] { host.command_disposed("c", "44"); });
    assert(observations.size() == n + (fault_writer == 3 ? 1u : 2u));
  }
}
