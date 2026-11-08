// Generic development SDK Component Host: explicit executable ReportBinding,
// ABI-v2 business Provider/Consumer modules and one host-owned DDS Domain.
// Original UMAA native TypeSupport/Key/QoS is NOT approved by this demo host.
#include "umaa/internal/component_runtime.hpp"
#include "umaa/internal/dds_backend.h"
#include "umaa/internal/host_report_bindings.hpp"
#if defined(UMAA_WITH_SOURCE_PINNED_BATTERY)
#include "umaa/internal/native_host_source_bindings.hpp"
#endif
#include "umaa/internal/runtime_config.hpp"
#include "umaa/module_api_v2.h"
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
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
  if(config.report_publishers.empty() && config.report_consumers.empty())
    throw std::invalid_argument("Report Host requires a publisher or consumer");
  std::set<std::pair<std::string,std::string>> instances;
  for(const auto& pub:config.report_publishers){
    umaa::internal::require_host_report_binding(
      pub.service_id,pub.topic_name,pub.schema_id,pub.qos_profile);
    if(!instances.insert({pub.service_id,pub.instance_key}).second)
      throw std::invalid_argument("duplicate Report publisher instance key");
  }
  std::set<std::string> consumers;
  for(const auto& sub:config.report_consumers){
    umaa::internal::require_host_report_binding(
      sub.service_id,sub.topic_name,sub.schema_id,sub.qos_profile);
    if(!consumers.insert(sub.service_id).second)
      throw std::invalid_argument("duplicate Report consumer service");
  }
}

// One reader polls exactly one sample per dispatch. The ReportSubscription
// currently exposes only its last event; batching would silently skip
// intermediate updates. Preserve Source→host-token identity until real
// keyed Dispose; never turn UNREGISTER or a missing source into Dispose.
class ReportForwarder final {
public:
  ReportForwarder(umaa::internal::ComponentRuntime& runtime,
                  const umaa::internal::RuntimeConfig::ReportConsumerConfig& binding)
    :runtime_(runtime),service_(binding.service_id),
     subscription_(runtime.subscribe_report_fixture(
       binding.service_id,binding.topic_name,binding.schema_id,binding.qos_profile)) {}

  bool poll_one() {
    const auto n=subscription_->poll(1);
    if(n<0)throw std::runtime_error("Report Consumer DDS reader failure");
    if(n==0)return false;
    std::vector<std::uint8_t> bytes;
    std::string key;
    std::uint32_t kind=0;
    std::uint64_t revision=0;
    if(!subscription_->last_event(bytes,key,kind,revision)||
       revision==last_revision_)return true; // a duplicate/timestamp-rejected sample
    if(revision!=last_revision_+1)
      throw std::runtime_error("Report Consumer event gap; no delivery claim");
    last_revision_=revision;
    if(key.empty()||key.size()>256)
      throw std::runtime_error("invalid Report Source key");
    if(kind==UMAA_DDS_EVENT_DATA){
      auto it=tokens_.find(key);
      if(it==tokens_.end()){
        if(tokens_.size()>=64 || next_token_==0)
          throw std::runtime_error("Report Consumer source budget exhausted");
        it=tokens_.emplace(key,next_token_++).first;
      }
      deliver(UMAA_COMPONENT_REPORT_ACCEPT,bytes,it->second);
      ++accepted_;
    }else if(kind==UMAA_DDS_EVENT_DISPOSE){
      auto it=tokens_.find(key);
      if(it==tokens_.end())throw std::runtime_error(
          "Report Dispose for unknown Source; no invented business callback");
      deliver(UMAA_COMPONENT_REPORT_DISPOSE,{},it->second);
      tokens_.erase(it);
      ++disposed_;
    }else{
      // UNREGISTER cannot be represented by ABI-v2 ReportSink::on_dispose.
      throw std::runtime_error("unsupported Report lifecycle event");
    }
    return true;
  }
  size_t accepted() const noexcept{return accepted_;}
  size_t disposed() const noexcept{return disposed_;}
  size_t active() const noexcept{return tokens_.size();}
private:
  void deliver(std::uint32_t action,const std::vector<std::uint8_t>& bytes,
               std::uint64_t token){
    std::vector<std::uint8_t> output;
    std::int32_t result=-1;
    const auto status=runtime_.call(service_,action,bytes,output,result,token);
    if(status!=UMAA_COMPONENT_OK || result!=0)
      throw std::runtime_error("business Report Consumer rejected DDS event");
  }
  umaa::internal::ComponentRuntime& runtime_;
  std::string service_;
  std::unique_ptr<umaa::internal::ReportSubscription> subscription_;
  std::unordered_map<std::string,std::uint64_t> tokens_;
  std::uint64_t next_token_{1},last_revision_{0};
  size_t accepted_{0},disposed_{0};
};

int run(const umaa::internal::RuntimeConfig& config,uint32_t duration_ms){
  umaa::internal::ComponentRuntime runtime;
  runtime.load_backend(config.backend,config.domain_id);
  runtime.set_qos_profiles(config.qos_profiles);
  for(const auto& module:config.modules)runtime.load_component(module);

  // Reject wrong Component roles before opening ANY Reader/Writer.
  for(const auto& pub:config.report_publishers)
    if(!runtime.has_service_role(pub.service_id,pub.schema_id,4u))
      throw std::runtime_error("no matching Report Provider Component role");
  for(const auto& sub:config.report_consumers)
    if(!runtime.has_service_role(sub.service_id,sub.schema_id,8u|16u))
      throw std::runtime_error("no matching Report Consumer Component role");

  std::vector<std::unique_ptr<ReportForwarder>> consumers;
  for(const auto& sub:config.report_consumers)
    consumers.push_back(std::make_unique<ReportForwarder>(runtime,sub));
  std::vector<std::unique_ptr<umaa::internal::ReportPublisher>> publishers;
  for(const auto& pub:config.report_publishers)
    publishers.push_back(runtime.open_report_fixture(
      pub.service_id,pub.topic_name,pub.schema_id,pub.instance_key,pub.qos_profile));
  for(size_t i=0;i<publishers.size();++i)
    publishers[i]->start_periodic(
      std::chrono::milliseconds(config.report_publishers[i].interval_ms));

  std::cout<<"UMAA_HOST_READY modules="<<config.modules.size()
           <<" publishers="<<publishers.size()
           <<" consumers="<<consumers.size()
           <<" domain="<<config.domain_id<<std::endl;
  const auto started=std::chrono::steady_clock::now();
  auto drain=[&](size_t budget){
    size_t events=0;
    for(;;){
      bool progress=false;
      for(auto& consumer:consumers) {
        if(consumer->poll_one()){
          if(++events>budget)
            throw std::runtime_error("Report Consumer drain budget exhausted");
          progress=true;
        }
      }
      if(!progress)break;
    }
  };
  while(!stop_requested){
    for(const auto& publisher:publishers)
      if(publisher->faulted()){
        publisher->rethrow_periodic_error();
        throw std::runtime_error("Report publisher faulted");
      }
    // Limit work in each turn: overload is a failure, not a hidden drop.
    drain(4096);
    if(duration_ms && std::chrono::steady_clock::now()-started>=
         std::chrono::milliseconds(duration_ms))break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  for(auto& publisher:publishers)publisher->stop_periodic();
  for(auto& publisher:publishers)publisher->rethrow_periodic_error();
  drain(4096);
  for(auto& publisher:publishers){
    publisher->dispose();
    publisher->close();
  }
  // Consume actual DDS Dispose before claiming clean Host shutdown.
  drain(4096);
  size_t data_count=0,dispose_count=0,active_count=0;
  for(const auto& c:consumers){
    data_count+=c->accepted();
    dispose_count+=c->disposed();
    active_count+=c->active();
  }
  if(active_count)throw std::runtime_error(
      "Report Consumer has unresolved active Sources at shutdown");
  std::cout<<"UMAA_HOST_STOPPED reports="<<data_count
           <<" disposes="<<dispose_count
           <<" active="<<active_count<<std::endl;
  return 0;
}
}

int main(int argc,char** argv) {
  try {
#if defined(UMAA_WITH_SOURCE_PINNED_BATTERY)
    // The *same Host source* accepts an original-source native binding only
    // when it was explicitly compiled with authentic generated TypeSupport.
    // All other Host configurations remain subject to normal demo admission.
    if(argc>1 && std::string(argv[1])=="--native-binding")
      return umaa::internal::run_source_bound_native_host(argc,argv);
#else
    if(argc>1 && std::string(argv[1])=="--native-binding")
      throw std::invalid_argument(
        "native source binding not compiled into this SDK Host");
#endif
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
    validate(config); // Explicit source/TypeBinding gate precedes module load.
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
