// Cyclone C backend: synthetic BatteryStatus TypeBinding only, NOT official UMAA.
#include "umaa/internal/dds_backend.h"
#include "../common/demo_type_binding_rules.hpp"
#include <dds/dds.h>
#include "DemoCommandEvent.h"
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
namespace {
struct Domain {dds_entity_t participant;};
struct Writer {dds_entity_t writer,topic;};
struct Reader {dds_entity_t reader,topic;UmaaDdsOnSampleV1 callback;void* user;};
constexpr uint64_t caps=umaa::vendor::demo::capabilities;
bool supported(const UmaaDdsTopicV1* topic) {
 return umaa::vendor::demo::accepts(topic, umaa::vendor::demo::Family::Command);
}
bool parse_key(const char* str,int32_t& out) {
 if(!str||!*str)return false;
 errno=0;char* end=nullptr;long n=std::strtol(str,&end,10);
 if(errno||*end||n<INT32_MIN||n>INT32_MAX)return false;
 out=static_cast<int32_t>(n);return true;
}
dds_qos_t* qos() {
 auto* q=dds_create_qos();
 if(q){dds_qset_reliability(q,DDS_RELIABILITY_RELIABLE,DDS_SECS(2));
       dds_qset_history(q,DDS_HISTORY_KEEP_LAST,16);}
 return q;
}
void* open_domain(uint32_t id) {
 if(id>230)return nullptr;
 auto p=dds_create_participant(id,nullptr,nullptr);
 if(p<0)return nullptr;
 auto* d=new(std::nothrow)Domain{p};
 if(!d)(void)dds_delete(p);
 return d;
}
void close_domain(void* ptr) {
 if(!ptr)return;auto* d=static_cast<Domain*>(ptr);
 (void)dds_delete(d->participant);delete d;
}
dds_entity_t topic(Domain* d,const UmaaDdsTopicV1* binding) {
 const auto* name=std::strcmp(binding->topic_name,"demo/AsyncEngineStatus")==0
     ?"Demo_EO_AsyncEngineStatus":
     (std::strcmp(binding->topic_name,"demo/AsyncEngineAck")==0
      ?"Demo_EO_AsyncEngineAck":"Demo_EO_AsyncEngineCommand");
 return dds_create_topic(d->participant,&Demo_EO_CommandEvent_desc,
                         name,nullptr,nullptr);
}
void* open_writer(void* ptr,const UmaaDdsTopicV1* t) {
 if(!ptr||!supported(t))return nullptr;
 auto* d=static_cast<Domain*>(ptr);auto id=topic(d,t);
 if(id<0)return nullptr;
 auto* q=qos();if(!q){(void)dds_delete(id);return nullptr;}
 auto w=dds_create_writer(d->participant,id,q,nullptr);dds_delete_qos(q);
 if(w<0){(void)dds_delete(id);return nullptr;}
 bool matched=false;
 for(int i=0;i<200;i++){
   dds_publication_matched_status_t s{};
   if(dds_get_publication_matched_status(w,&s)<0)break;
   if(s.current_count>0){matched=true;break;}
   dds_sleepfor(DDS_MSECS(25));
 }
 if(!matched){(void)dds_delete(w);(void)dds_delete(id);return nullptr;}
 auto* result=new(std::nothrow)Writer{w,id};
 if(!result){(void)dds_delete(w);(void)dds_delete(id);}
 return result;
}
void* open_reader(void* ptr,const UmaaDdsTopicV1* t,UmaaDdsOnSampleV1 cb,void* user) {
 if(!ptr||!supported(t)||!cb)return nullptr;
 auto* d=static_cast<Domain*>(ptr);auto id=topic(d,t);
 if(id<0)return nullptr;
 auto* q=qos();if(!q){(void)dds_delete(id);return nullptr;}
 auto r=dds_create_reader(d->participant,id,q,nullptr);dds_delete_qos(q);
 if(r<0){(void)dds_delete(id);return nullptr;}
 auto* result=new(std::nothrow)Reader{r,id,cb,user};
 if(!result){(void)dds_delete(r);(void)dds_delete(id);}
 return result;
}
int32_t submit(void* ptr,const UmaaDdsSampleV1* s,uint32_t kind) {
 if(!ptr||!s)return UMAA_DDS_INVALID;
 int32_t key{};
 if(!parse_key(s->instance_key,key))return UMAA_DDS_INVALID;
 Demo_EO_CommandEvent wire{};
 wire.session_id=key;
 if(kind==UMAA_DDS_EVENT_DATA){
   if(!s->payload||s->payload_size!=sizeof(int32_t))return UMAA_DDS_INVALID;
   std::memcpy(&wire.value,s->payload,sizeof(int32_t));
 }
 auto writer=static_cast<Writer*>(ptr)->writer;
 dds_return_t rc=0;
 if(kind==UMAA_DDS_EVENT_DATA)rc=dds_write(writer,&wire);
 else if(kind==UMAA_DDS_EVENT_DISPOSE)rc=dds_dispose(writer,&wire);
 else if(kind==UMAA_DDS_EVENT_UNREGISTER)rc=dds_unregister_instance(writer,&wire);
 else return UMAA_DDS_INVALID;
 return rc<0?UMAA_DDS_UNKNOWN_DELIVERY:UMAA_DDS_OK;
}
int32_t write(void* w,const UmaaDdsSampleV1* s){return submit(w,s,UMAA_DDS_EVENT_DATA);}
int32_t dispose(void* w,const UmaaDdsSampleV1* s){return submit(w,s,UMAA_DDS_EVENT_DISPOSE);}
int32_t unregister_instance(void* w,const UmaaDdsSampleV1* s){return submit(w,s,UMAA_DDS_EVENT_UNREGISTER);}
int32_t poll(void* ptr,uint32_t maximum) {
 if(!ptr||!maximum)return UMAA_DDS_INVALID;
 auto* reader=static_cast<Reader*>(ptr);
 uint32_t processed=0;
 while(processed<maximum){
   Demo_EO_CommandEvent value{};
   void* samples[1]={&value};dds_sample_info_t info[1]{};
   auto count=dds_take(reader->reader,samples,info,1,1);
   if(count<0)return UMAA_DDS_IO_ERROR;
   if(count==0)break;
   auto key=std::to_string(value.session_id);
   auto level=value.value;
   const uint8_t* bytes=nullptr;uint32_t length=0;uint32_t kind=0;
   if(info[0].valid_data){
     kind=UMAA_DDS_EVENT_DATA;
     bytes=reinterpret_cast<const uint8_t*>(&level);
     length=sizeof(level);
   }else if(info[0].instance_state==DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE)
     kind=UMAA_DDS_EVENT_DISPOSE;
   else if(info[0].instance_state==DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)
     kind=UMAA_DDS_EVENT_UNREGISTER;
   if(kind){
     UmaaDdsSampleV1 out{bytes,length,key.c_str(),info[0].source_timestamp,kind};
     reader->callback(reader->user,&out);
   }
   dds_sample_free(&value,&Demo_EO_CommandEvent_desc,DDS_FREE_CONTENTS);
   ++processed;
 }
 return static_cast<int32_t>(processed);
}
int32_t wait_acks(void* ptr,uint32_t ms){
 if(!ptr)return UMAA_DDS_INVALID;
 return dds_wait_for_acks(static_cast<Writer*>(ptr)->writer,DDS_MSECS(ms))<0
   ?UMAA_DDS_IO_ERROR:UMAA_DDS_OK;
}
void close_writer(void* ptr){
 if(!ptr)return;auto* w=static_cast<Writer*>(ptr);
 (void)dds_delete(w->writer);(void)dds_delete(w->topic);delete w;
}
void close_reader(void* ptr){
 if(!ptr)return;auto* r=static_cast<Reader*>(ptr);
 (void)dds_delete(r->reader);(void)dds_delete(r->topic);delete r;
}
const UmaaDdsBackendV1 api{
 UMAA_DDS_ABI_VERSION,sizeof(UmaaDdsBackendV1),"cyclone-synthetic-command-only",
 caps,&open_domain,&close_domain,&open_writer,&open_reader,&write,&dispose,
 &unregister_instance,&poll,&wait_acks,&close_writer,&close_reader};
}
extern "C" UMAA_DDS_EXPORT const UmaaDdsBackendV1* umaa_get_dds_backend_v1(uint32_t v){
 return v==UMAA_DDS_ABI_VERSION?&api:nullptr;
}
