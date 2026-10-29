#include "umaa/internal/component_runtime.hpp"
#include "umaa/component_module.hpp"
#include "umaa/component.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <stdexcept>
#include <mutex>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>
namespace umaa::internal {
struct ComponentRuntime::State {
  std::vector<std::unique_ptr<umaa::ComponentModule>> modules;
  std::unordered_map<std::string, umaa::ComponentModule*> services;
  std::shared_ptr<umaa::DdsBackendPlugin> backend;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain;
};
struct ReportSubscription::State {
  std::shared_ptr<umaa::DdsBackendPlugin> backend;
  std::shared_ptr<umaa::DdsBackendPlugin::Domain> domain;
  void* reader{nullptr};
  mutable std::mutex mutex;
  std::vector<uint8_t> latest_bytes;
  std::string instance_key;
  int64_t timestamp{INT64_MIN};
  bool has_value{false};
  bool was_disposed{false};
  State(std::shared_ptr<umaa::DdsBackendPlugin> b,
        std::shared_ptr<umaa::DdsBackendPlugin::Domain> d)
    :backend(std::move(b)),domain(std::move(d)){}
  ~State(){if(reader)domain->close_reader(reader);}
  static void receive(void* ptr,const UmaaDdsSampleV1* sample) noexcept {
    if(!ptr||!sample||!sample->instance_key||
       (sample->payload_size&&!sample->payload))return;
    auto* self=static_cast<State*>(ptr);
    try {
      std::lock_guard<std::mutex> lock(self->mutex);
      if(sample->source_timestamp_ns<self->timestamp)return;
      if(sample->event_kind==UMAA_DDS_EVENT_DATA){
        self->latest_bytes.clear();
        if(sample->payload_size)
          self->latest_bytes.assign(sample->payload,sample->payload+sample->payload_size);
        self->instance_key=sample->instance_key;
        self->timestamp=sample->source_timestamp_ns;
        self->has_value=true;self->was_disposed=false;
      }else if(sample->event_kind==UMAA_DDS_EVENT_DISPOSE ||
               sample->event_kind==UMAA_DDS_EVENT_UNREGISTER){
        self->latest_bytes.clear();
        self->instance_key=sample->instance_key;
        self->timestamp=sample->source_timestamp_ns;
        self->has_value=false;self->was_disposed=true;
      }
    }catch(...){ /* No exceptions cross DDS backend callback C ABI. */ }
  }
};
ReportSubscription::ReportSubscription(std::unique_ptr<State> s):state_(std::move(s)){}
ReportSubscription::~ReportSubscription()=default;
int32_t ReportSubscription::poll(uint32_t n) {return state_->domain->poll(state_->reader,n);}
bool ReportSubscription::latest(std::vector<uint8_t>& data,std::string& key) const {
 std::lock_guard<std::mutex> lock(state_->mutex);
 if(!state_->has_value)return false;
 data=state_->latest_bytes;key=state_->instance_key;return true;
}
bool ReportSubscription::disposed() const {
 std::lock_guard<std::mutex> lock(state_->mutex);
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
  auto mod=std::make_unique<umaa::ComponentModule>(filename);
  for(const auto& service:mod->services())
    if(state_->services.count(service.service))
      throw std::runtime_error("duplicate service between components: "+service.service);
  auto* pointer=mod.get();
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
void ComponentRuntime::publish_report_fixture(const std::string& service_id,
  const std::string& topic_name,const std::string& schema_id,
  const std::string& instance_key){
  if(!state_->domain)throw std::runtime_error("DDS backend not loaded");
  const auto found=state_->services.find(service_id);
  if(found==state_->services.end())throw std::runtime_error("unknown component report service");
  bool correct_schema=false;
  for(const auto& desc:found->second->services())
    if(desc.service==service_id && desc.schema==schema_id && (desc.actions&4u))correct_schema=true;
  if(!correct_schema)throw std::runtime_error("report schema mismatch");
  std::vector<uint8_t> payload,output;
  int32_t business=-1;
  if(found->second->call(service_id,UMAA_COMPONENT_REPORT,payload,output,business)!=0 ||
     business!=static_cast<int32_t>(umaa::Result::Completed))
    throw std::runtime_error("report Handler failed");
  const UmaaDdsTopicV1 topic{topic_name.c_str(),schema_id.c_str(),"demo-reliable",
    UMAA_DDS_CAP_RELIABLE};
  auto* writer=state_->domain->writer(topic);
  try{
    UmaaDdsSampleV1 sample{output.empty()?nullptr:output.data(),
      static_cast<uint32_t>(output.size()),instance_key.c_str(),1,UMAA_DDS_EVENT_DATA};
    if(state_->domain->write(writer,sample)!=UMAA_DDS_OK ||
      state_->domain->wait_for_acks(writer,1000)!=UMAA_DDS_OK)
      throw std::runtime_error("failed to publish fixture report");
    state_->domain->close_writer(writer);
  }catch(...){state_->domain->close_writer(writer);throw;}
}
} // namespace umaa::internal
