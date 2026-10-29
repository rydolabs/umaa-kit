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
#include <unordered_map>
#include <utility>
#include <vector>
namespace umaa::internal {
struct ComponentRuntime::State {
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
      if(sample->source_timestamp_ns<instance.timestamp)return;
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
bool ReportSubscription::disposed() const {
  std::lock_guard<std::mutex> guard(state_->mutex);
  return state_->was_disposed;
}
std::unique_ptr<ReportSubscription> ComponentRuntime::subscribe_report_fixture(
 const std::string& service,const std::string& topic_name,
 const std::string& schema,const std::string& qos_profile){
  if(!state_->domain)throw std::runtime_error("DDS backend not installed");
  auto it=state_->services.find(service);
  if(it==state_->services.end())throw std::runtime_error("unknown report service");
  bool permitted=false;
  for(const auto& entry:it->second->services())
    if(entry.service==service&&entry.schema==schema&&(entry.actions&4u))permitted=true;
  if(!permitted)throw std::runtime_error("report type/role mismatch");
  auto slot=std::make_unique<ReportSubscription::State>(state_->backend,state_->domain);
  const UmaaDdsTopicV1 descriptor{topic_name.c_str(),schema.c_str(),qos_profile.c_str(),
    UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
  slot->reader=slot->domain->reader(descriptor,&ReportSubscription::State::receive,slot.get());
  return std::unique_ptr<ReportSubscription>(new ReportSubscription(std::move(slot)));
}
ComponentRuntime::ComponentRuntime():state_(std::make_unique<State>()){}
ComponentRuntime::~ComponentRuntime()=default;
void ComponentRuntime::load_component(const std::string& filename){
  auto mod=std::make_shared<umaa::ComponentModule>(filename);
  for(const auto& service:mod->services())
    if(state_->services.count(service.service))
      throw std::runtime_error("duplicate service between components: "+service.service);
  auto pointer=mod;
  for(const auto& service:mod->services())state_->services.emplace(service.service,pointer);
  state_->modules.push_back(std::move(mod));
}
void ComponentRuntime::load_backend(const std::string& filename,uint32_t domain_id){
  if(state_->domain)throw std::runtime_error("DDS backend already installed");
  auto candidate=std::make_shared<umaa::DdsBackendPlugin>(filename);
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain(candidate->open_domain(domain_id));
  state_->backend=std::move(candidate);
  state_->domain=std::move(domain);
}
std::vector<std::string> ComponentRuntime::services()const{
  std::vector<std::string> out;
  for(const auto& s:state_->services)out.push_back(s.first);
  return out;
}
int32_t ComponentRuntime::call(const std::string& service_id,uint32_t action,
  const std::vector<uint8_t>& payload,std::vector<uint8_t>& output,
  int32_t& business_result,uint64_t session_id){
  const auto found=state_->services.find(service_id);
  if(found==state_->services.end())return UMAA_COMPONENT_NOT_FOUND;
  return found->second->call(service_id,action,payload,output,business_result,session_id);
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
  bool active{true},is_faulted{false};
  State(std::shared_ptr<umaa::DdsBackendPlugin> b,
        std::shared_ptr<umaa::DdsBackendPlugin::Domain> d,
        std::shared_ptr<umaa::ComponentModule> m,void* w,
        std::string s,std::string k):backend(std::move(b)),domain(std::move(d)),
        module(std::move(m)),writer(w),service(std::move(s)),key(std::move(k)){}
  ~State(){if(writer)domain->close_writer(writer);}
};
ReportPublisher::ReportPublisher(std::unique_ptr<State> state):state_(std::move(state)){}
ReportPublisher::~ReportPublisher()=default;
bool ReportPublisher::faulted() const noexcept{return state_->is_faulted;}
void ReportPublisher::update(){
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
  if(!state_->active||state_->is_faulted)throw std::logic_error("report publisher not active");
  const UmaaDdsSampleV1 sample{nullptr,0,state_->key.c_str(),next_report_timestamp(),UMAA_DDS_EVENT_DISPOSE};
  if(state_->domain->dispose(state_->writer,sample)!=UMAA_DDS_OK){
    state_->is_faulted=true;throw std::runtime_error("report dispose delivery failed");
  }
}
void ReportPublisher::close(){
  if(!state_->active)return;
  state_->active=false;
  if(!state_->is_faulted && state_->domain->wait_for_acks(state_->writer,5000)!=UMAA_DDS_OK){
    state_->is_faulted=true;throw std::runtime_error("report delivery ack timeout");
  }
  state_->domain->close_writer(state_->writer);state_->writer=nullptr;
}
std::unique_ptr<ReportPublisher> ComponentRuntime::open_report_fixture(
    const std::string& service_id,const std::string& topic_name,
    const std::string& schema_id,const std::string& instance_key){
  if(!state_->domain)throw std::runtime_error("DDS backend not loaded");
  const auto found=state_->services.find(service_id);
  if(found==state_->services.end())throw std::runtime_error("unknown report service");
  bool authorized=false;
  for(const auto& d:found->second->services())
    if(d.service==service_id&&d.schema==schema_id&&(d.actions&4u))authorized=true;
  if(!authorized)throw std::runtime_error("report schema or role mismatch");
  const UmaaDdsTopicV1 topic{topic_name.c_str(),schema_id.c_str(),"demo-reliable",
    UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
  auto writer=state_->domain->writer(topic);
  return std::unique_ptr<ReportPublisher>(new ReportPublisher(
    std::make_unique<ReportPublisher::State>(state_->backend,state_->domain,
      found->second,writer,service_id,instance_key)));
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
