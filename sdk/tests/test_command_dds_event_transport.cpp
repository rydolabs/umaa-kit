#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/component_command_provider.hpp"
#include "umaa/dds_command_event_transport.hpp"
#include "umaa/remote_command_monitor.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
struct S {
 static constexpr const char* id="Demo::EO::AsyncEngineControl";
 static constexpr const char* status_topic="demo/AsyncEngineStatus";
 static constexpr const char* status_schema="demo:async-status-v1";
 static constexpr const char* ack_topic="demo/AsyncEngineAck";
 static constexpr const char* ack_schema="demo:async-ack-v1";
 static constexpr const char* qos_profile="demo-reliable";
 struct Command{std::int32_t rpm{};};
 static std::vector<std::uint8_t> encode_command(const Command& c) {
   std::vector<std::uint8_t> v(4);std::memcpy(v.data(),&c.rpm,4);return v;
 }
 static std::string instance_key(const umaa::Identity& id){return id.session;}
 static std::vector<std::uint8_t> encode_status(const umaa::Identity&,
                                                  const umaa::Event& e) {
   return std::vector<std::uint8_t>(e.detail.begin(),e.detail.end());
 }
 static std::vector<std::uint8_t> encode_ack(const umaa::Identity&,
                                               const Command& c){
   return encode_command(c);
 }
};
struct Capture{
 std::vector<std::string> values;
 std::vector<std::int64_t> times;
 static void receive(void* ctx,const UmaaDdsSampleV1* sample) {
   auto* self=static_cast<Capture*>(ctx);
   self->times.push_back(sample->source_timestamp_ns);
   if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE)
     self->values.push_back(std::string("dispose:")+sample->instance_key);
   else self->values.emplace_back(reinterpret_cast<const char*>(sample->payload),
                                   sample->payload_size);
 }
};
int main(int argc,char** argv){
 if(argc!=3)return 2;
 auto backend=std::make_shared<umaa::DdsBackendPlugin>(argv[2]);
 auto domain=std::shared_ptr<umaa::DdsBackendPlugin::Domain>(backend->open_domain(87).release());
 Capture statuses,acks;
 UmaaDdsTopicV1 st{S::status_topic,S::status_schema,S::qos_profile,UMAA_DDS_CAP_RELIABLE};
 UmaaDdsTopicV1 ak{S::ack_topic,S::ack_schema,S::qos_profile,UMAA_DDS_CAP_RELIABLE};
 auto* status_reader=domain->reader(st,&Capture::receive,&statuses);
 auto* ack_reader=domain->reader(ak,&Capture::receive,&acks);
 {
   umaa::DdsCommandEventTransport<S> writer(backend,domain);
   auto runtime=std::make_shared<umaa::internal::ComponentRuntime>();
   runtime->load_component(argv[1]);
   umaa::ComponentCommandProvider<S> provider("vehicle",runtime,writer);
   auto events=provider.receive({"controller","vehicle","s1",1},{1800});
   assert(events.size()==4);
   assert(provider.poll_device("s1")[0].kind==umaa::Event::Kind::Ignored);
   assert(provider.poll_device("s1")[0].kind==umaa::Event::Kind::Ignored);
   assert(provider.poll_device("s1")[0].detail=="COMPLETED");
   assert(provider.command_disposed("controller","s1").size()==2);
   writer.wait_for_acks(1000);
   assert(domain->poll(status_reader,20)==5);
   assert(domain->poll(ack_reader,20)==2);
   assert(statuses.values.size()==5 && acks.values.size()==2);
   assert(statuses.values[0]=="ISSUED" && statuses.values[3]=="COMPLETED");
   assert(statuses.values[4]=="dispose:s1" && acks.values[1]=="dispose:s1");
   // Independent receiving client: Status alone must not fabricate success.
   umaa::RemoteCommandMonitor client;
   client.issue({"controller","vehicle","s1",1});
   assert(client.status("s1","vehicle",umaa::State::ISSUED,statuses.times[0]));
   assert(client.status("s1","vehicle",umaa::State::COMMANDED,statuses.times[1]));
   assert(client.status("s1","vehicle",umaa::State::EXECUTING,statuses.times[2]));
   assert(client.status("s1","vehicle",umaa::State::COMPLETED,statuses.times[3]));
   assert(client.result("s1")==umaa::RemoteCommandResult::Pending);
   assert(client.ack("s1","vehicle",acks.times[0]));
   assert(client.result("s1")==umaa::RemoteCommandResult::Completed);
   assert(client.disposed_status("s1","vehicle"));
   assert(client.disposed_ack("s1","vehicle"));
   assert(client.release("s1"));
 }
 domain->close_reader(status_reader);
 domain->close_reader(ack_reader);
 std::cout<<"PASS command protocol events over vendor-neutral mock DDS writer/reader\n";
}
