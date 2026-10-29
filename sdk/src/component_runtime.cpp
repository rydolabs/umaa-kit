#include "umaa/internal/component_runtime.hpp"
#include "umaa/component_module.hpp"
#include "umaa/component.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <stdexcept>
#include <mutex>
#include <memory>
#include <cstdint>
#include <chrono>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <exception>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>
#include <unordered_set>
namespace umaa::internal {
struct ComponentRuntime::State {
  std::mutex service_mutex;
  std::unordered_set<std::string> unloading;
  std::vector<std::shared_ptr<umaa::ComponentModule>> modules;
  std::unordered_map<std::string, std::shared_ptr<umaa::ComponentModule>> services;
  std::shared_ptr<umaa::DdsBackendPlugin> backend;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain;
};
struct ReportSubscription::State {
  std::shared_ptr<umaa::DdsBackendPlugin> backend;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain;
  void* reader{nullptr};
  mutable std::mutex mutex;
  struct Instance {std::vector<uint8_t> bytes; int64_t timestamp{INT64_MIN}; bool present{false}; bool disposed{false};};
  std::unordered_map<std::string,Instance> instances;
  std::string last_key;
  std::vector<uint8_t> last_event_payload;
  uint32_t last_event_kind{0};
  uint64_t last_event_revision{0};
  bool was_disposed{false};
  State(std::shared_ptr<umaa::DdsBackendPlugin> b,
        std::shared_ptr<umaa::DdsBackendPlugin::Domain> d)
      :backend(std::move(b)),domain(std::move(d)){}
  ~State(){if(reader) domain->close_reader(reader);}
  static void receive(void* user,const UmaaDdsSampleV1* sample) noexcept {
    if(!user||!sample||!sample->instance_key||
       (sample->payload_size&&!sample->payload))return;
    auto* self=static_cast<State*>(user);
    try{
      std::lock_guard<std::mutex> guard(self->mutex);
      const std::string key=sample->instance_key;
      auto& instance=self->instances[key];
      if(sample->source_timestamp_ns<=instance.timestamp)return; // same-time replay is not newer
      if(sample->event_kind==UMAA_DDS_EVENT_DATA){
        std::vector<uint8_t> bytes;
        if(sample->payload_size)bytes.assign(sample->payload,sample->payload+sample->payload_size);
        instance.bytes=std::move(bytes);
        instance.timestamp=sample->source_timestamp_ns;
        instance.present=true;instance.disposed=false;
      } else if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE ||
                sample->event_kind==UMAA_DDS_EVENT_UNREGISTER){
        instance.bytes.clear();
        instance.timestamp=sample->source_timestamp_ns;
        instance.present=false;
        instance.disposed=sample->event_kind==UMAA_DDS_EVENT_DISPOSE;
      } else return;
      self->last_key=key;
      self->last_event_kind=sample->event_kind;
      ++self->last_event_revision;
      self->last_event_payload=instance.present?instance.bytes:std::vector<uint8_t>{};
      self->was_disposed=instance.disposed;
    }catch(...){ /* Never throw across the vendor ABI. */ }
  }
};
ReportSubscription::ReportSubscription(std::unique_ptr<State> s):state_(std::move(s)){}
ReportSubscription::~ReportSubscription()=default;
int32_t ReportSubscription::poll(uint32_t n) {return state_->domain->poll(state_->reader,n);}
bool ReportSubscription::latest(std::vector<uint8_t>& out,std::string& key) const {
  std::lock_guard<std::mutex> guard(state_->mutex);
  const auto it=state_->instances.find(state_->last_key);
  if(it==state_->instances.end() || !it->second.present)return false;
  out=it->second.bytes;key=it->first;return true;
}
bool ReportSubscription::latest_for(const std::string& key,std::vector<uint8_t>& out) const {
  std::lock_guard<std::mutex> guard(state_->mutex);
  const auto it=state_->instances.find(key);
  if(it==state_->instances.end() || !it->second.present)return false;
  out=it->second.bytes;return true;
}
std::vector<std::string> ReportSubscription::keys() const {
  std::lock_guard<std::mutex> guard(state_->mutex);
  std::vector<std::string> result;
  for(const auto& p:state_->instances)if(p.second.present)result.push_back(p.first);
  return result;
}
bool ReportSubscription::disposed_for(const std::string& key) const {
  std::lock_guard<std::mutex> guard(state_->mutex);
  const auto it=state_->instances.find(key);
  return it!=state_->instances.end() && it->second.disposed;
}
bool ReportSubscription::last_event(std::vector<uint8_t>& payload,
 std::string& key,uint32_t& kind,uint64_t& revision) const {
  std::lock_guard<std::mutex> lock(state_->mutex);
  if(!state_->last_event_kind)return false;
  payload=state_->last_event_payload;
  key=state_->last_key;
  kind=state_->last_event_kind;
  revision=state_->last_event_revision;
  return true;
}
bool ReportSubscription::disposed() const {
  std::lock_guard<std::mutex> guard(state_->mutex);
  return state_->was_disposed;
}
std::unique_ptr<ReportSubscription> ComponentRuntime::subscribe_report_fixture(
 const std::string& service,const std::string& topic_name,
 const std::string& schema,const std::string& qos_profile){
  if(!state_->domain)throw std::runtime_error("DDS backend not installed");
  if(service.empty())throw std::invalid_argument("Report service ID is required");
  // A Consumer need not load a local Provider device module. The backend's
  // explicitly compiled TypeBinding must authorize Topic, schema and QoS.
  // If this host ALSO provides the service, verify its local role/schema.
  std::shared_ptr<umaa::ComponentModule> local_provider;
  {
    std::lock_guard<std::mutex> lock(state_->service_mutex);
    auto it=state_->services.find(service);
    if(it!=state_->services.end()){
      local_provider=it->second;
      if(state_->unloading.count(local_provider->name()))
        throw std::runtime_error("component unload is in progress");
    }
  }
  if(local_provider){
    bool permitted=false;
    for(const auto& entry:local_provider->services())
      if(entry.service==service&&entry.schema==schema&&(entry.actions&4u))permitted=true;
    if(!permitted)throw std::runtime_error("report type/role mismatch");
  }
  // Reject unknown TypeBinding and insufficient QoS by backend->reader().
  // Production generation must also verify Provider/Consumer roles.
  auto slot=std::make_unique<ReportSubscription::State>(state_->backend,state_->domain);
  const UmaaDdsTopicV1 descriptor{topic_name.c_str(),schema.c_str(),qos_profile.c_str(),
    UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
  slot->reader=slot->domain->reader(descriptor,&ReportSubscription::State::receive,slot.get());
  return std::unique_ptr<ReportSubscription>(new ReportSubscription(std::move(slot)));
}
ComponentRuntime::ComponentRuntime():state_(std::make_unique<State>()){}
ComponentRuntime::~ComponentRuntime()=default;
void ComponentRuntime::load_component(const std::string& filename){
  // Do not load modules while replacing another module. Management operations
  // are serialized; data calls may be concurrently serviced.
  {
    std::lock_guard<std::mutex> lock(state_->service_mutex);
    if(!state_->unloading.empty())
      throw std::runtime_error("component unload is in progress");
  }
  auto mod=std::make_shared<umaa::ComponentModule>(filename);
  std::lock_guard<std::mutex> lock(state_->service_mutex);
  for(const auto& service:mod->services())
    if(state_->services.count(service.service))
      throw std::runtime_error("duplicate service between components: "+service.service);
  for(const auto& service:mod->services())
    state_->services.emplace(service.service,mod);
  state_->modules.push_back(std::move(mod));
}
bool ComponentRuntime::unload_component(const std::string& module_id){
  std::shared_ptr<umaa::ComponentModule> module;
  {
    std::lock_guard<std::mutex> lock(state_->service_mutex);
    auto found=std::find_if(state_->modules.begin(),state_->modules.end(),
      [&](const auto& x){return x->name()==module_id;});
    if(found==state_->modules.end() || state_->unloading.count(module_id))
      return false;
    module=*found;
    // One vector ref, one temporary ref, and one ref per registered service.
    // ReportPublisher holds an additional ref: do not unload underneath it.
    if(module.use_count()>static_cast<long>(module->services().size()+2))
      return false;
    state_->unloading.insert(module_id);
  }
  bool stopped=false;
  try{stopped=module->stop();}
  catch(...){stopped=false;}
  {
    std::lock_guard<std::mutex> lock(state_->service_mutex);
    if(stopped){
      for(const auto& service:module->services()){
        auto pos=state_->services.find(service.service);
        if(pos!=state_->services.end()&&pos->second==module)
          state_->services.erase(pos);
      }
      state_->modules.erase(std::remove(state_->modules.begin(),
        state_->modules.end(),module),state_->modules.end());
    }
    state_->unloading.erase(module_id);
  }
  return stopped;
}
void ComponentRuntime::load_backend(const std::string& filename,uint32_t domain_id){
  if(state_->domain)throw std::runtime_error("DDS backend already installed");
  auto candidate=std::make_shared<umaa::DdsBackendPlugin>(filename);
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain(candidate->open_domain(domain_id));
  state_->backend=std::move(candidate);
  state_->domain=std::move(domain);
}
std::vector<std::string> ComponentRuntime::services()const{
  std::lock_guard<std::mutex> lock(state_->service_mutex);
  std::vector<std::string> out;
  for(const auto& s:state_->services)out.push_back(s.first);
  return out;
}
int32_t ComponentRuntime::call(const std::string& service_id,uint32_t action,
  const std::vector<uint8_t>& payload,std::vector<uint8_t>& output,
  int32_t& business_result,uint64_t session_id){
  std::shared_ptr<umaa::ComponentModule> module;
  {
    std::lock_guard<std::mutex> lock(state_->service_mutex);
    auto found=state_->services.find(service_id);
    if(found==state_->services.end())return UMAA_COMPONENT_NOT_FOUND;
    module=found->second;
    if(state_->unloading.count(module->name()))return UMAA_COMPONENT_NOT_ACTIVE;
  }
  return module->call(service_id,action,payload,output,business_result,session_id);
}
namespace {
int64_t next_report_timestamp(){
  static std::atomic<int64_t> last{0};
  const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  int64_t before=last.load();
  for(;;){
    const int64_t value=now>before?now:before+1;
    if(last.compare_exchange_weak(before,value))return value;
  }
}
}
struct ReportPublisher::State {
  std::shared_ptr<umaa::DdsBackendPlugin> backend;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain;
  std::shared_ptr<umaa::ComponentModule> module;
  void* writer{nullptr};
  std::string service,key;
  mutable std::mutex writer_mutex;
  std::mutex timer_mutex;
  std::condition_variable timer_wake;
  std::thread periodic;
  bool stop_timer{false};
  std::exception_ptr timer_error;
  bool active{true};
  std::atomic<bool> is_faulted{false};
  State(std::shared_ptr<umaa::DdsBackendPlugin> b,
        std::shared_ptr<umaa::DdsBackendPlugin::Domain> d,
        std::shared_ptr<umaa::ComponentModule> m,void* w,
        std::string s,std::string k):backend(std::move(b)),domain(std::move(d)),
        module(std::move(m)),writer(w),service(std::move(s)),key(std::move(k)){}
  ~State(){if(writer)domain->close_writer(writer);}
};
ReportPublisher::ReportPublisher(std::unique_ptr<State> state):state_(std::move(state)){}
ReportPublisher::~ReportPublisher(){stop_periodic();}
bool ReportPublisher::faulted() const noexcept{return state_->is_faulted.load();}
void ReportPublisher::update(){
  std::lock_guard<std::mutex> lock(state_->writer_mutex);
  if(!state_->active||state_->is_faulted)throw std::logic_error("report publisher not active");
  std::vector<uint8_t> in,out;int32_t business=-1;
  if(state_->module->call(state_->service,UMAA_COMPONENT_REPORT,in,out,business)!=UMAA_COMPONENT_OK ||
     business!=static_cast<int32_t>(umaa::Result::Completed))
    throw std::runtime_error("report Handler failed");
  const UmaaDdsSampleV1 sample{out.empty()?nullptr:out.data(),
    static_cast<uint32_t>(out.size()),state_->key.c_str(),next_report_timestamp(),UMAA_DDS_EVENT_DATA};
  if(state_->domain->write(state_->writer,sample)!=UMAA_DDS_OK){
    state_->is_faulted=true;throw std::runtime_error("report write delivery failed");
  }
}
void ReportPublisher::dispose(){
  std::lock_guard<std::mutex> lock(state_->writer_mutex);
  if(!state_->active||state_->is_faulted)throw std::logic_error("report publisher not active");
  const UmaaDdsSampleV1 sample{nullptr,0,state_->key.c_str(),next_report_timestamp(),UMAA_DDS_EVENT_DISPOSE};
  if(state_->domain->dispose(state_->writer,sample)!=UMAA_DDS_OK){
    state_->is_faulted=true;throw std::runtime_error("report dispose delivery failed");
  }
}
void ReportPublisher::close(){
  stop_periodic();
  std::lock_guard<std::mutex> lock(state_->writer_mutex);
  if(!state_->active)return;
  state_->active=false;
  if(!state_->is_faulted && state_->domain->wait_for_acks(state_->writer,5000)!=UMAA_DDS_OK){
    state_->is_faulted=true;throw std::runtime_error("report delivery ack timeout");
  }
  state_->domain->close_writer(state_->writer);state_->writer=nullptr;
}
void ReportPublisher::start_periodic(std::chrono::milliseconds interval){
  if(interval.count()<=0 || interval>std::chrono::hours(24))
    throw std::invalid_argument("invalid Report publish period");
  {
    std::lock_guard<std::mutex> writer_lock(state_->writer_mutex);
    if(!state_->active || state_->is_faulted.load())
      throw std::logic_error("Report publisher not active");
  }
  std::lock_guard<std::mutex> lock(state_->timer_mutex);
  if(state_->periodic.joinable())
    throw std::logic_error("Report schedule already active");
  state_->stop_timer=false;
  state_->timer_error=nullptr;
  state_->periodic=std::thread([this,interval]{
    try{
      for(;;){
        {
          std::lock_guard<std::mutex> timer_lock(state_->timer_mutex);
          if(state_->stop_timer)break;
        }
        update(); // the device ReportSource, then checked vendor write
        std::unique_lock<std::mutex> timer_lock(state_->timer_mutex);
        if(state_->timer_wake.wait_for(timer_lock,interval,
             [this]{return state_->stop_timer;}))break;
      }
    }catch(...){
      std::lock_guard<std::mutex> timer_lock(state_->timer_mutex);
      state_->is_faulted=true;
      state_->timer_error=std::current_exception();
      state_->stop_timer=true;
    }
  });
}
void ReportPublisher::stop_periodic() noexcept {
  {
    std::lock_guard<std::mutex> lock(state_->timer_mutex);
    state_->stop_timer=true;
  }
  state_->timer_wake.notify_all();
  if(state_->periodic.joinable() &&
      state_->periodic.get_id()!=std::this_thread::get_id())
    state_->periodic.join();
}
void ReportPublisher::rethrow_periodic_error(){
  stop_periodic();
  std::lock_guard<std::mutex> lock(state_->timer_mutex);
  if(state_->timer_error)std::rethrow_exception(state_->timer_error);
}
std::unique_ptr<ReportPublisher> ComponentRuntime::open_report_fixture(
    const std::string& service_id,const std::string& topic_name,
    const std::string& schema_id,const std::string& instance_key){
  if(!state_->domain)throw std::runtime_error("DDS backend not loaded");
  std::shared_ptr<umaa::ComponentModule> module;
  {
    std::lock_guard<std::mutex> lock(state_->service_mutex);
    const auto found=state_->services.find(service_id);
    if(found==state_->services.end())throw std::runtime_error("unknown report service");
    module=found->second;
    if(state_->unloading.count(module->name()))
      throw std::runtime_error("component unload is in progress");
  }
  bool authorized=false;
  for(const auto& d:module->services())
    if(d.service==service_id&&d.schema==schema_id&&(d.actions&4u))authorized=true;
  if(!authorized)throw std::runtime_error("report schema or role mismatch");
  const UmaaDdsTopicV1 topic{topic_name.c_str(),schema_id.c_str(),"demo-reliable",
    UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
  auto writer=state_->domain->writer(topic);
  return std::unique_ptr<ReportPublisher>(new ReportPublisher(
    std::make_unique<ReportPublisher::State>(state_->backend,state_->domain,
      module,writer,service_id,instance_key)));
}
void ComponentRuntime::publish_report_fixture(const std::string& service_id,
  const std::string& topic_name,const std::string& schema_id,
  const std::string& instance_key,bool dispose_after){
  auto publisher=open_report_fixture(service_id,topic_name,schema_id,instance_key);
  publisher->update();
  if(dispose_after)publisher->dispose();
  publisher->close();
}
} // namespace umaa::internal
