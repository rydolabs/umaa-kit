// Incremental host-local typed consumer facade for Component ABI v2.
// Report poll/latest uses the backend Reader; CommandClient is host-local only.
#pragma once
#include "umaa/component.hpp"
#include "umaa/internal/component_runtime.hpp"
#include <cstdint>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
// Host-local asynchronous business result, not an UMAA DDS Ack/Status session.
class CommandHandle final {
public:
  Result state() const noexcept {return outcome_;}
  uint64_t session_id() const noexcept {return session_;}
  bool terminal() const noexcept {return outcome_!=Result::Pending;}
  Result poll() {
    if(terminal())return outcome_;
    outcome_=request(UMAA_COMPONENT_COMMAND_POLL);
    return outcome_;
  }
  Result cancel() {
    if(terminal())return outcome_;
    const auto r=request(UMAA_COMPONENT_CANCEL);
    if(r==Result::Completed||r==Result::Failed)outcome_=r;
    return r;
  }
  Result wait() const {
    if(!terminal())throw std::logic_error(
      "pending device command requires wait_for() or UMAA status integration");
    return outcome_;
  }
  Result wait_for(std::chrono::milliseconds timeout,
                  std::chrono::milliseconds interval=std::chrono::milliseconds(10)) {
    if(timeout.count()<0||interval.count()<=0)
      throw std::invalid_argument("invalid polling interval");
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    while(!terminal()) {
      poll();
      if(terminal()||std::chrono::steady_clock::now()>=deadline)break;
      std::this_thread::sleep_for(std::min(interval,
        std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now())));
    }
    return outcome_; // Pending remains Pending on timeout
  }
private:
  Result request(uint32_t action) {
    std::vector<std::uint8_t> empty,out;
    int32_t result=-1;
    if(host_->call(service_,action,empty,out,result,session_)!=UMAA_COMPONENT_OK)
      throw std::runtime_error("component session operation failed");
    if(result<0||result>static_cast<int32_t>(Result::Failed))
      throw std::runtime_error("invalid business result");
    return static_cast<Result>(result);
  }
  CommandHandle(std::shared_ptr<internal::ComponentRuntime> h,std::string service,
                uint64_t session,Result result):host_(std::move(h)),
                service_(std::move(service)),session_(session),outcome_(result){}
  std::shared_ptr<internal::ComponentRuntime> host_;
  std::string service_;
  uint64_t session_;
  Result outcome_;
  template<class> friend class CommandClient;
};
template<class Service> class CommandClient final {
public:
  using Command=typename Service::Command;
  explicit CommandClient(std::shared_ptr<internal::ComponentRuntime> h):host_(std::move(h)){}
  CommandHandle send(const Command& command) {
    const auto payload=Service::encode_command(command);
    static std::atomic<uint64_t> counter{1};
    const uint64_t session=counter.fetch_add(1,std::memory_order_relaxed);
    if(session==0)throw std::overflow_error("command session counter exhausted");
    std::vector<std::uint8_t> out;
    int32_t business=-1;
    if(host_->call(Service::id,UMAA_COMPONENT_COMMAND,payload,out,business,session)!=UMAA_COMPONENT_OK)
      throw std::runtime_error("component command dispatch failed");
    if(business<0||business>static_cast<int32_t>(Result::Failed))
      throw std::runtime_error("invalid business result");
    return CommandHandle(host_,Service::id,session,static_cast<Result>(business));
  }
private:
  std::shared_ptr<internal::ComponentRuntime> host_;
};

template<class Service> class ReportConsumer final {
public:
  explicit ReportConsumer(std::shared_ptr<internal::ComponentRuntime> host):host_(std::move(host)){}
  // Lazy backend Reader. Service metadata is generated/reviewed, never guessed.
  int32_t poll(uint32_t max_samples=1){
    if(!subscription_)subscription_=host_->subscribe_report_fixture(
       Service::id,Service::topic_name,Service::schema_id,Service::qos_profile);
    return subscription_->poll(max_samples);
  }
  bool latest(typename Service::Report& out,std::string& key) const {
    if(!subscription_)return false;
    std::vector<uint8_t> bytes;
    if(!subscription_->latest(bytes,key))return false;
    return Service::decode_report(bytes.data(),bytes.size(),out);
  }
  bool disposed() const {return subscription_&&subscription_->disposed();}
  // Development-only local read, not DDS subscription.
  typename Service::Report read_local() {
    std::vector<uint8_t> empty,out;
    int32_t business=-1;
    if(host_->call(Service::id,UMAA_COMPONENT_REPORT,empty,out,business)!=UMAA_COMPONENT_OK ||
       business!=static_cast<int32_t>(Result::Completed))
      throw std::runtime_error("device module report read failed");
    typename Service::Report report{};
    if(!Service::decode_report(out.data(),out.size(),report))
      throw std::runtime_error("report schema decode failed");
    return report;
  }
private:
  std::shared_ptr<internal::ComponentRuntime> host_;
  std::unique_ptr<internal::ReportSubscription> subscription_;
};
class Application final {
public:
  Application():host_(std::make_shared<internal::ComponentRuntime>()){}
  void load_component(const std::string& path){host_->load_component(path);}
  void use_backend(const std::string& path,uint32_t domain){host_->load_backend(path,domain);}
  template<class Service> auto consume() {
    if constexpr(Service::is_command)return CommandClient<Service>(host_);
    else return ReportConsumer<Service>(host_);
  }
private:
  std::shared_ptr<internal::ComponentRuntime> host_;
};
} // namespace umaa
