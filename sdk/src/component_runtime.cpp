#include "umaa/internal/component_runtime.hpp"
#include "umaa/component_module.hpp"
#include "umaa/component.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
namespace umaa::internal {
struct ComponentRuntime::State {
  std::vector<std::unique_ptr<umaa::ComponentModule>> modules;
  std::unordered_map<std::string, umaa::ComponentModule*> services;
  std::unique_ptr<umaa::DdsBackendPlugin> backend;
  std::unique_ptr<umaa::DdsBackendPlugin::Domain> domain;
};
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
  auto candidate=std::make_unique<umaa::DdsBackendPlugin>(filename);
  auto domain=candidate->open_domain(domain_id);
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
