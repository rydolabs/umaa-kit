// Installed-target smoke with DDS test ABI shim, never a real Cyclone test.
#include <umaa/cyclone_command_transport.hpp>
#include <cstdint>
struct Payload { int state; };
struct Sample { int instance; };
struct Mapper {
  Sample make_status(const umaa::Identity&, const umaa::Event&) const {return {1};}
  Sample make_ack(const umaa::Identity&, const Payload&) const {return {1};}
};
extern "C" dds_return_t dds_write(dds_entity_t, const void*) {return 0;}
extern "C" dds_return_t dds_dispose(dds_entity_t, const void*) {return 0;}
extern "C" const char* dds_strretcode(int32_t) {return "fixture";}
int main() {
  umaa::cyclone::CommandWriter<Payload,Sample,Sample,Mapper> adapter(1,2,Mapper{});
  const umaa::Identity id{"s","d","s1",1};
  adapter.publish_status(id,{umaa::Event::Kind::Status,"s1","ISSUED","SUCCEEDED"});
  adapter.publish_ack(id,{1});
  adapter.dispose_status(id);
  adapter.dispose_ack(id,{1});
  return adapter.published_status_instances() == 0 ? 0 : 1;
}
