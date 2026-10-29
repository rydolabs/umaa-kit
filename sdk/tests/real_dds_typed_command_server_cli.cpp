// SDK-owned DDS Request Reader and Command FSM, no application DDS calls.
#include "umaa/application.hpp"
#include "fixtures/synthetic_async_engine_binding.hpp"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
int main(int argc,char** argv) {
  if(argc!=4)return 2;
  try {
    const std::string mode=argv[1];
    if(mode!="server"&&mode!="server-cancel"&&mode!="server-update")return 2;
    umaa::Application app;
    app.load_component(argv[3]);
    app.use_backend(argv[2],44);
    auto server=app.serve_remote<AsyncEngine>(
       "controller","vehicle",std::chrono::milliseconds(180));
    const bool cancel_mode=mode=="server-cancel";
    const bool update_mode=mode=="server-update";
    for(int i=0;i<500&&server->retired_sessions()==0&&!server->faulted();++i){
      const bool device_poll=!cancel_mode &&
          (!update_mode||server->accepted_requests()>=2);
      server->poll(20,device_poll);
      std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    const bool ok=!server->faulted()&&server->accepted_requests()==
        (update_mode?2u:1u)&&server->retired_sessions()==1 &&
        server->terminal_commands()==1 &&
        server->canceled_commands()==(cancel_mode?1u:0u)&&
        server->active_sessions()==0;
    if(ok)server->wait_for_acks(5000);
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    std::cout<<"SERVER_TYPED accepted="<<server->accepted_requests()
             <<" terminal="<<server->terminal_commands()
             <<" canceled="<<server->canceled_commands()
             <<" retired="<<server->retired_sessions()
             <<" faulted="<<server->faulted()<<"\n";
    return ok?0:3;
  }catch(const std::exception& e) {
    std::cerr<<"Typed Command server failure: "<<e.what()<<"\n";
    return 4;
  }
}
