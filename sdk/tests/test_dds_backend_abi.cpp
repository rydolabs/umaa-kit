#include "umaa/dds_backend_loader.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
struct Received {std::vector<uint32_t> kinds;int32_t last_value{};};
void on_sample(void* u,const UmaaDdsSampleV1* s){
 auto& r=*static_cast<Received*>(u);
 r.kinds.push_back(s->event_kind);
 if(s->payload_size==1)r.last_value=s->payload[0];
}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 umaa::DdsBackendPlugin plugin(argv[1]);
 assert(plugin.name()=="mock-contract-only");
 auto domain=plugin.open_domain(42);
 UmaaDdsTopicV1 topic{"demo/BatteryStatus","demo:battery-v1","demo-reliable",
  UMAA_DDS_CAP_RELIABLE|UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
 Received received;
 auto* reader=domain->reader(topic,on_sample,&received);
 auto* writer=domain->writer(topic);
 const uint8_t value=80;
 UmaaDdsSampleV1 sample{&value,1,"battery-a",100,UMAA_DDS_EVENT_DATA};
 assert(domain->write(writer,sample)==0);
 assert(domain->dispose(writer,sample)==0);
 assert(domain->unregister_instance(writer,sample)==0);
 assert(domain->poll(reader)==3);
 assert((received.kinds==std::vector<uint32_t>{1,2,3}));
 assert(received.last_value==80);
 assert(domain->wait_for_acks(writer,1000)==0);
 bool blocked=false;
 topic.required_features=UMAA_DDS_CAP_COHERENT_GROUP;
 try{(void)domain->writer(topic);}catch(const std::runtime_error&){blocked=true;}
 assert(blocked); // no silent unsupported QoS substitution
 topic.required_features=0;
 topic.schema_id="official:unreviewed";
 blocked=false;
 try{(void)domain->writer(topic);}catch(const std::runtime_error&){blocked=true;}
 assert(blocked); // no fake TypeSupport for official schemas
 domain->close_reader(reader);domain->close_writer(writer);
 std::cout<<"PASS DDS vendor ABI, events, lifecycle, fail-closed features/schema\n";
}
