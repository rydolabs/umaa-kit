#include "umaa/sdk_v1.hpp"
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace umaa::sdk_v1;
struct CommandService {};
struct ReportService {};
struct CollectionService {};
namespace umaa::sdk_v1 {
template<> struct ServiceTraits<CommandService> {
  using Command=std::string;
  inline static constexpr ServiceDescriptor descriptor{
    "demo:engine-control","demo:command-v1",FlowKind::CommandResponse,ServiceKind::Control};
};
template<> struct ServiceTraits<ReportService> {
  using Report=int;
  inline static constexpr ServiceDescriptor descriptor{
    "demo:battery","demo:report-v1",FlowKind::QuerylessRequestReply,ServiceKind::Status};
};
template<> struct ServiceTraits<CollectionService> {
  using Element=int;
  inline static constexpr ServiceDescriptor descriptor{
    "demo:cells","demo:cells-v1",FlowKind::QuerylessRequestReply,
    ServiceKind::Status,CollectionShape::LargeList};
};
}
struct ReportSource final : umaa::ReportHandler<int> {
  int state{10};
  int read_report() const override{return state;}
};
struct CommandHandler final : umaa::Handler<std::string> {
  int called{},canceled{};
  umaa::Outcome execute(const std::string&) override {++called;return umaa::Outcome::Pending;}
  bool cancel(const std::string&) override {++canceled;return true;}
};

int main(){
  auto provider_guid=Guid128::parse("00000000-0000-0000-0000-000000000001");
  auto source_guid=Guid128::parse("00000000-0000-0000-0000-000000000002");
  auto session=Guid128::parse("00000000-0000-0001-0000-000000000003");
  Identifier provider{provider_guid,Guid128{}}, source{source_guid,Guid128{}};
  const CommandIdentity id{source,provider,session,100};
  assert(legacy_command_identity(id).session==session.str());
  assert(legacy_command_identity(id).destination==canonical_id(provider));
  CommandHandler handler;
  CommandProvider<CommandService> server(provider,handler);
  CommandConsumer<CommandService> client;
  client.issue(id);
  auto accepted=server.receive(id,"run");
  assert(!accepted.empty() && handler.called==1 && server.active_sessions()==1);
  assert(client.observe_status(session,provider,umaa::State::ISSUED,101));
  assert(client.observe_status(session,provider,umaa::State::COMMANDED,102));
  assert(client.observe_status(session,provider,umaa::State::EXECUTING,103));
  assert(client.observe_ack(session,provider,104));
  assert(client.ready_for_update(session));
  client.begin_update(session);
  auto updated=id;updated.timestamp_ns=200;
  auto updated_events=server.receive(updated,"update");
  assert(!updated_events.empty()&&handler.called==2&&handler.canceled==1);
  assert(client.observe_status(session,provider,umaa::State::ISSUED,201));
  assert(client.observe_status(session,provider,umaa::State::COMMANDED,202));
  assert(client.observe_status(session,provider,umaa::State::EXECUTING,203));
  assert(client.observe_ack(session,provider,204));
  auto complete=server.complete(session,true);
  assert(!complete.empty());
  assert(client.observe_status(session,provider,umaa::State::COMPLETED,205));
  assert(client.result(session)==umaa::RemoteCommandResult::Completed);
  const auto cleaned=server.command_disposed(source,session);
  assert(!cleaned.empty()&&server.active_sessions()==0);
  assert(client.disposed_status(session,provider));
  assert(client.disposed_ack(session,provider));
  assert(client.release(session));
  assert(handler.called==2);

  // Different session with same low 64 bits must still remain distinct.
  auto session2=Guid128::parse("10000000-0000-0001-0000-000000000003");
  assert(session2!=session);
  CommandIdentity id2{source,provider,session2,300};
  client.issue(id2);
  server.receive(id2,"cancel");
  const auto canceled=server.command_disposed(source,session2);
  bool seen_canceled=false;
  for(const auto& e:canceled)
    if(e.kind==umaa::Event::Kind::Status && e.detail=="CANCELED")seen_canceled=true;
  assert(seen_canceled && server.active_sessions()==0);

  ReportSource src;
  ReportProvider<ReportService> publisher("battery-1",src);
  ReportConsumer<ReportService> reader;
  const auto epoch=reader.start();
  auto initial=publisher.start(10);
  assert(initial.size()==2);
  assert(reader.receive(*initial.back().report,epoch));
  assert(reader.latest("battery-1")->data==10);
  src.state=15;
  auto latest=publisher.update(20);
  assert(reader.receive(*latest.report,epoch));
  assert(!reader.receive(*initial.back().report,epoch));
  assert(reader.latest("battery-1")->data==15);
  auto finish=publisher.stop(30);
  assert(finish.size()==2);
  assert(reader.dispose("battery-1",30,epoch));
  assert(!reader.latest("battery-1"));
  reader.stop();
  const auto next_epoch=reader.start();
  assert(next_epoch!=epoch);
  assert(!reader.receive(*latest.report,epoch));
  reader.stop();

  CollectionConsumer<CollectionService> list;
  list.metadata({"cells",2,"cell-2",200,"cell-1"});
  assert(!list.snapshot());
  assert(list.upsert({"cell-2",200,20,""}));
  assert(!list.snapshot());
  assert(list.upsert({"cell-1",100,10,"cell-2"}));
  assert(list.snapshot() && list.snapshot()->size()==2);
  assert((*list.snapshot())[0]==10 && (*list.snapshot())[1]==20);
  assert(list.commit_count()==1);
  // Metadata update after deleting a member must produce a consistent view.
  assert(list.dispose("cell-2",300));
  // A linked List delete must also rewire the surviving predecessor.
  assert(list.upsert({"cell-1",301,10,""}));
  list.metadata({"cells",1,"cell-1",301,"cell-1"});
  assert(list.snapshot() && list.snapshot()->size()==1);
  assert((*list.snapshot())[0]==10);
}
