// Incremental host-local typed consumer facade for Component ABI v2.
// Report poll/latest uses the backend Reader; CommandClient is host-local only.
#pragma once
#include "umaa/component.hpp"
#include "umaa/remote_command_client.hpp"
#include "umaa/dds_command_request_server.hpp"
#include "umaa/dds_collection_endpoint.hpp"
#include "umaa/internal/component_runtime.hpp"
#include "umaa/internal/dds_backend.h"
#include "umaa/internal/runtime_config.hpp" // vendor-neutral event kinds only
#include <cstdint>
#include <functional>
#include <exception>
#include <thread>
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
  using ReportCallback=std::function<void(const typename Service::Report&,const std::string&)>;
  using DisposeCallback=std::function<void(const std::string&)>;

public:
  explicit ReportConsumer(std::shared_ptr<internal::ComponentRuntime> host):host_(std::move(host)){}
  ~ReportConsumer(){stop_watch();}
  ReportConsumer(const ReportConsumer&)=delete;
  ReportConsumer& operator=(const ReportConsumer&)=delete;
  ReportConsumer(ReportConsumer&&)=delete;
  ReportConsumer& operator=(ReportConsumer&&)=delete;
  // Subscribe once and dispatch each new keyed Report / lifecycle event from
  // a private bounded polling thread. User callbacks contain business logic
  // only; never call a DDS vendor function. This is a synthetic v2 API slice.
  void on_change(ReportCallback on_report, DisposeCallback on_dispose={},
                 std::chrono::milliseconds interval=std::chrono::milliseconds(25)){
    if(!on_report||interval.count()<=0||interval>std::chrono::seconds(5))
      throw std::invalid_argument("invalid Report callback or polling interval");
    if(watcher_.joinable())throw std::logic_error("Report watch already started");
    if(!subscription_)subscription_=host_->subscribe_report_fixture(
       Service::id,Service::topic_name,Service::schema_id,Service::qos_profile);
    watch_stop_.store(false);
    watch_error_=nullptr;
    watcher_=std::thread([this,callback=std::move(on_report),
                          disposed=std::move(on_dispose),interval]{
      try {
        uint64_t delivered_revision=0;
        while(!watch_stop_.load()){
          const auto result=subscription_->poll(1);
          if(result<0)throw std::runtime_error("DDS Report subscription poll failed");
          if(result>0){
            std::vector<uint8_t> payload;
            std::string key;
            uint32_t kind=0;
            uint64_t revision=0;
            if(subscription_->last_event(payload,key,kind,revision) &&
               revision!=delivered_revision){
              delivered_revision=revision;
              if(kind==UMAA_DDS_EVENT_DATA){
                typename Service::Report data{};
                if(!Service::decode_report(payload.data(),payload.size(),data))
                  throw std::runtime_error("DDS Report schema decoding failed");
                callback(data,key);
              }else if(disposed&&(kind==UMAA_DDS_EVENT_DISPOSE ||
                                   kind==UMAA_DDS_EVENT_UNREGISTER))
                disposed(key);
            }
          }else std::this_thread::sleep_for(interval);
        }
      }catch(...){watch_error_=std::current_exception();watch_stop_.store(true);}
    });
  }
  void stop_watch() noexcept {
    watch_stop_.store(true);
    if(watcher_.joinable()){
      // Self-destruction from a callback is unsupported; callback may request
      // shutdown, but owning thread must perform the final join.
      if(watcher_.get_id()!=std::this_thread::get_id())watcher_.join();
    }
  }
  void rethrow_watch_error(){
    stop_watch();
    if(watch_error_)std::rethrow_exception(watch_error_);
  }

  // Lazy backend Reader. Service metadata is generated/reviewed, never guessed.
  int32_t poll(uint32_t max_samples=1){
    if(watcher_.joinable())throw std::logic_error("manual poll while Report watcher owns Reader");
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
  // Explicit keyed access: latest() alone is insufficient for multiple instances.
  bool latest_for(const std::string& instance_key,typename Service::Report& out) const {
    if(!subscription_||instance_key.empty())return false;
    std::vector<uint8_t> bytes;
    if(!subscription_->latest_for(instance_key,bytes))return false;
    return Service::decode_report(bytes.data(),bytes.size(),out);
  }
  std::vector<std::string> keys() const {
    return subscription_?subscription_->keys():std::vector<std::string>{};
  }
  bool disposed_for(const std::string& instance_key) const {
    return subscription_&&subscription_->disposed_for(instance_key);
  }
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
  std::atomic<bool> watch_stop_{true};
  std::thread watcher_;
  std::exception_ptr watch_error_;
};
// Minimal typed Report publishing handle for an already registered device
// ReportSource. No DDS names or middleware objects appear in user code.
// A generated Service descriptor supplies a reviewed binding in production;
// today's runtimes only accept the explicit demo schema.
template<class Service> class ReportPublisherHandle final {
public:
  explicit ReportPublisherHandle(std::unique_ptr<internal::ReportPublisher> endpoint)
    :endpoint_(std::move(endpoint)){}
  ReportPublisherHandle(ReportPublisherHandle&&)=default;
  ReportPublisherHandle& operator=(ReportPublisherHandle&&)=default;
  ReportPublisherHandle(const ReportPublisherHandle&)=delete;
  ReportPublisherHandle& operator=(const ReportPublisherHandle&)=delete;
  void update(){endpoint_->update();}
  void dispose(){endpoint_->dispose();}
  void close(){endpoint_->close();}
  bool faulted() const noexcept{return endpoint_->faulted();}
  void start_periodic(std::chrono::milliseconds period){endpoint_->start_periodic(period);}
  void stop_periodic() noexcept{endpoint_->stop_periodic();}
  void rethrow_periodic_error(){endpoint_->rethrow_periodic_error();}
private:
  std::unique_ptr<internal::ReportPublisher> endpoint_;
};
class Application final {
public:
  Application():host_(std::make_shared<internal::ComponentRuntime>()){}
  static Application from_config(const std::string& config_path) {
    auto config=internal::RuntimeConfig::read(config_path);
    Application app;
    app.use_backend(config.backend,config.domain_id);
    for(const auto& module:config.modules)app.load_component(module);
    return app;
  }
  void load_component(const std::string& path){host_->load_component(path);}
  // Cooperative in-process reload; not a crash-safe hot-upgrade.
  bool unload_component(const std::string& id){return host_->unload_component(id);}
  template<class Service>
  ReportPublisherHandle<Service> publish(const std::string& instance_key) {
    static_assert(!Service::is_command,"publish<Service>() requires a Report contract");
    if(instance_key.empty())throw std::invalid_argument("Report instance key required");
    return ReportPublisherHandle<Service>(host_->open_report_fixture(
      Service::id,Service::topic_name,Service::schema_id,instance_key));
  }

  void use_backend(const std::string& path,uint32_t domain){
    host_->load_backend(path,domain);
    backend_path_=path;domain_id_=domain;
  }
  // Device-independent remote Command client. Binding codecs remain Service-owned.
  // Existing consume<Service>() retains its original host-local semantics.
  template<class Service>
  RemoteCommandClient<Service> consume_remote(const std::string& source,
                                              const std::string& destination) const {
    static_assert(Service::is_command,"remote Command consumer requires a Command service");
    if(backend_path_.empty())throw std::logic_error("configure DDS backend first");
    return RemoteCommandClient<Service>(backend_path_,domain_id_,source,destination);
  }
  // Bounded Set/List sample-atomic development transport. No vendor objects
  // or manual DDS QoS in application code. NOT native DDS GROUP presentation.
  template<class Service>
  std::unique_ptr<DdsCollectionPublisher<Service>> publish_collection(
      const std::string& instance_key,std::size_t maximum=1024) const {
    if(backend_path_.empty())throw std::logic_error("configure DDS backend first");
    return std::make_unique<DdsCollectionPublisher<Service>>(
       backend_path_,domain_id_,instance_key,maximum);
  }
  template<class Service>
  std::unique_ptr<DdsCollectionConsumer<Service>> consume_collection(
      const std::string& instance_key,std::size_t maximum=1024) const {
    if(backend_path_.empty())throw std::logic_error("configure DDS backend first");
    return std::make_unique<DdsCollectionConsumer<Service>>(
       backend_path_,domain_id_,instance_key,maximum);
  }
  // Typed remote Command provider: SDK owns DDS Request reader and
  // status/ack FSM. Device-specific business code is only provide<Service>().
  template<class Service>
  std::unique_ptr<DdsCommandRequestServer<Service>> serve_remote(
      const std::string& source,const std::string& provider,
      std::chrono::milliseconds retirement_delay={}) const {
    static_assert(Service::is_command,"serve_remote requires Command service");
    if(backend_path_.empty())throw std::logic_error("configure DDS backend first");
    return std::make_unique<DdsCommandRequestServer<Service>>(
        backend_path_,domain_id_,host_,source,provider,retirement_delay);
  }
  // The SDK owns the Command reader polling thread as well as the FSM.
  template<class Service>
  std::unique_ptr<DdsCommandRequestServer<Service>> start_remote(
      const std::string& source,const std::string& provider,
      std::chrono::milliseconds retirement_delay={},
      std::chrono::milliseconds poll_interval=std::chrono::milliseconds(25)) const {
    auto server=serve_remote<Service>(source,provider,retirement_delay);
    server->start_background(poll_interval);
    return server;
  }
  // One-step typed asynchronous Command Handle. DDS channel lifetime belongs
  // to the Handle, not to this Application's ComponentRuntime.
  template<class Service>
  RemoteCommandHandle<Service> issue_remote(
      const typename Service::Command& command,const std::string& session,
      const std::string& source,const std::string& destination) const {
    static_assert(Service::is_command,"issue_remote requires Command service");
    if(backend_path_.empty())throw std::logic_error("configure DDS backend first");
    auto client=std::make_shared<RemoteCommandClient<Service>>(
      backend_path_,domain_id_,source,destination);
    client->send(command,session);
    return RemoteCommandHandle<Service>(std::move(client),session);
  }
  template<class Service> auto consume() {
    if constexpr(Service::is_command)return CommandClient<Service>(host_);
    else return ReportConsumer<Service>(host_);
  }
private:
  std::shared_ptr<internal::ComponentRuntime> host_;
  std::string backend_path_;
  uint32_t domain_id_{0};
};
} // namespace umaa
