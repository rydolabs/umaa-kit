/* Independent UMAA AnchorControl real DDS scenario client/verifier.
 * Uses only Cyclone-generated official IDL types and DDS C. Does not link
 * the SDK CommandEngine, module loader or shared protocol verification code.
 */
#include <dds/dds.h>
#include "UMAA/EO/AnchorControl/AnchorCommandType.h"
#include "UMAA/EO/AnchorControl/AnchorCommandStatusType.h"
#include "UMAA/EO/AnchorControl/AnchorCommandAckReportType.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef UMAA_EO_AnchorControl_AnchorCommandType Cmd;
typedef UMAA_EO_AnchorControl_AnchorCommandStatusType Status;
typedef UMAA_EO_AnchorControl_AnchorCommandAckReportType Ack;

static void fillid(uint8_t id[16],uint8_t seed) {
 memset(id,0,16);id[0]=seed;id[6]=0x40;id[8]=0x80;id[15]=seed;
}
static int same(const uint8_t a[16],const uint8_t b[16]) {return memcmp(a,b,16)==0;}
static dds_qos_t* test_qos(void){
 dds_qos_t* q=dds_create_qos();
 dds_qset_reliability(q,DDS_RELIABILITY_RELIABLE,DDS_SECS(1));
 dds_qset_history(q,DDS_HISTORY_KEEP_ALL,0);
 return q;
}
static int matched(dds_entity_t writer,dds_entity_t status_reader,dds_entity_t ack_reader){
 const int attempts = getenv("UMAA_NETEM_RECOVERY") ? 1600 : 250;
 for(int tick=0;tick<attempts;++tick){
   dds_publication_matched_status_t publication={0};
   dds_subscription_matched_status_t status={0},ack={0};
   if(dds_get_publication_matched_status(writer,&publication)<0 ||
      dds_get_subscription_matched_status(status_reader,&status)<0 ||
      dds_get_subscription_matched_status(ack_reader,&ack)<0)return 0;
   if(publication.current_count>0 && status.current_count>0 && ack.current_count>0)return 1;
   dds_sleepfor(DDS_MSECS(25));
 }
 return 0;
}
int main(int argc,char** argv){
 const int updating=(argc==2 && strcmp(argv[1],"--update")==0);
 const int raising=(argc==2 && strcmp(argv[1],"--raise")==0);
 const int bad_destination=(argc==2 && strcmp(argv[1],"--bad-destination")==0);
 if(argc>2 || (argc==2 && !updating && !bad_destination && !raising))return 2;
 dds_entity_t p=dds_create_participant(DDS_DOMAIN_DEFAULT,NULL,NULL);
 if(p<0)return 2;
 dds_entity_t ct=dds_create_topic(p,&UMAA_EO_AnchorControl_AnchorCommandType_desc,"AnchorCommandType",NULL,NULL);
 dds_entity_t st=dds_create_topic(p,&UMAA_EO_AnchorControl_AnchorCommandStatusType_desc,"AnchorCommandStatusType",NULL,NULL);
 dds_entity_t at=dds_create_topic(p,&UMAA_EO_AnchorControl_AnchorCommandAckReportType_desc,"AnchorCommandAckReportType",NULL,NULL);
 if(ct<0||st<0||at<0){fprintf(stderr,"ANCHOR_CONTROL_ORACLE: topic failure: %d,%d,%d\n",ct,st,at);return 2;}
 dds_qos_t* q=test_qos();
 dds_entity_t command_writer=dds_create_writer(p,ct,q,NULL);
 dds_entity_t status_reader=dds_create_reader(p,st,q,NULL);
 dds_entity_t ack_reader=dds_create_reader(p,at,q,NULL);
 dds_delete_qos(q);
 if(command_writer<0||status_reader<0||ack_reader<0)return 3;
 if(!matched(command_writer,status_reader,ack_reader)){fputs("ANCHOR_CONTROL_ORACLE: host discovery timeout\n",stderr);return 4;}
 Cmd command={0};
 command.action=raising?UMAA_Common_MaritimeEnumeration_AnchorActionEnumModule_RAISE:
  UMAA_Common_MaritimeEnumeration_AnchorActionEnumModule_LOWER;
 command.timeStamp.seconds=time(NULL);
 command.timeStamp.nanoseconds=1;
 fillid(command.source.id,0x11);
 fillid(command.destination.id,bad_destination?0x99:0x52);
 fillid(command.sessionID,0x33);
 if(dds_write(command_writer,&command)<0){fputs("ANCHOR_CONTROL_ORACLE: issue command failed\n",stderr);return 5;}
 puts(raising?"ANCHOR_CONTROL_ORACLE: issued RAISE anchor command":
              "ANCHOR_CONTROL_ORACLE: issued LOWER anchor command");
 if(bad_destination){
   // An independently constructed wrong-destination Command must not result
   // in any Status or Ack. The supervisor separately checks that the host
   // exits with its explicit destination-rejection error.
   for(int ticks=0;ticks<80;++ticks){
     Status status={0};void* status_sample[1]={&status};dds_sample_info_t si[1]={{0}};
     dds_return_t n=dds_take(status_reader,status_sample,si,1,1);
     if(n<0)return 16;
     if(n>0){
       const int unexpected=si[0].valid_data;
       dds_sample_free(&status,&UMAA_EO_AnchorControl_AnchorCommandStatusType_desc,DDS_FREE_CONTENTS);
       if(unexpected){fputs("ANCHOR_CONTROL_ORACLE: rejected Command produced Status\n",stderr);return 17;}
     }
     Ack ack={0};void* ack_sample[1]={&ack};dds_sample_info_t ai[1]={{0}};
     n=dds_take(ack_reader,ack_sample,ai,1,1);
     if(n<0)return 18;
     if(n>0){
       const int unexpected=ai[0].valid_data;
       dds_sample_free(&ack,&UMAA_EO_AnchorControl_AnchorCommandAckReportType_desc,DDS_FREE_CONTENTS);
       if(unexpected){fputs("ANCHOR_CONTROL_ORACLE: rejected Command produced Ack\n",stderr);return 19;}
     }
     dds_sleepfor(DDS_MSECS(25));
   }
   if(dds_delete(p)<0)return 20;
   puts("ANCHOR_CONTROL_ORACLE: PASS invalid destination with no Status/Ack");
   return 0;
 }
 const UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_CommandStatusEnumType states[7]={
 UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_ISSUED,
 UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_COMMANDED,
 UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_EXECUTING,
 UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_ISSUED,
 UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_COMMANDED,
 UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_EXECUTING,
 UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_CANCELED
 };
 const char* labels[7]={"ISSUED","COMMANDED","EXECUTING","ISSUED_UPDATED","COMMANDED","EXECUTING","CANCELED"};
 const int expected=updating?7:4;
 int nstate=0;int got_ack=0;int sent_cancel=0;int sent_update=0;int disposed_status=0;int disposed_ack=0;
 for(int ticks=0;ticks<650;++ticks){
   Status s={0};void* sample[1]={&s};dds_sample_info_t info[1]={{0}};
   dds_return_t n=dds_take(status_reader,sample,info,1,1);
   if(n<0)return 6;
   if(n>0){
     if(info[0].valid_data){
       if(nstate>=expected || !same(s.sessionID,command.sessionID) ||
          s.commandStatus!=states[updating?nstate:(nstate==3?6:nstate)] ||
          s.commandStatusReason != (nstate==expected-1?
          UMAA_Common_MaritimeEnumeration_CommandStatusReasonEnumModule_CANCELED:
          (updating && nstate==3?
          UMAA_Common_MaritimeEnumeration_CommandStatusReasonEnumModule_UPDATED:
          UMAA_Common_MaritimeEnumeration_CommandStatusReasonEnumModule_SUCCEEDED))) {
         fprintf(stderr,"ANCHOR_CONTROL_ORACLE: bad status, phase=%d got=%d reason=%d\n",
                 nstate,(int)s.commandStatus,(int)s.commandStatusReason);
         return 7;
       }
       printf("ANCHOR_CONTROL_ORACLE: status=%s state=%d\n",labels[updating?nstate:(nstate==3?6:nstate)],(int)info[0].instance_state);++nstate;
       // DDS may attach DISPOSED instance state to the last valid sample
       // instead of yielding a separate invalid-data placeholder.
       if(nstate==expected && info[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED){
          disposed_status=1; puts("ANCHOR_CONTROL_ORACLE: status Dispose observed with terminal data");
       }
     }else if(info[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED){
       if(nstate!=expected){fputs("ANCHOR_CONTROL_ORACLE: status disposed before CANCELED\n",stderr);return 8;}
       disposed_status=1;puts("ANCHOR_CONTROL_ORACLE: status Dispose observed");
     } else {
       fprintf(stderr,"ANCHOR_CONTROL_ORACLE: status invalid-data instance_state=%d phase=%d\n",(int)info[0].instance_state,nstate);
     }
     dds_sample_free(&s,&UMAA_EO_AnchorControl_AnchorCommandStatusType_desc,DDS_FREE_CONTENTS);
   }
   Ack ack={0};void* ack_sample[1]={&ack};dds_sample_info_t ai[1]={{0}};
   n=dds_take(ack_reader,ack_sample,ai,1,1);
   if(n<0)return 9;
   if(n>0){
     if(ai[0].valid_data){
       if(got_ack >= (updating?2:1) || !same(ack.sessionID,command.sessionID) ||
          !same(ack.command.sessionID,command.sessionID) ||
          ack.command.action!=(got_ack==0?
             (raising?UMAA_Common_MaritimeEnumeration_AnchorActionEnumModule_RAISE:
                      UMAA_Common_MaritimeEnumeration_AnchorActionEnumModule_LOWER):
             UMAA_Common_MaritimeEnumeration_AnchorActionEnumModule_STOP)){
          fputs("ANCHOR_CONTROL_ORACLE: malformed or duplicate Ack\n",stderr);return 10;
       }
       ++got_ack;puts("ANCHOR_CONTROL_ORACLE: command Ack verified");
       if(got_ack==(updating?2:1) && ai[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED){
         disposed_ack=1;puts("ANCHOR_CONTROL_ORACLE: Ack Dispose observed with last valid Ack");
       }
     }else if(ai[0].instance_state==DDS_IST_NOT_ALIVE_DISPOSED){
       if(!got_ack){fputs("ANCHOR_CONTROL_ORACLE: disposed Ack before sample\n",stderr);return 11;}
       disposed_ack=1;puts("ANCHOR_CONTROL_ORACLE: Ack Dispose observed");
     }
     dds_sample_free(&ack,&UMAA_EO_AnchorControl_AnchorCommandAckReportType_desc,DDS_FREE_CONTENTS);
   }
   if(updating && !sent_update && nstate>=3 && got_ack>=1){
     command.action=UMAA_Common_MaritimeEnumeration_AnchorActionEnumModule_STOP;
     command.timeStamp.nanoseconds+=1000; // same session/source, strictly newer
     if(dds_write(command_writer,&command)<0){fputs("ANCHOR_CONTROL_ORACLE: update command failed\n",stderr);return 15;}
     sent_update=1;puts("ANCHOR_CONTROL_ORACLE: command UPDATE issued (same source/session, later timestamp)");
   }
   if(nstate>=expected-1 && got_ack>=(updating?2:1) && !sent_cancel){
     if(dds_dispose(command_writer,&command)<0){fputs("ANCHOR_CONTROL_ORACLE: command dispose failed\n",stderr);return 12;}
     sent_cancel=1;puts("ANCHOR_CONTROL_ORACLE: command Dispose initiated to cancel");
   }
   if(nstate==expected && disposed_status && disposed_ack)break;
   dds_sleepfor(DDS_MSECS(25));
 }
 if(nstate!=expected||got_ack!=(updating?2:1)||!sent_cancel||!disposed_status||!disposed_ack || (updating && !sent_update)){
   fprintf(stderr,"ANCHOR_CONTROL_ORACLE: incomplete states=%d ack=%d cancel=%d ds=%d da=%d\n",
     nstate,got_ack,sent_cancel,disposed_status,disposed_ack);
   return 13;
 }
 if(dds_delete(p)<0)return 14;
 puts(updating?"ANCHOR_CONTROL_ORACLE: PASS official UMAA AnchorControl UPDATE/Ack/Status/Cancel/Dispose":"ANCHOR_CONTROL_ORACLE: PASS official UMAA AnchorControl AnchorCommand/Ack/Status/Cancel/Dispose");
 return 0;
}
