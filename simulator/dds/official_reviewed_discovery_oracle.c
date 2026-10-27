/* Independent C DDS discovery oracle. No UMAA SDK dependencies.
 * Role is inverse of application: Report consumer / Command producer.
 * A matched endpoint is NOT a protocol-compliant message exchange. */
#include <dds/dds.h>
#include <stdio.h>

#if defined(UMAA_ANCHOR_SPECS_REPORT)
#include "UMAA/EO/AnchorSpecs/AnchorSpecsReportType.h"
#define TYPE_DESC (&UMAA_EO_AnchorSpecs_AnchorSpecsReportType_desc)
#define WIRE_TOPIC "AnchorSpecsReportType"
#define SERVICE "UMAA::EO::AnchorSpecs"
#define SIM_PUBLISHER 0
#elif defined(UMAA_ANCHOR_CONTROL_COMMAND)
#include "UMAA/EO/AnchorControl/AnchorCommandType.h"
#define TYPE_DESC (&UMAA_EO_AnchorControl_AnchorCommandType_desc)
#define WIRE_TOPIC "AnchorCommandType"
#define SERVICE "UMAA::EO::AnchorControl"
#define SIM_PUBLISHER 1
#else
#error "Pick official service probe"
#endif

int main(void){
  dds_entity_t participant=dds_create_participant(DDS_DOMAIN_DEFAULT,NULL,NULL);
  if(participant<0)return 2;
  dds_entity_t topic=dds_create_topic(participant,TYPE_DESC,WIRE_TOPIC,NULL,NULL);
  if(topic<0)return 3;
  dds_qos_t* qos=dds_create_qos();
  if(!qos)return 4;
  dds_qset_reliability(qos,DDS_RELIABILITY_RELIABLE,DDS_SECS(1));
  dds_entity_t endpoint=SIM_PUBLISHER
    ? dds_create_writer(participant,topic,qos,NULL)
    : dds_create_reader(participant,topic,qos,NULL);
  dds_delete_qos(qos);
  if(endpoint<0)return 5;
  int matched=0;
  for(int i=0;i<300 && !matched;++i){
    if(SIM_PUBLISHER){
      dds_publication_matched_status_t status={0};
      if(dds_get_publication_matched_status(endpoint,&status)<0)return 6;
      matched=status.current_count>0;
    }else{
      dds_subscription_matched_status_t status={0};
      if(dds_get_subscription_matched_status(endpoint,&status)<0)return 7;
      matched=status.current_count>0;
    }
    if(!matched)dds_sleepfor(DDS_MSECS(25));
  }
  if(!matched){fprintf(stderr,"REVIEWED_SERVICE_DISCOVERY_SIM_FAIL %s\n",SERVICE);return 8;}
  dds_sleepfor(DDS_MSECS(500)); /* keep endpoint alive for peer observation */
  puts("REVIEWED_SERVICE_DISCOVERY_SIM_PASS only=discovery");
  return dds_delete(participant)<0?9:0;
}
