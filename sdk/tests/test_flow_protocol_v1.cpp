#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/sdk_v1.hpp"
#include "umaa/command_transport.hpp"
#include "umaa/command_deadline.hpp"
#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>

using namespace umaa;
namespace api=umaa::sdk_v1;
struct P{int value;};
struct ReportSvc{};
struct CommandSvc{};
struct QueryDataSvc{};
namespace umaa::sdk_v1 {
template<> struct ServiceTraits<ReportSvc> {
  using Report=int;
  inline static constexpr ServiceDescriptor descriptor{
    "synthetic:status","synthetic:report",FlowKind::QuerylessRequestReply,ServiceKind::Status};
};
template<> struct ServiceTraits<CommandSvc> {
  using Command=P;
  inline static constexpr ServiceDescriptor descriptor{
    "synthetic:control","synthetic:command",FlowKind::CommandResponse,ServiceKind::Control};
};
template<> struct ServiceTraits<QueryDataSvc> {
  using Report=int;
  inline static constexpr ServiceDescriptor descriptor{
    "synthetic:query","synthetic:query-v1",FlowKind::QueryDataRequestReply,ServiceKind::Report};
};
}
struct Device:Handler<P> {
  bool allow_cancel{true};
  unsigned executions{0},cancellations{0};
  Outcome execute(const P&) override {++executions;return Outcome::Pending;}
  bool cancel(const P&) override {++cancellations;return allow_cancel;}
};
struct Sink:CommandTransport<P> {
  std::vector<std::string> log;
  bool fail_next{false};
  void push(const std::string& value){
    if(fail_next){fail_next=false;throw std::runtime_error("ambiguous writer delivery");}
    log.push_back(value);
  }
  void publish_status(const Identity& id,const Event& e) override {
    push(id.session+":"+e.detail+":"+e.reason);
  }
  void publish_ack(const Identity& id,const P&) override{push(id.session+":ACK");}
  void before_dispose(const Identity& id) override{push(id.session+":BARRIER");}
  void dispose_status(const Identity& id) override{push(id.session+":DS");}
  void dispose_ack(const Identity& id,const P&) override{push(id.session+":DA");}
};
struct Sensor:umaa::ReportHandler<int>{
  int value{5};
  int read_report() const override{return value;}
};
static std::size_t count_kind(const std::vector<Event>& es,Event::Kind kind){
  std::size_t n=0;for(const auto& e:es)if(e.kind==kind)++n;return n;
}
static bool has_status(const std::vector<Event>& es,const std::string& detail){
  for(const auto& e:es)if(e.kind==Event::Kind::Status&&e.detail==detail)return true;
  return false;
}
static bool throws_logic(const std::function<void()>& fn){
  try{fn();}catch(const std::logic_error&){return true;}return false;
}

int main(){
  // EO ICD Figure 23: reasons and transitions are validated as one contract.
  assert(command_transition_allowed(std::nullopt,State::ISSUED,"SUCCEEDED"));
  assert(!command_transition_allowed(std::nullopt,State::COMMANDED,"SUCCEEDED"));
  assert(!command_transition_allowed(State::ISSUED,State::EXECUTING,"SUCCEEDED"));
  assert(command_transition_allowed(State::ISSUED,State::FAILED,"VALIDATION_FAILED"));
  assert(!command_transition_allowed(State::ISSUED,State::FAILED,"OBJECTIVE_FAILED"));
  assert(command_transition_allowed(State::COMMANDED,State::FAILED,"RESOURCE_REJECTED"));
  assert(!command_transition_allowed(State::COMMANDED,State::FAILED,"OBJECTIVE_FAILED"));
  assert(command_transition_allowed(State::EXECUTING,State::FAILED,"OBJECTIVE_FAILED"));
  assert(command_transition_allowed(State::EXECUTING,State::ISSUED,"UPDATED"));
  assert(!command_transition_allowed(State::EXECUTING,State::ISSUED,"SUCCEEDED"));
  assert(command_transition_allowed(State::ISSUED,State::CANCELED,"CANCELED"));
  assert(!command_transition_allowed(State::COMPLETED,State::CANCELED,"CANCELED"));

  Device device;Sink sink;
  BoundCommandProvider<P> wired("target",device,sink);
  wired.receive({"alpha","target","session-a",1},{1});
  wired.receive({"alpha","target","session-b",2},{2});
  wired.receive({"beta","target","session-c",3},{3});
  assert(wired.sessions()==3);
  auto lost=wired.consumer_liveliness_lost("alpha");
  assert(count_kind(lost,Event::Kind::Status)==2);
  assert(count_kind(lost,Event::Kind::DisposeStatus)==2);
  assert(count_kind(lost,Event::Kind::DisposeAck)==2);
  assert(has_status(lost,"CANCELED")&&wired.sessions()==1);
  assert(wired.consumer_liveliness_lost("alpha").empty());
  assert(device.cancellations==2);
  assert(sink.log.back()=="session-b:DA");
  // Source loss is isolated; unrelated consumer's session stays active.
  assert(has_status(wired.complete("session-c",true),"COMPLETED"));
  assert(wired.sessions()==1);
  wired.command_disposed("beta","session-c");
  assert(wired.sessions()==0);

  // A source loss remains fenced: a stale/new DDS Command from the
  // same Consumer is not proof that its liveliness has recovered.
  auto blocked=wired.receive({"alpha","target","blocked",9},{9});
  assert(blocked.size()==1&&blocked[0].detail=="CONSUMER_NOT_ALIVE");
  assert(wired.sessions()==0);
  wired.consumer_liveliness_restored("alpha");
  // Hardware cancellation refusal must neither emit CANCELED nor erase
  // the session. A later confirmed cancel can complete it safely.
  wired.receive({"alpha","target","retry",10},{10});
  device.allow_cancel=false;
  lost=wired.consumer_liveliness_lost("alpha");
  assert(lost.size()==1&&lost[0].detail=="LIVELINESS_CANCEL_NOT_SAFE");
  assert(!has_status(lost,"CANCELED")&&wired.sessions()==1);
  device.allow_cancel=true;
  // An untrusted incoming sample cannot restart a stranded device session.
  const auto late_new=wired.receive({"alpha","target","blocked2",11},{11});
  assert(late_new.size()==1&&late_new[0].detail=="CONSUMER_NOT_ALIVE");
  device.allow_cancel=true;
  lost=wired.consumer_liveliness_lost("alpha");
  assert(has_status(lost,"CANCELED")&&wired.sessions()==0);

  // Even if cancellation was unsafe at loss time, a genuine device
  // completion is terminal and allows cleanup without the absent Consumer.
  wired.consumer_liveliness_restored("alpha");
  wired.receive({"alpha","target","late-completion",12},{12});
  device.allow_cancel=false;
  auto still_active=wired.consumer_liveliness_lost("alpha");
  assert(still_active.size()==1 && wired.sessions()==1);
  auto genuinely_done=wired.complete("late-completion",true);
  assert(has_status(genuinely_done,"COMPLETED"));
  assert(count_kind(genuinely_done,Event::Kind::DisposeAck)==1);
  assert(wired.sessions()==0);
  device.allow_cancel=true;

  // Unknown DDS status delivery permanently faults a transport. Never retry
  // a potentially side-effecting Status/Ack/Dispose sequence automatically.
  Sink bad_sink;BoundCommandProvider<P> bad("target",device,bad_sink);
  bad.receive({"alpha","target","fault",20},{20});
  bad_sink.fail_next=true;
  bool threw=false;
  try{(void)bad.consumer_liveliness_lost("alpha");}
  catch(const std::runtime_error&){threw=true;}
  assert(threw&&bad.faulted());
  assert(throws_logic([&]{(void)bad.consumer_liveliness_lost("alpha");}));

  // Reason-aware Consumer observes Status and ACK independently.
  RemoteCommandMonitor mon;
  mon.issue({"alpha","target","reason",100});
  assert(mon.status("reason","target",State::ISSUED,"SUCCEEDED",101));
  assert(mon.status("reason","target",State::COMMANDED,"SUCCEEDED",102));
  assert(!mon.status("reason","target",State::EXECUTING,"RESOURCE_REJECTED",103));
  assert(mon.needs_recovery("reason"));
  assert(mon.result("reason")==RemoteCommandResult::Pending);
  RemoteCommandMonitor good;
  good.issue({"alpha","target","good",100});
  assert(good.status("good","target",State::ISSUED,"SUCCEEDED",101));
  assert(good.status("good","target",State::COMMANDED,"SUCCEEDED",102));
  assert(good.status("good","target",State::EXECUTING,"SUCCEEDED",103));
  assert(good.ack("good","target",104));
  assert(good.ready_for_update("good"));
  good.begin_update("good");
  assert(good.status("good","target",State::ISSUED,"UPDATED",105));
  assert(good.ack("good","target",106));
  assert(good.status("good","target",State::COMMANDED,"SUCCEEDED",107));
  assert(good.status("good","target",State::EXECUTING,"SUCCEEDED",108));
  assert(good.status("good","target",State::COMPLETED,"SUCCEEDED",109));
  assert(good.result("good")==RemoteCommandResult::Completed);
  good.issue({"alpha","target","missing",111});
  good.issue({"alpha","different-provider","unrelated",112});
  auto cancel=good.provider_liveliness_lost("target");
  assert(cancel.size()==1&&cancel.front()=="missing");
  assert(good.needs_recovery("missing"));
  assert(good.result("missing")==RemoteCommandResult::Pending);
  assert(good.provider_liveliness_lost("target").empty());
  assert(!good.needs_recovery("unrelated"));
  assert(good.result("good")==RemoteCommandResult::Completed);
  assert(good.disposed_status("missing","target"));
  assert(good.disposed_ack("missing","target"));
  assert(good.release("missing"));

  // A monotonic deadline must be cleared only when liveliness cancellation
  // actually retired the session, not when a device refused.
  Sink timed_sink;Device timed_device;
  TimedCommandProvider<P> timed("target",timed_device,timed_sink,
    std::chrono::milliseconds(50));
  using Clock=std::chrono::steady_clock;
  timed.receive({"alpha","target","due",1},{1},Clock::time_point{});
  assert(timed.armed_deadlines()==1);
  timed_device.allow_cancel=false;
  auto unresolved=timed.consumer_liveliness_lost("alpha");
  assert(unresolved.size()==1 && timed.armed_deadlines()==1);
  timed_device.allow_cancel=true;
  auto resolved=timed.consumer_liveliness_lost("alpha");
  assert(has_status(resolved,"CANCELED") && timed.armed_deadlines()==0);
  assert(timed.expire(Clock::time_point{}+std::chrono::seconds(1)).empty());

  // The typed API does not accept QueryData as a fabricated Queryless flow.
  Sensor sensor;
  api::ReportProvider<ReportSvc> rp("sensor",sensor);
  api::ReportConsumer<ReportSvc> rc;
  auto epoch=rc.start();
  auto first=rp.start(10);
  assert(rc.receive(*first.back().report,epoch));
  assert(rc.latest("sensor")->data==5);
  auto final=rp.stop(11);
  assert(rc.dispose("sensor",*final.front().timestamp,epoch));
  assert(!rc.latest("sensor"));
  rc.stop();
  bool rejected=false;
  try{api::ReportConsumer<QueryDataSvc> invalid;(void)invalid;}
  catch(const std::invalid_argument&){rejected=true;}
  assert(rejected);

  // 128-bit GUID adapter: keep separate sessions sharing their lower bits.
  auto prov=api::Guid128::parse("00000000-0000-0000-0000-000000000001");
  auto src=api::Guid128::parse("00000000-0000-0000-0000-000000000002");
  auto sess1=api::Guid128::parse("00000000-0000-0001-0000-000000000003");
  auto sess2=api::Guid128::parse("f0000000-0000-0001-0000-000000000003");
  api::Identifier provider{prov,{}},consumer{src,{}};
  api::CommandProvider<CommandSvc> typed(provider,device);
  api::CommandConsumer<CommandSvc> observer;
  api::CommandIdentity id1{consumer,provider,sess1,1000};
  api::CommandIdentity id2{consumer,provider,sess2,1001};
  observer.issue(id1);observer.issue(id2);
  typed.receive(id1,P{1});typed.receive(id2,P{2});
  assert(typed.active_sessions()==2);
  auto actions=observer.provider_liveliness_lost(provider);
  assert(actions.size()==2 && actions[0]!=actions[1]);
  assert(observer.needs_recovery(sess1)&&observer.needs_recovery(sess2));
  auto terminal=typed.consumer_liveliness_lost(consumer);
  assert(count_kind(terminal,Event::Kind::DisposeStatus)==2);
  assert(typed.active_sessions()==0);
}
