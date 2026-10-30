// Minimal typed remote Command client, no DDS Reader/Writer or vendor headers.
// Uses synthetic Command IDL for independent-process acceptance only.
#include "umaa/application.hpp"
#include "fixtures/synthetic_async_engine_binding.hpp"
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main(int argc,char** argv){
  if(argc!=4 || (std::string(argv[1])!="client" &&
                 std::string(argv[1])!="client-cancel" &&
                 std::string(argv[1])!="client-update"))return 2;
  try{
    // A typed CommandHandle must own its DDS channel independently of
    // the short-lived Application configuration object that created it.
    auto cmd=[&] {
      umaa::Application app;
      app.use_backend(argv[2],44);
      return app.issue_remote<AsyncEngine>({1800},"17","controller","vehicle");
    }();
    const bool cancel_mode=std::string(argv[1])=="client-cancel";
    const bool update_mode=std::string(argv[1])=="client-update";
    if(update_mode) {
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
      while(!cmd.ready_for_update()&&std::chrono::steady_clock::now()<deadline){
        cmd.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      if(!cmd.ready_for_update())return 6;
      cmd.update({1900});
    }
    if(cancel_mode) {
      const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
      while(!cmd.acknowledged() && std::chrono::steady_clock::now()<deadline) {
        cmd.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      if(!cmd.acknowledged() ||
         cmd.state()!=umaa::RemoteCommandResult::Pending) return 5;
      cmd.request_cancel(std::chrono::seconds(5));
    }
    const auto outcome=cmd.wait_for(std::chrono::seconds(10));
    const auto expected=cancel_mode?umaa::RemoteCommandResult::Canceled:
                                    umaa::RemoteCommandResult::Completed;
    if(outcome!=expected){
      std::cerr<<"Remote Command did not reach expected terminal Status\n";return 3;
    }
    if(!cancel_mode)std::this_thread::sleep_for(std::chrono::milliseconds(150));
    cmd.finish(std::chrono::seconds(10));
    std::cout<<"PASS typed remote Command "
             <<(cancel_mode?"cancellation":update_mode?"revision update":"completion")
             <<" after real Status/Ack and Dispose\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"Typed Command failure: "<<e.what()<<"\n";return 4;
  }
}
