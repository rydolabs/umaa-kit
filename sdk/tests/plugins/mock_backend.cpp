// Contract-test backend only. NOT network DDS, NOT a UMAA TypeSupport binding.
// Real vendors implement sdk/include/umaa/internal/dds_backend.h.
#include "umaa/internal/dds_backend.h"
#include <algorithm>
#include <deque>
#include <mutex>
#include <new>
#include <string>
#include <utility>
#include <vector>
namespace {
struct Packet {
  std::string key;
  std::vector<uint8_t> bytes;
  int64_t ts;
  uint32_t kind;
};
struct Reader;
struct Domain {
  std::mutex mtx;
  std::vector<Reader*> readers;
  uint32_t id;
};
struct Writer { Domain* domain; std::string topic; std::string schema; };
struct Reader {
  Domain* domain;
  std::string topic;
  std::string schema;
  UmaaDdsOnSampleV1 callback;
  void* context;
  std::deque<Packet> queue;
};
void* open_domain(uint32_t id) { return new (std::nothrow) Domain{{},{},id}; }
void close_domain(void* ctx) { delete static_cast<Domain*>(ctx); }
bool valid(const UmaaDdsTopicV1* t) {
  // Mock accepts only explicitly marked non-normative fixture contracts.
  return t && t->topic_name && *t->topic_name && t->schema_id &&
    std::string(t->schema_id).rfind("demo:",0)==0 && t->qos_profile &&
    std::string(t->qos_profile)=="demo-reliable" &&
    (t->required_features & ~(UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE)) == 0;
}
void* open_writer(void* ctx, const UmaaDdsTopicV1* t) {
  if (!ctx || !valid(t)) return nullptr;
  return new (std::nothrow) Writer{static_cast<Domain*>(ctx),t->topic_name,t->schema_id};
}
void* open_reader(void* ctx,const UmaaDdsTopicV1* t,UmaaDdsOnSampleV1 cb,void* u) {
  if (!ctx || !valid(t) || !cb) return nullptr;
  auto* d = static_cast<Domain*>(ctx);
  auto* r = new (std::nothrow) Reader{d,t->topic_name,t->schema_id,cb,u,{}};
  if (!r) return nullptr;
  std::lock_guard<std::mutex> lock(d->mtx);
  d->readers.push_back(r);
  return r;
}
int32_t submit(void* ptr,const UmaaDdsSampleV1* s,uint32_t kind) {
  if (!ptr || !s || !s->instance_key || !*s->instance_key ||
      (s->payload_size && !s->payload)) return UMAA_DDS_INVALID;
  auto* w = static_cast<Writer*>(ptr);
  Packet p{s->instance_key, {}, s->source_timestamp_ns,kind};
  if (s->payload_size) p.bytes.assign(s->payload,s->payload+s->payload_size);
  auto* d = w->domain;
  std::lock_guard<std::mutex> lock(d->mtx);
  for (auto* r : d->readers)
    if (r->topic==w->topic && r->schema==w->schema) r->queue.push_back(p);
  return UMAA_DDS_OK;
}
int32_t write(void* w,const UmaaDdsSampleV1* s) {return submit(w,s,UMAA_DDS_EVENT_DATA);}
int32_t dispose(void* w,const UmaaDdsSampleV1* s) {return submit(w,s,UMAA_DDS_EVENT_DISPOSE);}
int32_t unregister_instance(void* w,const UmaaDdsSampleV1* s) {return submit(w,s,UMAA_DDS_EVENT_UNREGISTER);}
int32_t poll(void* ptr,uint32_t max_samples) {
  if(!ptr || max_samples==0)return UMAA_DDS_INVALID;
  auto* r=static_cast<Reader*>(ptr);
  uint32_t count=0;
  while(count<max_samples){
    Packet p;
    {
      std::lock_guard<std::mutex> lock(r->domain->mtx);
      if(r->queue.empty()) break;
      p=std::move(r->queue.front());r->queue.pop_front();
    }
    UmaaDdsSampleV1 sample{p.bytes.empty()?nullptr:p.bytes.data(),
      static_cast<uint32_t>(p.bytes.size()),p.key.c_str(),p.ts,p.kind};
    r->callback(r->context,&sample);
    ++count;
  }
  return static_cast<int32_t>(count);
}
int32_t wait_for_acks(void* ptr,uint32_t) {return ptr?UMAA_DDS_OK:UMAA_DDS_INVALID;}
void close_writer(void* ptr) {delete static_cast<Writer*>(ptr);}
void close_reader(void* ptr) {
  if(!ptr)return;
  auto* r=static_cast<Reader*>(ptr);
  {std::lock_guard<std::mutex> lock(r->domain->mtx);
   auto& v=r->domain->readers;v.erase(std::remove(v.begin(),v.end(),r),v.end());}
  delete r;
}
const UmaaDdsBackendV1 api={UMAA_DDS_ABI_VERSION,sizeof(UmaaDdsBackendV1),
  "mock-contract-only",UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE,
  &open_domain,&close_domain,&open_writer,&open_reader,&write,&dispose,
  &unregister_instance,&poll,&wait_for_acks,&close_writer,&close_reader};
}
extern "C" UMAA_DDS_EXPORT const UmaaDdsBackendV1* umaa_get_dds_backend_v1(uint32_t v){
  return v==UMAA_DDS_ABI_VERSION ? &api : nullptr;
}
