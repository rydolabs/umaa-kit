#include "umaa/cyclone_service_context.hpp"
#include <cassert>
#include <exception>
#include <string>
#include <vector>

struct dds_topic_descriptor {};
static dds_topic_descriptor descriptor{};
static std::vector<std::string> ops;
static bool fail_participant=false,fail_topic=false,fail_delete=false;
extern "C" dds_entity_t dds_create_participant(uint32_t domain,const dds_qos_t*,const void*) {
  assert(domain==DDS_DOMAIN_DEFAULT);
  ops.push_back("participant");
  return fail_participant ? -1 : 10;
}
extern "C" dds_entity_t dds_create_topic(dds_entity_t participant,const dds_topic_descriptor_t* desc,
                                    const char* name,const dds_qos_t*,const void*) {
  assert(participant==10 && desc==&descriptor && name);
  ops.emplace_back(std::string("topic:")+name);
  return fail_topic ? -2 : 11;
}
extern "C" dds_return_t dds_delete(dds_entity_t entity) {
  assert(entity==10);
  ops.push_back("delete");
  return fail_delete?-3:0;
}
extern "C" const char* dds_strretcode(int32_t) {return "injected DDS error";}
template<class F> void rejects(F&& f) {
  bool caught=false;
  try { f(); } catch (const std::exception&) { caught=true; }
  assert(caught);
}
int main(){
  using umaa::cyclone::ServiceContext;
  using umaa::reviewed::Role;
  constexpr auto service="UMAA::EO::EngineControl";
  constexpr auto command="UMAA::EO::EngineControl::EngineCommandType";
  rejects([&]{ ServiceContext context(""); });
  assert(ops.empty());
  fail_participant=true;
  rejects([&]{ ServiceContext context(service); });
  fail_participant=false;
  ops.clear();
  {
    ServiceContext context(service);
    rejects([&]{ context.topic(command,command,Role::Command,"EngineCommandType",&descriptor); });
    assert(ops.size()==1);  // fail closed: no DDS Topic created
  }
  assert((ops==std::vector<std::string>{"participant","delete"}));
  ops.clear();
  {
    ServiceContext context(service,ServiceContext::ReviewMode::ExplicitPrototypeOnly);
    rejects([&]{ context.topic(command,command,Role::Report,"EngineCommandType",&descriptor); });
    rejects([&]{ context.topic(command,"Incorrect::Type",Role::Command,"EngineCommandType",&descriptor); });
    rejects([&]{ context.topic(command,command,Role::Command,"",&descriptor); });
    rejects([&]{ context.topic(command,command,Role::Command,"EngineCommandType",nullptr); });
    assert(ops.size()==1);
    assert(context.topic(command,command,Role::Command,"EngineCommandType",&descriptor)==11);
    fail_topic=true;
    rejects([&]{ context.topic(command,command,Role::Command,"EngineCommandType",&descriptor); });
    fail_topic=false;
    context.close();
    rejects([&]{ context.topic(command,command,Role::Command,"EngineCommandType",&descriptor); });
  }
  assert(ops.back()=="delete");
  ops.clear();
  {
    ServiceContext context(service,ServiceContext::ReviewMode::ExplicitPrototypeOnly);
    fail_delete=true;
    rejects([&]{ context.close(); });
    fail_delete=false; // destructor retries cleanup only as last resort
  }
  assert(ops.size()==3);
}
