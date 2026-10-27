#include "umaa/command_deadline.hpp"
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>
using Clock=std::chrono::steady_clock;
using Tick=std::chrono::milliseconds;
struct P { int action{}; };
struct Device : umaa::Handler<P> {
  bool cancel_ok{true};
  umaa::Outcome execute(const P&) override { return umaa::Outcome::Pending; }
  bool cancel(const P&) override { return cancel_ok; }
};
struct Sink : umaa::CommandTransport<P> {
  std::vector<std::string> output;
  bool fail{false};
  void write(const std::string& s) {
    output.push_back(s);
    if (fail) throw std::runtime_error("uncertain DDS delivery");
  }
  void publish_status(const umaa::Identity& id,const umaa::Event& e) override {
    write(id.session+":"+e.detail+":"+e.reason);
  }
  void publish_ack(const umaa::Identity& id,const P&) override { write(id.session+":ACK"); }
  void dispose_status(const umaa::Identity& id) override { write(id.session+":DS"); }
  void dispose_ack(const umaa::Identity& id,const P&) override { write(id.session+":DA"); }
};
static std::string timeout_reason(const std::vector<umaa::Event>& events) {
  for (const auto& e:events)
    if (e.kind==umaa::Event::Kind::Status && e.detail=="FAILED") return e.reason;
  return "";
}
int main() {
  Device device; Sink sink;
  umaa::TimedCommandProvider<P> host("target",device,sink,Tick(100));
  const Clock::time_point t0{};
  auto id=[](const char* source,const char* session,int ts) {
    return umaa::Identity{source,"target",session,ts};
  };
  host.receive(id("one","s",1),{1},t0);
  assert(host.expire(t0+Tick(99)).empty());
  const auto replay=host.receive(id("one","s",1),{2},t0+Tick(90));
  assert(replay.at(0).detail=="STALE_OR_REPLAY");
  const auto wrong=host.receive(id("other","s",2),{2},t0+Tick(95));
  assert(wrong.at(0).detail=="SOURCE_CONFLICT");
  assert(timeout_reason(host.expire(t0+Tick(100)))=="TIMEOUT");
  assert(host.armed_deadlines()==0 && host.sessions()==1);
  const auto n=sink.output.size();
  const auto late=host.receive(id("one","s",3),{2},t0+Tick(101));
  assert(late.at(0).detail=="TERMINAL_UPDATE" && sink.output.size()==n);
  host.command_disposed("one","s");
  assert(host.sessions()==0 && sink.output.at(n)=="s:DS");
  host.receive(id("one","update",1),{1},t0);
  host.receive(id("one","update",2),{2},t0+Tick(50));
  assert(host.expire(t0+Tick(100)).empty());
  device.cancel_ok=false;
  auto refused=host.expire(t0+Tick(151));
  assert(refused.size()==1 && refused[0].detail=="TIMEOUT_CANCEL_NOT_SAFE");
  assert(host.armed_deadlines()==1);
  device.cancel_ok=true;
  assert(timeout_reason(host.expire(t0+Tick(152)))=="TIMEOUT");
  host.command_disposed("one","update");
  host.receive(id("a","c",1),{1},t0);
  host.receive(id("b","b",1),{1},t0+Tick(10));
  auto due=host.expire(t0+Tick(100));
  assert(due.size()==1 && due[0].session=="c");
  due=host.expire(t0+Tick(110));
  assert(due.size()==1 && due[0].session=="b");
  host.command_disposed("a","c");
  host.command_disposed("b","b");
  host.receive(id("one","done",1),{1},t0);
  host.complete("done",true);
  assert(host.armed_deadlines()==0 && host.expire(t0+Tick(500)).empty());
  host.command_disposed("one","done");
  host.receive(id("one","error",1),{1},t0);
  sink.fail=true;
  bool threw=false;
  try { (void)host.expire(t0+Tick(101)); }
  catch(const std::runtime_error&) { threw=true; }
  assert(threw && host.faulted());
  sink.fail=false; threw=false;
  try { (void)host.expire(t0+Tick(102)); }
  catch(const std::logic_error&) { threw=true; }
  assert(threw);
  threw=false;
  try { umaa::TimedCommandProvider<P> invalid("target",device,sink,Tick(0)); }
  catch(const std::invalid_argument&) { threw=true; }
  assert(threw);
}
