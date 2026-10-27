// Runtime registration test for two *additional* official UMAA 6.0 services.
// Verifies reviewed Contract/Role -> DDS Topic -> Reader/Writer discovery.
// This is NOT application payload, status, Ack, keyed Dispose or QoS proof.
#include <dds/dds.h>
#include "umaa/cyclone_service_context.hpp"
#include <cstdio>
#include <type_traits>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

#if defined(UMAA_ANCHOR_SPECS_REPORT)
#include "UMAA/EO/AnchorSpecs/AnchorSpecsReportType.h"
static constexpr auto SERVICE = "UMAA::EO::AnchorSpecs";
static constexpr auto TOPIC = "UMAA::EO::AnchorSpecs::AnchorSpecsReportType";
static constexpr auto WIRE = "AnchorSpecsReportType";
static constexpr auto DESC = &UMAA_EO_AnchorSpecs_AnchorSpecsReportType_desc;
using Wire=UMAA_EO_AnchorSpecs_AnchorSpecsReportType;
static constexpr auto ROLE = umaa::reviewed::Role::Report;
static constexpr bool PROVIDER = true;
#elif defined(UMAA_ANCHOR_CONTROL_COMMAND)
#include "UMAA/EO/AnchorControl/AnchorCommandType.h"
static constexpr auto SERVICE = "UMAA::EO::AnchorControl";
static constexpr auto TOPIC = "UMAA::EO::AnchorControl::AnchorCommandType";
static constexpr auto WIRE = "AnchorCommandType";
static constexpr auto DESC = &UMAA_EO_AnchorControl_AnchorCommandType_desc;
using Wire=UMAA_EO_AnchorControl_AnchorCommandType;
static constexpr auto ROLE = umaa::reviewed::Role::Command;
static constexpr bool PROVIDER = false;
#else
#error "Exactly one official service probe must be selected"
#endif

static void check(dds_return_t result, const char* operation) {
  if (result < 0) throw std::runtime_error(std::string(operation) + ": " + dds_strretcode(-result));
}

// A prototype anchor specification has a real, non-default identity and
// physical limit. Numeric measure types are populated only where the official
// C TypeSupport exposes a scalar; incompatible IDL must fail at compile-time.
template<class V> void set_measure(V& value, double amount) {
  if constexpr (std::is_arithmetic_v<V>) value=static_cast<V>(amount);
  else value.value=amount; // compile-time contract check: never silently zero
}
template<class V> double get_measure(const V& value) {
  if constexpr (std::is_arithmetic_v<V>) return static_cast<double>(value);
  else return static_cast<double>(value.value);
}
#if defined(UMAA_ANCHOR_SPECS_REPORT)
static void sample_fill(Wire& sample, int iteration) {
  // Use two genuinely different anchor specifications; never claim normative
  // units without the corresponding Component Definitions review.
  sample.timeStamp.seconds=1700000000 + iteration;
  sample.timeStamp.nanoseconds=0;
  sample.source.id[0]=0x75;
  sample.source.id[6]=0x40;
  sample.source.id[8]=0x80;
  sample.source.id[15]=0x75;
  set_measure(sample.anchorHoldingPower, iteration==0 ? 1500.0 : 1750.0);
  set_measure(sample.rodeLength, iteration==0 ? 35.0 : 42.0);
}
#endif

int main(int argc, char** argv) {
  const bool semantic = argc == 2 && std::strcmp(argv[1], "--semantic") == 0;
  const bool wire = (argc == 2 && std::strcmp(argv[1], "--wire") == 0) || semantic;
  if (argc > 2 || (argc == 2 && !wire)) return 3;
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
    if(wire) {
      if(PROVIDER) {
        // A default-initialized official TypeSupport sample exercises the
        // real serializer only. It is NOT an application-semantic Report.
        Wire sample{};
#if defined(UMAA_ANCHOR_SPECS_REPORT)
        if(semantic) {
          for(int iteration=0;iteration<2;++iteration) {
            Wire valued{};
            sample_fill(valued,iteration);
            if(get_measure(valued.anchorHoldingPower)<1000.0 ||
               get_measure(valued.rodeLength)<30.0)
              throw std::runtime_error("invalid semantic AnchorSpecs sample");
            check(dds_write(endpoint,&valued),"write semantic AnchorSpecs Report");
            check(dds_wait_for_acks(endpoint,DDS_SECS(3)),"ACK semantic AnchorSpecs Report");
            dds_sleepfor(DDS_MSECS(250));
          }
          Wire disposed{};
          sample_fill(disposed,1);
          check(dds_dispose(endpoint,&disposed),"dispose semantic AnchorSpecs Report");
          check(dds_wait_for_acks(endpoint,DDS_SECS(3)),"ACK AnchorSpecs Dispose");
          std::puts("ANCHOR_SPECS_SEMANTIC_HOST_PASS two_nondefault_values_and_dispose");
        } else
#endif
        {
          check(dds_write(endpoint,&sample),"write zero-valued TypeSupport sample");
          check(dds_wait_for_acks(endpoint,DDS_SECS(3)),"wait for sample ACK");
        }
      }else{
        // Receive the independent Command publisher sample, without treating
        // a zero/default payload as a valid executable hardware command.
        bool got=false;
        for(int i=0;i<250 && !got;++i){
          Wire sample{};void* samples[1]={&sample};dds_sample_info_t info[1]{};
          const dds_return_t count=dds_take(endpoint,samples,info,1,1);
          check(count,"take official Command TypeSupport sample");
          if(count>0){
            got=info[0].valid_data;
            dds_sample_free(&sample,DESC,DDS_FREE_CONTENTS);
          }
          if(!got)dds_sleepfor(DDS_MSECS(25));
        }
        if(!got)throw std::runtime_error("no independent typed Command sample");
      }
      std::puts("REVIEWED_SERVICE_TYPED_WIRE_HOST_PASS only=default_type_serialization");
    }
    dds_sleepfor(DDS_MSECS(semantic?2000:500));
    context.close();
    std::printf("REVIEWED_SERVICE_DISCOVERY_HOST_PASS service=%s role=%s only=discovery\n",
                SERVICE, PROVIDER?"ReportProvider":"CommandConsumer");
    return 0;
  }catch(const std::exception& e){
    std::fprintf(stderr,"REVIEWED_SERVICE_DISCOVERY_HOST_FAIL %s\n",e.what());return 2;
  }
}
