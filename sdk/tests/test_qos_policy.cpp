#ifdef NDEBUG
#undef NDEBUG
#endif
#include "umaa/qos_policy.hpp"
#include "umaa/generated/umaa6_qos_requirements.hpp"
#include "umaa/dds_backend_loader.hpp"
#include <cassert>
#include <stdexcept>
#include <string>
#include <iostream>
namespace {
template <typename Fn> void must_fail(Fn&& fn) {
 bool thrown=false;
 try {fn();}catch(const std::exception&){thrown=true;}
 assert(thrown);
}
}
int main(int argc,char** argv){
 using namespace umaa::qos;
 Policy settings;
 const auto normative=umaa::generated::reviewed_qos_requirements(
     "UMAA::EO::BatteryStatus",
     "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement");
 assert(normative && normative->require_ordered_access);
 assert(normative->presentation==Presentation::topic);
 must_fail([&]{validate(settings,*normative);});
 auto ordered=settings;ordered.presentation=Presentation::topic;
 ordered.ordered_access=true;
 validate(ordered,*normative);
 assert(!umaa::generated::reviewed_qos_requirements(
     "UMAA::EO::EngineControl","UMAA::EO::EngineControl::EngineCommandType"));
 validate(settings);
 assert(settings.depth==16 && required_capabilities(settings)==UMAA_DDS_CAP_RELIABLE);
 Requirements reviewed;
 reviewed.reliability=Reliability::reliable;
 reviewed.minimum_history_depth=8;
 validate(settings,reviewed);
 reviewed.require_ordered_access=true;
 must_fail([&]{validate(settings,reviewed);});
 settings.presentation=Presentation::topic;settings.ordered_access=true;
 validate(settings,reviewed);
 assert(required_capabilities(settings)==(UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_ORDERED_ACCESS));
 reviewed.presentation=Presentation::topic;
 validate(settings,reviewed);
 settings.presentation=Presentation::instance;
 must_fail([&]{validate(settings);});
 settings.presentation=Presentation::topic;
 settings.depth=7;
 must_fail([&]{validate(settings,reviewed);});
 settings.depth=16;
 settings.history=History::keep_all;settings.depth=16;
 must_fail([&]{validate(settings);});
 settings.depth=0;
 validate(settings,reviewed);
 settings.partition=std::string("safe");
 validate(settings);
 settings.partition=std::string("unsafe\0partition",16);
 must_fail([&]{validate(settings);});
 settings.partition="safe";
 auto raw=to_abi(settings);
 assert(raw.struct_size==sizeof(UmaaDdsQosV2) && raw.partition==settings.partition.c_str());
 Policy writer;Policy reader;
 writer.reliability=Reliability::best_effort;
 must_fail([&]{validate_compatible(writer,reader);});
 writer.reliability=Reliability::reliable;
 writer.partition="other";
 must_fail([&]{validate_compatible(writer,reader);});
 writer.partition.clear();
 validate_compatible(writer,reader);
 if(argc==2){
  umaa::DdsBackendPlugin old(argv[1]);
  assert(!old.has_qos_extension());
  auto domain=old.open_domain(42);
  const UmaaDdsTopicV1 topic{"demo/BatteryStatus","demo:battery-v1",
                             "demo-reliable",UMAA_DDS_CAP_RELIABLE};
  must_fail([&]{auto* w=domain->writer(topic,writer);domain->close_writer(w);});
  UmaaDdsTopicV1 requires_ordered=topic;
  requires_ordered.required_features|=UMAA_DDS_CAP_ORDERED_ACCESS;
  must_fail([&]{auto* w=domain->writer(requires_ordered,writer);domain->close_writer(w);});
 }
 std::cout<<"PASS shared QoS contract, constraints, compatibility and ABI isolation\n";
}
