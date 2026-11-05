/* Independently generated UMAA BatteryReportType; links no SDK runtime. */
#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  dds_entity_t participant=dds_create_participant(DDS_DOMAIN_DEFAULT,NULL,NULL);
  if(participant<0)return 2;
  dds_entity_t topic=dds_create_topic(participant,&UMAA_EO_BatteryStatus_BatteryReportType_desc,
                                      "BatteryReportType",NULL,NULL);
  if(topic<0){fprintf(stderr,"official subscriber topic rc=%d\n",topic);return 2;}
  dds_qos_t* q=dds_create_qos();
  dds_qset_reliability(q,DDS_RELIABILITY_RELIABLE,DDS_SECS(1));
  dds_qset_history(q,DDS_HISTORY_KEEP_LAST,16);
  dds_entity_t reader=dds_create_reader(participant,topic,q,NULL);
  dds_delete_qos(q);
  if(reader<0)return 2;
  int phase=0;
  // Loss-impaired network-namespace tests require extra time for the actual
  // keyed DDS Dispose. Keep exact 20/80/Dispose validation; do not skip it.
  const int attempts = getenv("UMAA_NETEM_RECOVERY") ? 1600 :
                       getenv("UMAA_NETEM_RETAIN_FINAL") ? 600 : 250;
  for(int i=0;i<attempts && phase<3;++i){
    UMAA_EO_BatteryStatus_BatteryReportType data={0};
    void* samples[1]={&data};
    dds_sample_info_t info[1];
    dds_return_t n=dds_take(reader,samples,info,1,1);
    if(n<0)return 3;
    if(n==0){dds_sleepfor(DDS_MSECS(25));continue;}
    if(info[0].valid_data){
      double expected=phase==0?20.0:80.0;
      if(phase>1 || !data.chargeRemaining || *data.chargeRemaining!=expected){
        fprintf(stderr,"OFFICIAL_SIM: invalid battery phase=%d\n",phase);return 4;
      }
      printf("OFFICIAL_SIM: BatteryReportType chargeRemaining=%.0f\n",*data.chargeRemaining);
      ++phase;
    } else if(info[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED){
      if(phase!=2)return 5;
      puts("OFFICIAL_SIM: DDS dispose observed");++phase;
    }
    dds_sample_free(&data,&UMAA_EO_BatteryStatus_BatteryReportType_desc,DDS_FREE_CONTENTS);
  }
  dds_delete(participant);
  if(phase!=3){fprintf(stderr,"OFFICIAL_SIM: incomplete %d/3\n",phase);return 6;}
  // Netns-only independent application observation: never emit evidence
  // after just two Report values or on DDS NO_WRITERS instead of Dispose.
  const char* receipt=getenv("UMAA_NETEM_RECEIPT_FILE");
  if(receipt && *receipt){
    FILE* out=fopen(receipt,"wb");
    if(!out){perror("DDS Dispose receipt open");return 7;}
    const int written=fputs("NATIVE_BATTERY_REPORT_DISPOSE_OBSERVED\n",out);
    const int flushed=fflush(out);
    const int closed=fclose(out);
    if(written<0 || flushed!=0 || closed!=0){
      fprintf(stderr,"OFFICIAL_SIM: DDS Dispose receipt write failed\n");
      return 7;
    }
  }
  puts("OFFICIAL_SIM: PASS real DDS with AUVSI UMAA 6.0 BatteryReportType");
  return 0;
}
