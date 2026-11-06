// Runnable UMAA SDK Component Host v1: business DLL/SO and DDS ABI plugin.
// Only a source-explicit synthetic Report fixture is admitted for now.
// Formal UMAA 6.0 ServiceBinding generation is not implemented here.
#include "umaa/internal/component_runtime.hpp"
#include "umaa/internal/runtime_config.hpp"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
volatile std::sig_atomic_t stop_requested=0;
extern "C" void stop_signal(int) {stop_requested=1;}

uint32_t duration(const std::string& value) {
  if(value.empty())throw std::invalid_argument("invalid Host duration");
  uint32_t result=0;
  for(unsigned char c:value){
    if(c<'0'||c>'9'||result>600000u/10)
      throw std::invalid_argument("invalid Host duration");
    result=result*10+static_cast<uint32_t>(c-'0');
    if(result>600000u)throw std::invalid_argument("Host duration exceeds 10 minutes");
  }
  if(!result)throw std::invalid_argument("Host duration must be positive");
  return result;
}

void validate(const umaa::internal::RuntimeConfig& config) {
  if(config.modules.empty())
    throw std::invalid_argument("Component Host requires at least one module");
  if(config.report_publishers.empty())
    throw std::invalid_argument("Report Host requires an explicit publisher");
  std::set<std::pair<std::string,std::string>> instances;
  for(const auto& pub:config.report_publishers){
    // Prevent injecting an arbitrary UMAA:: service name or a "verified"
    // marker into a development-only backend/host path.
    if(pub.service_id!="Demo::EO::BatteryStatus" ||
       pub.topic_name!="demo/BatteryStatus" ||
       pub.schema_id!="demo:battery-v1" ||
       pub.qos_profile!="demo-reliable")
      throw std::invalid_argument("unapproved Host Report DDS binding");
    if(!instances.insert({pub.service_id,pub.instance_key}).second)
      throw std::invalid_argument("duplicate Report publisher instance key");
  }
}

int run(const umaa::internal::RuntimeConfig& config, uint32_t duration_ms){
  umaa::internal::ComponentRuntime runtime;
  runtime.load_backend(config.backend,config.domain_id);
  runtime.set_qos_profiles(config.qos_profiles);
  for(const auto& module:config.modules)runtime.load_component(module);
  std::vector<std::unique_ptr<umaa::internal::ReportPublisher>> publishers;
  for(const auto& pub:config.report_publishers)
    publishers.push_back(runtime.open_report_fixture(
      pub.service_id,pub.topic_name,pub.schema_id,pub.instance_key,pub.qos_profile));
  for(size_t i=0;i<publishers.size();++i)
    publishers[i]->start_periodic(
      std::chrono::milliseconds(config.report_publishers[i].interval_ms));

  std::cout<<"UMAA_HOST_READY modules="<<config.modules.size()
           <<" publishers="<<publishers.size()
           <<" domain="<<config.domain_id<<std::endl;
  const auto started=std::chrono::steady_clock::now();
  while(!stop_requested){
    for(const auto& publisher:publishers)
      if(publisher->faulted()){
        publisher->rethrow_periodic_error();
        throw std::runtime_error("Report publisher faulted");
      }
    if(duration_ms && std::chrono::steady_clock::now()-started>=
         std::chrono::milliseconds(duration_ms))break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Stop and inspect every asynchronous publisher before emitting "STOPPED".
  // On any failure the Host exits nonzero and never claims clean shutdown.
  for(auto& publisher:publishers)publisher->stop_periodic();
  for(auto& publisher:publishers)publisher->rethrow_periodic_error();
  for(auto& publisher:publishers){
    publisher->dispose();
    publisher->close();
  }
  std::cout<<"UMAA_HOST_STOPPED"<<std::endl;
  // Publisher wrappers go out of scope before the runtime/backend.
  return 0;
}
}

int main(int argc,char** argv) {
  try {
    if(argc!=3 && argc!=5)
      throw std::invalid_argument(
        "usage: umaa_component_host --config FILE [--duration-ms 1..600000]");
    if(std::string(argv[1])!="--config")
      throw std::invalid_argument("Host requires --config FILE");
    uint32_t ms=0;
    if(argc==5){
      if(std::string(argv[3])!="--duration-ms")
        throw std::invalid_argument("unknown Host argument");
      ms=duration(argv[4]);
    }
    const auto config=umaa::internal::RuntimeConfig::read(argv[2]);
    validate(config); // Reject unknown service before DLL/SO is opened.
    std::signal(SIGINT,stop_signal);
    std::signal(SIGTERM,stop_signal);
    return run(config,ms);
  }catch(const std::exception& error){
    std::cerr<<"UMAA_HOST_ERROR: "<<error.what()<<std::endl;
    return 2;
  }catch(...){
    std::cerr<<"UMAA_HOST_ERROR: unknown failure"<<std::endl;
    return 2;
  }
}
