#include "umaa/command_engine.hpp"
#include <cassert>
#include <string>
#include <stdexcept>
#include <vector>

struct P { int rpm{}; };
struct H : umaa::Handler<P> {
  int run_count{};
  bool allow_cancel{true};
  bool throw_on_validate{false};
  bool throw_on_cancel{false};
  bool validate(const P& p) override { if (throw_on_validate) throw std::runtime_error("validation callback"); return p.rpm >= 0; }
  umaa::Outcome execute(const P&) override { ++run_count; return umaa::Outcome::Pending; }
  bool cancel(const P&) override { if (throw_on_cancel) throw std::runtime_error("cancellation callback"); return allow_cancel; }
};
static std::string states(const std::vector<umaa::Event>& events) {
  std::string out;
  for (const auto& e : events) if (e.kind == umaa::Event::Kind::Status) {
    if (!out.empty()) out += ",";
    out += e.detail;
  }
  return out;
}
int main() {
  H h; umaa::CommandEngine<P> e("p1", h);
  auto start = e.receive({"c1", "p1", "s1", 100}, {1000});
  assert(states(start) == "ISSUED,COMMANDED,EXECUTING");
  assert(e.size() == 1 && h.run_count == 1);
  auto replay = e.receive({"c1", "p1", "s1", 100}, {2000});
  assert(replay[0].detail == "STALE_OR_REPLAY" && h.run_count == 1);
  auto conflict = e.receive({"c2", "p1", "s1", 101}, {2000});
  assert(conflict[0].detail == "SOURCE_CONFLICT");
  h.allow_cancel = false;
  auto rejected = e.receive({"c1", "p1", "s1", 101}, {1500});
  assert(rejected[0].detail == "UPDATE_NOT_SAFE");
  h.allow_cancel = true;
  auto updated = e.receive({"c1", "p1", "s1", 101}, {1500});
  assert(states(updated) == "ISSUED,COMMANDED,EXECUTING");
  assert(updated[0].reason == "UPDATED");
  auto done = e.complete("s1", true);
  assert(states(done) == "COMPLETED");
  assert(e.contains("s1")); // No premature status dispose.
  auto terminal = e.receive({"c1", "p1", "s1", 102}, {1500});
  assert(terminal[0].detail == "TERMINAL_UPDATE");
  auto dispose = e.command_disposed("c1", "s1");
  assert(dispose.size() == 2 && dispose[0].kind == umaa::Event::Kind::DisposeStatus);
  assert(e.size() == 0);
  auto initial_bad = e.receive({"c1", "p1", "s2", 1}, {-1});
  assert(states(initial_bad) == "ISSUED,FAILED");
  assert(e.contains("s2"));
  auto cleanup = e.command_disposed("c1", "s2");
  assert(cleanup.size() == 2);
  e.receive({"c1", "p1", "s3", 1}, {200});
  auto recover = e.fail_unrecoverable();
  assert(states(recover) == "FAILED");
  assert(recover[0].reason == "SERVICE_FAILED");
  e.command_disposed("c1", "s3");
  e.receive({"c1", "p1", "s4", 1}, {300});
  auto cancelled = e.command_disposed("c1", "s4");
  assert(states(cancelled) == "CANCELED" && cancelled.size() == 3);
  e.receive({"c1", "p1", "s5", 1}, {300});
  h.allow_cancel = false;
  auto noncancellable = e.command_disposed("c1", "s5");
  assert(noncancellable.at(0).detail == "CANCEL_NOT_SAFE");
  assert(e.contains("s5"));
  auto finally_complete = e.complete("s5", true);
  assert(states(finally_complete) == "COMPLETED" && finally_complete.size()==3);
  assert(finally_complete.at(1).kind == umaa::Event::Kind::DisposeStatus);
  assert(!e.contains("s5"));
  h.throw_on_validate = true;
  auto invalid_exception = e.receive({"c1", "p1", "s6", 1}, {100});
  assert(states(invalid_exception) == "ISSUED,FAILED");
  assert(invalid_exception.back().reason == "SERVICE_FAILED");
  e.command_disposed("c1", "s6");
  h.throw_on_validate = false;
  e.receive({"c1", "p1", "s7", 1}, {100});
  h.throw_on_cancel = true;
  auto refused = e.command_disposed("c1", "s7");
  assert(refused.size() == 1 && refused.at(0).detail == "CANCEL_NOT_SAFE");
  assert(e.contains("s7"));
  auto completed = e.complete("s7", true);
  assert(states(completed) == "COMPLETED" && completed.size() == 3);
  h.throw_on_cancel = false;
}
