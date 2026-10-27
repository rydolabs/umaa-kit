/* Independent real DDS AnchorSpecs semantic observer (no SDK dependencies). */
#include <dds/dds.h>
#include "UMAA/EO/AnchorSpecs/AnchorSpecsReportType.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef UMAA_EO_AnchorSpecs_AnchorSpecsReportType Sample;
static int approx_equal(double a,double b){return isfinite(a)&&fabs(a-b)<0.0002;}
static int verify(const Sample* s,int phase){
 const int newer=phase==1;
 if(strcmp(s->anchorDescription,
    newer?"Proof anchor revised":"Proof bow anchor")!=0) return 0;
 return s->source.id[0]==0x45 && s->source.id[15]==0x45 &&
    s->timeStamp.seconds>0 && s->timeStamp.nanoseconds==(newer?2000:1000) &&
    approx_equal(s->anchorSize,newer?175:125) &&
    approx_equal(s->anchorHoldingPower,newer?2900:2150) &&
    approx_equal(s->anchorHoldingPowerRatio,newer?16.571428:17.2) &&
    approx_equal(s->rodeLength,newer?115:85.5) && approx_equal(s->rodeSize,0.016) &&
    approx_equal(s->rodeWorkingLoadLimit,newer?1600:1400);
}
int main(void){
 dds_entity_t participant=dds_create_participant(DDS_DOMAIN_DEFAULT,NULL,NULL);
 if(participant<0)return 2;
 dds_entity_t topic=dds_create_topic(participant,
   &UMAA_EO_AnchorSpecs_AnchorSpecsReportType_desc,"AnchorSpecsReportType",NULL,NULL);
 if(topic<0)return 3;
 dds_qos_t* qos=dds_create_qos();if(!qos)return 4;
 dds_qset_reliability(qos,DDS_RELIABILITY_RELIABLE,DDS_SECS(1));
 dds_qset_history(qos,DDS_HISTORY_KEEP_ALL,0);
 dds_entity_t reader=dds_create_reader(participant,topic,qos,NULL);
 dds_delete_qos(qos);if(reader<0)return 5;
 int received=0,disposed=0;
 for(int ticks=0;ticks<700 && !(received==2 && disposed);++ticks){
  Sample sample={0};void* data[1]={&sample};dds_sample_info_t info[1]={{0}};
  const dds_return_t n=dds_take(reader,data,info,1,1);
  if(n<0){fprintf(stderr,"ANCHOR_SPECS_ORACLE_FAIL take=%d\n",n);return 6;}
  if(n>0){
   if(info[0].valid_data){
    if(received>=2 || !verify(&sample,received)){
     fprintf(stderr,"ANCHOR_SPECS_ORACLE_FAIL semantic value phase=%d\n",received);
     return 7;
    }
    ++received;
   }
   if(info[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED){
    if(received!=2){fputs("ANCHOR_SPECS_ORACLE_FAIL early Dispose\n",stderr);return 8;}
    disposed=1;
   }
   dds_sample_free(&sample,&UMAA_EO_AnchorSpecs_AnchorSpecsReportType_desc,DDS_FREE_CONTENTS);
  }
  if(!(received==2 && disposed))dds_sleepfor(DDS_MSECS(25));
 }
 if(received!=2 || !disposed){
   fprintf(stderr,"ANCHOR_SPECS_ORACLE_FAIL expected 2 and Dispose actual=%d,%d\n",received,disposed);
   return 9;
 }
 if(dds_delete(participant)<0)return 10;
 puts("ANCHOR_SPECS_ORACLE_PASS validated_anchor_size_holding_power_rode_length_description update=1 dispose=1");
 return 0;
}
