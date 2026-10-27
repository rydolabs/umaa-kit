/* Independent C DDS discovery oracle. No UMAA SDK dependencies.
 * Role is inverse of application: Report consumer / Command producer.
 * A matched endpoint is NOT a protocol-compliant message exchange. */
#include <dds/dds.h>
#include <stdio.h>
#include <string.h>

#if defined(UMAA_ANCHOR_SPECS_REPORT)
#include "UMAA/EO/AnchorSpecs/AnchorSpecsReportType.h"
#define TYPE_DESC (&UMAA_EO_AnchorSpecs_AnchorSpecsReportType_desc)
#define WIRE_TOPIC "AnchorSpecsReportType"
#define SERVICE "UMAA::EO::AnchorSpecs"
#define WIRE_SAMPLE UMAA_EO_AnchorSpecs_AnchorSpecsReportType
#define SIM_PUBLISHER 0
#elif defined(UMAA_ANCHOR_CONTROL_COMMAND)
#include "UMAA/EO/AnchorControl/AnchorCommandType.h"
#define TYPE_DESC (&UMAA_EO_AnchorControl_AnchorCommandType_desc)
#define WIRE_TOPIC "AnchorCommandType"
#define SERVICE "UMAA::EO::AnchorControl"
#define WIRE_SAMPLE UMAA_EO_AnchorControl_AnchorCommandType
#define SIM_PUBLISHER 1
#else
#error "Pick official service probe"
#endif

int main(int argc,char** argv){
  int semantic=argc==2 && strcmp(argv[1],"--semantic")==0;
  int wire=(argc==2 && strcmp(argv[1],"--wire")==0)||semantic;
  if(argc>2 || (argc==2 && !wire))return 11;
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
  if(wire){
    if(SIM_PUBLISHER){
      WIRE_SAMPLE sample={0};
      if(dds_write(endpoint,&sample)<0)return 12;
      if(dds_wait_for_acks(endpoint,DDS_SECS(3))<0)return 13;
    }else{
      int got=0;
      int disposed=0;
      for(int i=0;i<280 && (!semantic ? !got : (got<2||!disposed));++i){
        WIRE_SAMPLE sample={0};void* samples[1]={&sample};dds_sample_info_t info[1]={{0}};
        dds_return_t n=dds_take(endpoint,samples,info,1,1);
        if(n<0)return 14;
        if(n>0){
          if(info[0].valid_data){
#if defined(UMAA_ANCHOR_SPECS_REPORT)
            if(semantic){
              const double holding=sample.anchorHoldingPower;
              const double rode=sample.rodeLength;
              const double expected_holding=(got==0?1500.0:1750.0);
              const double expected_rode=(got==0?35.0:42.0);
              if(got>=2 || holding!=expected_holding || rode!=expected_rode ||
                 sample.timeStamp.seconds!=1700000000+got ||
                 sample.source.id[0]!=0x75 || sample.source.id[15]!=0x75){
                fprintf(stderr,"ANCHOR_SPECS_ORACLE_BAD_VALUES step=%d holding=%f rode=%f\n",got,holding,rode);
                return 25;
              }
            }
#endif
            ++got;
            if(semantic && info[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED)
              disposed=1;
          } else if(semantic && info[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED && got==2)
            disposed=1;
          dds_sample_free(&sample,TYPE_DESC,DDS_FREE_CONTENTS);
        }
        if(!semantic && got) break;
        dds_sleepfor(DDS_MSECS(25));
      }
      if(!got || (semantic && (got!=2||!disposed))) {
        fprintf(stderr,"ANCHOR_SPECS_ORACLE_INCOMPLETE data=%d disposed=%d\n",got,disposed);
        return 15;
      }
#if defined(UMAA_ANCHOR_SPECS_REPORT)
      if(semantic)puts("ANCHOR_SPECS_SEMANTIC_SIM_PASS two_meaningful_reports_and_dispose");
#endif
    }
    puts("REVIEWED_SERVICE_TYPED_WIRE_SIM_PASS only=default_type_serialization");
  }
  dds_sleepfor(DDS_MSECS(500)); /* keep endpoint alive for peer observation */
  puts("REVIEWED_SERVICE_DISCOVERY_SIM_PASS only=discovery");
  return dds_delete(participant)<0?9:0;
}
