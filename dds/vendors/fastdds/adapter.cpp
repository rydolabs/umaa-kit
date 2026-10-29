// Fast DDS adapter for synthetic BatteryStatus ONLY. No official UMAA QoS claim.
#include "umaa/internal/dds_backend.h"
#include "DemoBattery.h"
#include "DemoBatteryPubSubTypes.h"
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <new>
#include <string>
#include <thread>
namespace {
using namespace eprosima::fastdds::dds;
using Wire=Demo::EO::BatteryReportType;
using WirePubSub=Demo::EO::BatteryReportTypePubSubType;
constexpr uint64_t supported=UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE;
struct Domain {DomainParticipant* p;TypeSupport type;};
struct Writer {Domain* d;Topic* t;Publisher* pub;DataWriter* writer;};
struct Reader {Domain* d;Topic* t;Subscriber* sub;DataReader* reader;UmaaDdsOnSampleV1 callback;void* user;};
bool approved(const UmaaDdsTopicV1* topic){
 return topic && topic->topic_name && topic->schema_id && topic->qos_profile &&
 !std::strcmp(topic->topic_name,"demo/BatteryStatus") &&
 !std::strcmp(topic->schema_id,"demo:battery-v1") &&
 !std::strcmp(topic->qos_profile,"demo-reliable") &&
 !(topic->required_features & ~supported);
}
bool decode_key(const char* s,int32_t& key){
 if(!s||!*s)return false;
 errno=0;char* end=nullptr;long n=std::strtol(s,&end,10);
 if(errno||!*s||*end||n<INT32_MIN||n>INT32_MAX)return false;
 key=static_cast<int32_t>(n);return true;
}
void* open_domain(uint32_t domain){
 if(domain>230)return nullptr;
 try{
  auto* p=DomainParticipantFactory::get_instance()->create_participant(domain,PARTICIPANT_QOS_DEFAULT);
  if(!p)return nullptr;
  TypeSupport type(new WirePubSub());
  if(type.register_type(p)!=ReturnCode_t::RETCODE_OK){
   (void)DomainParticipantFactory::get_instance()->delete_participant(p);return nullptr;
  }
  return new Domain{p,type};
 }catch(...){return nullptr;}
}
void close_domain(void* handle){
 auto* d=static_cast<Domain*>(handle);if(!d)return;
 (void)d->p->delete_contained_entities();
 (void)DomainParticipantFactory::get_instance()->delete_participant(d->p);delete d;
}
Topic* make_topic(Domain* d){
 return d->p->create_topic("Demo_EO_BatteryReportType",d->type.get_type_name(),TOPIC_QOS_DEFAULT);
}
void* open_writer(void* handle,const UmaaDdsTopicV1* topic){
 if(!handle||!approved(topic))return nullptr;
 auto* d=static_cast<Domain*>(handle);auto* t=make_topic(d);if(!t)return nullptr;
 auto* pub=d->p->create_publisher(PUBLISHER_QOS_DEFAULT);
 if(!pub){(void)d->p->delete_topic(t);return nullptr;}
 DataWriterQos qos=DATAWRITER_QOS_DEFAULT;
 qos.reliability().kind=RELIABLE_RELIABILITY_QOS;
 qos.history().kind=KEEP_LAST_HISTORY_QOS;qos.history().depth=16;
 qos.writer_data_lifecycle().autodispose_unregistered_instances=false;
 auto* writer=pub->create_datawriter(t,qos);
 if(!writer){
  (void)d->p->delete_publisher(pub);(void)d->p->delete_topic(t);return nullptr;
 }
 auto* result=new(std::nothrow)Writer{d,t,pub,writer};
 if(!result){
  (void)pub->delete_datawriter(writer);(void)d->p->delete_publisher(pub);
  (void)d->p->delete_topic(t);return nullptr;
 }
 bool found=false;
 for(int attempt=0;attempt<200;++attempt){
  PublicationMatchedStatus status{};
  if(writer->get_publication_matched_status(status)!=ReturnCode_t::RETCODE_OK)break;
  if(status.current_count>0){found=true;break;}
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
 }
 if(!found){
  (void)pub->delete_datawriter(writer);(void)d->p->delete_publisher(pub);
  (void)d->p->delete_topic(t);delete result;return nullptr;
 }
 return result;
}
void* open_reader(void* handle,const UmaaDdsTopicV1* topic,UmaaDdsOnSampleV1 cb,void* user){
 if(!handle||!approved(topic)||!cb)return nullptr;
 auto* d=static_cast<Domain*>(handle);auto* t=make_topic(d);if(!t)return nullptr;
 auto* sub=d->p->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
 if(!sub){(void)d->p->delete_topic(t);return nullptr;}
 DataReaderQos qos=DATAREADER_QOS_DEFAULT;
 qos.reliability().kind=RELIABLE_RELIABILITY_QOS;
 qos.history().kind=KEEP_LAST_HISTORY_QOS;qos.history().depth=16;
 auto* reader=sub->create_datareader(t,qos);
 if(!reader){
  (void)d->p->delete_subscriber(sub);(void)d->p->delete_topic(t);return nullptr;
 }
 auto* result=new(std::nothrow)Reader{d,t,sub,reader,cb,user};
 if(!result){
  (void)sub->delete_datareader(reader);(void)d->p->delete_subscriber(sub);
  (void)d->p->delete_topic(t);
 }
 return result;
}
int32_t send(void* handle,const UmaaDdsSampleV1* sample,uint32_t kind){
 if(!handle||!sample||!sample->instance_key)return UMAA_DDS_INVALID;
 int32_t key=0;if(!decode_key(sample->instance_key,key))return UMAA_DDS_INVALID;
 if(kind==UMAA_DDS_EVENT_DATA && (!sample->payload||sample->payload_size!=sizeof(int32_t)))
  return UMAA_DDS_INVALID;
 Wire wire;wire.source_id(key);
 int32_t level=0;
 if(kind==UMAA_DDS_EVENT_DATA)std::memcpy(&level,sample->payload,sizeof(level));
 wire.level_percent(level);
 auto* writer=static_cast<Writer*>(handle)->writer;
 ReturnCode_t rc=ReturnCode_t::RETCODE_BAD_PARAMETER;
 if(kind==UMAA_DDS_EVENT_DATA)rc=writer->write(&wire);
 else if(kind==UMAA_DDS_EVENT_DISPOSE)rc=writer->dispose(&wire,eprosima::fastdds::dds::HANDLE_NIL);
 else if(kind==UMAA_DDS_EVENT_UNREGISTER)rc=writer->unregister_instance(&wire,eprosima::fastdds::dds::HANDLE_NIL);
 return rc==ReturnCode_t::RETCODE_OK?UMAA_DDS_OK:UMAA_DDS_UNKNOWN_DELIVERY;
}
int32_t write(void* p,const UmaaDdsSampleV1* s){return send(p,s,UMAA_DDS_EVENT_DATA);}
int32_t dispose(void* p,const UmaaDdsSampleV1* s){return send(p,s,UMAA_DDS_EVENT_DISPOSE);}
int32_t unregister_instance(void* p,const UmaaDdsSampleV1* s){return send(p,s,UMAA_DDS_EVENT_UNREGISTER);}
int32_t poll(void* handle,uint32_t max_samples){
 if(!handle||!max_samples)return UMAA_DDS_INVALID;
 auto* r=static_cast<Reader*>(handle);uint32_t delivered=0;
 for(;delivered<max_samples;++delivered){
  Wire wire;SampleInfo info{};
  auto rc=r->reader->take_next_sample(&wire,&info);
  if(rc==ReturnCode_t::RETCODE_NO_DATA)break;
  if(rc!=ReturnCode_t::RETCODE_OK)return UMAA_DDS_IO_ERROR;
  uint32_t kind=0;
  if(info.valid_data)kind=UMAA_DDS_EVENT_DATA;
  else if(info.instance_state==NOT_ALIVE_DISPOSED_INSTANCE_STATE)kind=UMAA_DDS_EVENT_DISPOSE;
  else if(info.instance_state==NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)kind=UMAA_DDS_EVENT_UNREGISTER;
  if(!info.valid_data&&kind&&r->reader->get_key_value(&wire,info.instance_handle)!=ReturnCode_t::RETCODE_OK)
   return UMAA_DDS_IO_ERROR;
  if(kind){
   const std::string key=std::to_string(wire.source_id());
   int32_t value=wire.level_percent();
   int64_t timestamp=info.source_timestamp.to_ns();
   UmaaDdsSampleV1 sample{
     kind==UMAA_DDS_EVENT_DATA?reinterpret_cast<uint8_t*>(&value):nullptr,
     kind==UMAA_DDS_EVENT_DATA?static_cast<uint32_t>(sizeof(value)):0,
     key.c_str(),timestamp,kind};
   r->callback(r->user,&sample);
  }
 }
 return static_cast<int32_t>(delivered);
}
int32_t wait_for_acks(void* ptr,uint32_t ms){
 if(!ptr)return UMAA_DDS_INVALID;
 const eprosima::fastrtps::Duration_t duration(static_cast<int32_t>(ms/1000),
                                                static_cast<uint32_t>((ms%1000)*1000000));
 return static_cast<Writer*>(ptr)->writer->wait_for_acknowledgments(duration)==ReturnCode_t::RETCODE_OK?
  UMAA_DDS_OK:UMAA_DDS_IO_ERROR;
}
void close_writer(void* handle){
 auto* w=static_cast<Writer*>(handle);if(!w)return;
 (void)w->pub->delete_datawriter(w->writer);
 (void)w->d->p->delete_publisher(w->pub);(void)w->d->p->delete_topic(w->t);delete w;
}
void close_reader(void* handle){
 auto* r=static_cast<Reader*>(handle);if(!r)return;
 (void)r->sub->delete_datareader(r->reader);
 (void)r->d->p->delete_subscriber(r->sub);(void)r->d->p->delete_topic(r->t);delete r;
}
const UmaaDdsBackendV1 backend={UMAA_DDS_ABI_VERSION,sizeof(UmaaDdsBackendV1),
 "fastdds-synthetic-battery-only",supported,&open_domain,&close_domain,
 &open_writer,&open_reader,&write,&dispose,&unregister_instance,&poll,
 &wait_for_acks,&close_writer,&close_reader};
}
extern "C" UMAA_DDS_EXPORT const UmaaDdsBackendV1* umaa_get_dds_backend_v1(uint32_t version){
 return version==UMAA_DDS_ABI_VERSION?&backend:nullptr;
}
