#include "umaa/command_transport.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

struct Payload { int value{}; };
struct Device : umaa::Handler<Payload> {
  bool can_cancel{true};
  bool fail_validation{false};
  bool throw_execute{false};
  int calls{};
  bool validate(const Payload& p) override {
    if (fail_validation) throw std::runtime_error("validation failed unexpectedly");
    return p.value >= 0;
  }
  umaa::Outcome execute(const Payload&) override {
    ++calls;
    if (throw_execute) throw std::runtime_error("device offline");
    return umaa::Outcome::Pending;
  }
  bool cancel(const Payload&) override { return can_cancel; }
};
struct Sink : umaa::CommandTransport<Payload> {
  std::vector<std::string> output;
  std::string fail_action;
  std::vector<int> ack_values;
  std::vector<int> disposed_ack_values;
  void write(const std::string& operation, const umaa::Identity& id) {
    assert(!id.source.empty() && !id.destination.empty() && !id.session.empty());
    output.push_back(operation + ":" + id.session + ":" + std::to_string(id.timestamp));
    if (fail_action == operation) throw std::runtime_error("DDS " + operation + " failed");
  }
  void publish_status(const umaa::Identity& id, const umaa::Event& event) override {
    write("status-" + event.detail, id);
  }
  void publish_ack(const umaa::Identity& id, const Payload& payload) override {
    assert(payload.value >= 0); write("ack", id);
    ack_values.push_back(payload.value);
  }
  void dispose_status(const umaa::Identity& id) override { write("dispose-status", id); }
  void dispose_ack(const umaa::Identity& id, const Payload& payload) override {
    assert(payload.value >= 0); write("dispose-ack", id);
    disposed_ack_values.push_back(payload.value);
  }
};
template<class Exception, class Fn> void throws(Fn f) {
  bool caught = false;
  try { f(); } catch (const Exception&) { caught = true; }
  assert(caught);
}
int main() {
  Device device;
  Sink sink;
  umaa::BoundCommandProvider<Payload> provider("host", device, sink);
  umaa::Identity first{"controller", "host", "s", 10};
  auto initial = provider.receive(first, {100});
  assert(initial.size() == 4 && sink.output.size() == 4);
  assert(sink.output[0] == "status-ISSUED:s:10");
  assert(sink.output[1] == "ack:s:10");
  assert(sink.output[2] == "status-COMMANDED:s:10");
  assert(sink.output[3] == "status-EXECUTING:s:10");
  assert(provider.sessions() == 1);
  auto ignored = provider.receive({"controller", "wrong", "s", 11}, {200});
  assert(ignored[0].kind == umaa::Event::Kind::Ignored && sink.output.size() == 4);
  ignored = provider.receive(first, {200});
  assert(ignored[0].detail == "STALE_OR_REPLAY" && sink.output.size() == 4);
  ignored = provider.receive({"intruder", "host", "s", 12}, {200});
  assert(ignored[0].detail == "SOURCE_CONFLICT" && sink.output.size() == 4);
  device.can_cancel = false;
  ignored = provider.receive({"controller", "host", "s", 12}, {200});
  assert(ignored[0].detail == "UPDATE_NOT_SAFE" && sink.output.size() == 4);
  device.can_cancel = true;
  auto updated = provider.receive({"controller", "host", "s", 12}, {200});
  assert(updated.size() == 4 && sink.output.size() == 8);
  assert(sink.output[4] == "status-ISSUED:s:12");
  assert(sink.output[5] == "ack:s:12");
  auto finished = provider.complete("s", true);
  assert(finished.size() == 1 && sink.output.back() == "status-COMPLETED:s:12");
  ignored = provider.receive({"controller", "host", "s", 13}, {200});
  assert(ignored[0].detail == "TERMINAL_UPDATE" && sink.output.size() == 9);
  ignored = provider.command_disposed("intruder", "s");
  assert(ignored[0].kind == umaa::Event::Kind::Ignored && sink.output.size() == 9);
  auto cleaned = provider.command_disposed("controller", "s");
  assert(cleaned.size() == 2 && provider.sessions() == 0);
  assert(sink.output[9] == "dispose-status:s:12");
  assert(sink.output[10] == "dispose-ack:s:12");
  assert((sink.ack_values == std::vector<int>{100, 200}));
  assert(sink.disposed_ack_values.back() == 200);
  // Cancellation refused: no premature dispose while a device is working.
  provider.receive({"controller", "host", "pending", 1}, {50});
  device.can_cancel = false;
  ignored = provider.command_disposed("controller", "pending");
  assert(ignored[0].detail == "CANCEL_NOT_SAFE");
  assert(provider.sessions() == 1);
  auto was = sink.output.size();
  provider.complete("pending", true);
  assert(provider.sessions() == 0 && sink.output.size() == was + 3);
  assert(sink.output.back() == "dispose-ack:pending:1");
  // A disposed but non-cancellable Command must never be reactivated
  // by a reordered durable update, even with a newer timestamp.
  provider.receive({"controller", "host", "disposing", 1}, {50});
  device.can_cancel = false;
  ignored = provider.command_disposed("controller", "disposing");
  assert(ignored[0].detail == "CANCEL_NOT_SAFE");
  was = sink.output.size();
  ignored = provider.receive({"controller", "host", "disposing", 2}, {90});
  assert(ignored[0].detail == "COMMAND_DISPOSED");
  assert(sink.output.size() == was && provider.sessions() == 1);
  // No fabricated terminal state while hardware cannot stop.
  ignored = provider.timeout("disposing");
  assert(ignored[0].detail == "TIMEOUT_CANCEL_NOT_SAFE");
  assert(sink.output.size() == was);
  device.can_cancel = true;
  auto timed_out = provider.timeout("disposing");
  assert(timed_out.size() == 3 && timed_out[0].reason == "TIMEOUT");
  assert(provider.sessions() == 0);
  assert(sink.output.back() == "dispose-ack:disposing:1");
  // A normal active deadline requires a terminal TIMEOUT status, followed
  // by explicit command dispose before provider responses may be disposed.
  provider.receive({"controller", "host", "deadline", 20}, {5});
  auto deadline = provider.timeout("deadline");
  assert(deadline.size() == 1 && deadline[0].reason == "TIMEOUT");
  assert(provider.sessions() == 1);
  provider.command_disposed("controller", "deadline");
  assert(provider.sessions() == 0);
  // Several sessions can interleave and cleanup uses the correct initiator.
  device.can_cancel = true;
  provider.receive({"a", "host", "a", 10}, {1});
  provider.receive({"b", "host", "b", 20}, {2});
  auto recovered = provider.fail_unrecoverable();
  assert(recovered.size() == 2);
  assert(provider.sessions() == 2);
  provider.command_disposed("b", "b");
  provider.command_disposed("a", "a");
  assert(provider.sessions() == 0 && !provider.faulted());
  // User exception is normalized by engine into FAILED and emitted on DDS.
  device.throw_execute = true;
  auto error = provider.receive({"controller", "host", "exception", 3}, {1});
  assert(error.back().reason == "SERVICE_FAILED");
  assert(sink.output.back() == "status-FAILED:exception:3");
  provider.command_disposed("controller", "exception");

  // Every wire operation is potentially non-idempotent and must fail closed.
  for (const auto& action : {"status-ISSUED", "ack", "status-COMMANDED",
                             "status-EXECUTING", "status-COMPLETED",
                             "dispose-status", "dispose-ack"}) {
    Device d;
    Sink output;
    umaa::BoundCommandProvider<Payload> bound("host", d, output);
    if (std::string(action) != "status-COMPLETED" &&
        std::string(action) != "dispose-status" &&
        std::string(action) != "dispose-ack") output.fail_action = action;
    if (!output.fail_action.empty()) {
      throws<std::runtime_error>([&] { bound.receive({"controller", "host", "x", 5}, {1}); });
    } else {
      bound.receive({"controller", "host", "x", 5}, {1});
      output.fail_action = action;
      if (std::string(action) == "status-COMPLETED") {
        throws<std::runtime_error>([&] { bound.complete("x", true); });
      } else {
        bound.complete("x", true);
        throws<std::runtime_error>([&] { bound.command_disposed("controller", "x"); });
      }
    }
    assert(bound.faulted());
    const auto count = output.output.size();
    throws<std::logic_error>([&] { bound.receive({"controller", "host", "next", 6}, {1}); });
    throws<std::logic_error>([&] { bound.fail_unrecoverable(); });
    assert(output.output.size() == count);
  }
}
