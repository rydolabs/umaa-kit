#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/component_command_provider.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
struct Async {
 static constexpr const char* id="Demo::EO::AsyncEngineControl";
 struct Command {std::int32_t rpm{};};
 static std::vector<std::uint8_t> encode_command(const Command& cmd){
   std::vector<std::uint8_t> out(sizeof(cmd.rpm));
   std::memcpy(out.data(),&cmd.rpm,out.size());return out;
 }
};
struct Sink:umaa::CommandTransport<Async::Command>{
 std::vector<std::string> data;
 void publish_status(const umaa::Identity& id,const umaa::Event& e) override {
   data.push_back("status:"+id.source+":"+id.session+":"+e.detail);
 }
 void publish_ack(const umaa::Identity& id,const Async::Command&) override {
   data.push_back("ack:"+id.session);
 }
 void dispose_status(const umaa::Identity& id) override {
   data.push_back("dispose-status:"+id.session);
 }
 void dispose_ack(const umaa::Identity& id,const Async::Command&) override {
   data.push_back("dispose-ack:"+id.session);
 }
};
int main(int argc,char** argv) {
 if(argc!=2)return 2;
 auto runtime=std::make_shared<umaa::internal::ComponentRuntime>();
 runtime->load_component(argv[1]);
 Sink sink;
 umaa::ComponentCommandProvider<Async> provider("vehicle",runtime,sink);
 auto accepted=provider.receive({"controller","vehicle","s1",10},{1500});
 assert(accepted.size()==4);
 assert(provider.sessions()==1 && sink.data.size()==4);
 auto pending=provider.poll_device("s1");
 assert(pending.size()==1 && pending[0].kind==umaa::Event::Kind::Ignored);
 assert(provider.poll_device("s1")[0].kind==umaa::Event::Kind::Ignored);
 auto done=provider.poll_device("s1");
 assert(done.size()==1 && done[0].detail=="COMPLETED");
 assert(sink.data.back()=="status:controller:s1:COMPLETED");
 auto disposal=provider.command_disposed("controller","s1");
 assert(disposal.size()==2 && provider.sessions()==0);
 assert(sink.data.back()=="dispose-ack:s1");
 // A different source cannot hijack the command session.
 provider.receive({"other","vehicle","s2",20},{2000});
 auto ignored=provider.command_disposed("intruder","s2");
 assert(ignored.size()==1 && ignored[0].kind==umaa::Event::Kind::Ignored);
 assert(provider.sessions()==1);
 assert(provider.poll_device("s2")[0].kind==umaa::Event::Kind::Ignored);
 assert(provider.poll_device("s2")[0].kind==umaa::Event::Kind::Ignored);
 assert(provider.poll_device("s2")[0].detail=="COMPLETED");
 assert(provider.command_disposed("other","s2").size()==2);
 std::cout<<"PASS Component business Command -> CommandEngine statuses/ack/dispose\n";
}
