// Incremental host-local typed consumer facade for Component ABI v2.
// This slice does NOT claim DDS CommandStatus/Ack or queryless Report semantics.
#pragma once
#include "umaa/component.hpp"
#include "umaa/internal/component_runtime.hpp"
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
class CommandHandle final {
public:
  Result state() const noexcept { return state_; }
  bool terminal() const noexcept { return state_ != Result::Pending; }
  Result wait() const {
    if (!terminal()) throw std::logic_error("Pending command requires UMAA asynchronous status integration");
    return state_;
  }
private:
  explicit CommandHandle(Result state):state_(state){}
  Result state_;
  template<class S> friend class CommandClient;
};
template<class Service> class CommandClient final {
public:
  explicit CommandClient(std::shared_ptr<internal::ComponentRuntime> host):host_(std::move(host)){}
  CommandHandle send(const typename Service::Command& command) {
    auto payload=Service::encode_command(command);
    std::vector<uint8_t> output;
    int32_t business=-1;
    int32_t rc=host_->call(Service::id,UMAA_COMPONENT_COMMAND,payload,output,business);
    if(rc!=UMAA_COMPONENT_OK)throw std::runtime_error("device module command invocation failed");
    if(business<static_cast<int32_t>(Result::Completed) ||
       business>static_cast<int32_t>(Result::Failed))
      throw std::runtime_error("invalid device result");
    return CommandHandle(static_cast<Result>(business));
  }
private:
  std::shared_ptr<internal::ComponentRuntime> host_;
};
template<class Service> class ReportConsumer final {
public:
  explicit ReportConsumer(std::shared_ptr<internal::ComponentRuntime> host):host_(std::move(host)){}
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
