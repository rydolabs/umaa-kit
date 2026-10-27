// Runtime registration test for two *additional* official UMAA 6.0 services.
// Verifies reviewed Contract/Role -> DDS Topic -> Reader/Writer discovery.
// This is NOT application payload, status, Ack, keyed Dispose or QoS proof.
#include <dds/dds.h>
#include "umaa/cyclone_service_context.hpp"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string>

#if defined(UMAA_ANCHOR_SPECS_REPORT)
#include "UMAA/EO/AnchorSpecs/AnchorSpecsReportType.h"
static constexpr auto SERVICE = "UMAA::EO::AnchorSpecs";
static constexpr auto TOPIC = "UMAA::EO::AnchorSpecs::AnchorSpecsReportType";
static constexpr auto WIRE = "AnchorSpecsReportType";
static constexpr auto DESC = &UMAA_EO_AnchorSpecs_AnchorSpecsReportType_desc;
static constexpr auto ROLE = umaa::reviewed::Role::Report;
static constexpr bool PROVIDER = true;
#elif defined(UMAA_ANCHOR_CONTROL_COMMAND)
#include "UMAA/EO/AnchorControl/AnchorCommandType.h"
static constexpr auto SERVICE = "UMAA::EO::AnchorControl";
static constexpr auto TOPIC = "UMAA::EO::AnchorControl::AnchorCommandType";
static constexpr auto WIRE = "AnchorCommandType";
static constexpr auto DESC = &UMAA_EO_AnchorControl_AnchorCommandType_desc;
static constexpr auto ROLE = umaa::reviewed::Role::Command;
static constexpr bool PROVIDER = false;
#else
#error "Exactly one official service probe must be selected"
#endif

static void check(dds_return_t result, const char* operation) {
  if (result < 0) throw std::runtime_error(std::string(operation) + ": " + dds_strretcode(-result));
}

int main() {
  try {
    using Context = umaa::cyclone::ServiceContext;
    Context context(SERVICE, Context::ReviewMode::ExplicitPrototypeOnly);
    const dds_entity_t topic=context.topic(TOPIC,TOPIC,ROLE,WIRE,DESC);
    dds_qos_t* qos=dds_create_qos();
    if (!qos) throw std::runtime_error("cannot allocate QoS");
    dds_qset_reliability(qos,DDS_RELIABILITY_RELIABLE,DDS_SECS(1));
    const dds_entity_t endpoint= PROVIDER
      ? dds_create_writer(context.participant(),topic,qos,nullptr)
      : dds_create_reader(context.participant(),topic,qos,nullptr);
    dds_delete_qos(qos);
    check(endpoint,"create reviewed service endpoint");
    bool matched=false;
    for(int i=0;i<300;++i) {
      if(PROVIDER) {
        dds_publication_matched_status_t status{};
        check(dds_get_publication_matched_status(endpoint,&status),"publication matched");
        matched=status.current_count>0;
      } else {
        dds_subscription_matched_status_t status{};
        check(dds_get_subscription_matched_status(endpoint,&status),"subscription matched");
        matched=status.current_count>0;
      }
      if(matched) break;
      dds_sleepfor(DDS_MSECS(25));
    }
    if(!matched) throw std::runtime_error("independent participant not discovered");
    // The application-side endpoint must stay up while the independent oracle
    // detects matching discovery. No payload is exchanged in this phase.
    dds_sleepfor(DDS_MSECS(500));
    context.close();
    std::printf("REVIEWED_SERVICE_DISCOVERY_HOST_PASS service=%s role=%s only=discovery\n",
                SERVICE, PROVIDER?"ReportProvider":"CommandConsumer");
    return 0;
  }catch(const std::exception& e){
    std::fprintf(stderr,"REVIEWED_SERVICE_DISCOVERY_HOST_FAIL %s\n",e.what());return 2;
  }
}
