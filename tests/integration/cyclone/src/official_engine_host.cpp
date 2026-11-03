// Real DDS EngineControl protocol proof with pinned official AUVSI 6.0 IDL.
// The Host owns all DDS entities and command state transitions. The user
// module provides only simulated engine behavior, never DDS administration.
// Not an official UMAA service conformance implementation.
#include <dds/dds.h>
#include "UMAA/EO/EngineControl/EngineCommandType.h"
#include "UMAA/EO/EngineControl/EngineCommandStatusType.h"
#include "UMAA/EO/EngineControl/EngineCommandAckReportType.h"
#include "umaa/cyclone_command_transport.hpp"
#include "umaa/cyclone_command_reader.hpp"
#include "umaa/cyclone_service_context.hpp"
#include "umaa/module_loader.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <time.h>

namespace {
using Cmd = UMAA_EO_EngineControl_EngineCommandType;
using Status = UMAA_EO_EngineControl_EngineCommandStatusType;
using Ack = UMAA_EO_EngineControl_EngineCommandAckReportType;
constexpr auto ISSUED=UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_ISSUED;
constexpr auto COMMANDED=UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_COMMANDED;
constexpr auto EXECUTING=UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_EXECUTING;
constexpr auto CANCELED=UMAA_Common_MaritimeEnumeration_CommandStatusEnumModule_CANCELED;
constexpr auto SUCCEEDED=UMAA_Common_MaritimeEnumeration_CommandStatusReasonEnumModule_SUCCEEDED;
constexpr auto CANCELED_REASON=UMAA_Common_MaritimeEnumeration_CommandStatusReasonEnumModule_CANCELED;
constexpr auto UPDATED_REASON=UMAA_Common_MaritimeEnumeration_CommandStatusReasonEnumModule_UPDATED;
// Scoped demo payload: preserve the entire fixed-layout Command needed for
// AckReport/Dispose. Optional pointer fields are deliberately unsupported.
// The SDK rejects those inputs rather than keeping borrowed DDS sample memory.
struct EnginePayload { Cmd command{}; int ignition{}; };

std::string hexid(const uint8_t id[16]) {
  static const char* hex="0123456789abcdef";
  std::string result;
  result.reserve(32);
  for(unsigned i=0;i<16;++i){result+=hex[id[i]>>4];result+=hex[id[i]&15];}
  return result;
}
void fillid(uint8_t id[16], uint8_t seed) {
  std::memset(id,0,16);
  id[0]=seed; id[6]=0x40; id[8]=0x80; id[15]=seed;
}
void report(dds_return_t rc,const char* op) {
  if(rc<0) {
    std::fprintf(stderr,"ENGINE_HOST DDS %s: %s (%d)\n",op,dds_strretcode(-rc),rc);
    throw umaa::ModuleLoadError(op);
  }
}
dds_qos_t* qos() {
  auto* q=dds_create_qos();
  dds_qset_reliability(q,DDS_RELIABILITY_RELIABLE,DDS_SECS(1));
  dds_qset_history(q,DDS_HISTORY_KEEP_ALL,0);
  return q;
}
class EngineHandler final : public umaa::Handler<EnginePayload> {
 public:
  explicit EngineHandler(umaa::ServiceModule& m):module(m){}
  bool validate(const EnginePayload& payload) override {
    const int state = payload.ignition;
    return state==UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_OFF ||
           state==UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_RUN;
  }
  umaa::Outcome execute(const EnginePayload& payload) override {
    module.invoke(1,payload.ignition);  // demo-only virtual operation, no hardware attached
    return umaa::Outcome::Pending; // ICD: EngineCommand has no natural completion
  }
  bool cancel(const EnginePayload&) override {
    module.invoke(1,0); // demo-only virtual stop action
    return true;
  }
 private:
  umaa::ServiceModule& module;
};
auto state_enum(const std::string& state) {
  if(state=="ISSUED")return ISSUED;
  if(state=="COMMANDED")return COMMANDED;
  if(state=="EXECUTING")return EXECUTING;
  if(state=="CANCELED")return CANCELED;
  throw umaa::ModuleLoadError("unsupported demo state");
}
// Session IDs in protocol events are hex-encoded UUID bytes, never arbitrary
// C strings. Reject any accidental or ambiguous key representation.
void decode_id(const std::string& hex, uint8_t id[16]) {
  if(hex.size()!=32) throw std::invalid_argument("invalid session UUID");
  auto nibble=[](char c)->int {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    throw std::invalid_argument("invalid session UUID hex");
  };
  for(unsigned i=0;i<16;++i) id[i]=static_cast<uint8_t>((nibble(hex[2*i])<<4)|nibble(hex[2*i+1]));
}
struct EngineMapper {
  Status make_status(const umaa::Identity& id,const umaa::Event& event) const {
    Status s{};
    fillid(s.source.id,0x22);
    decode_id(id.session,s.sessionID);
    s.timeStamp.seconds=(decltype(s.timeStamp.seconds))time(nullptr);
    s.commandStatus=state_enum(event.detail);
    s.commandStatusReason=(event.detail=="CANCELED"?CANCELED_REASON:
      (event.reason=="UPDATED"?UPDATED_REASON:SUCCEEDED));
    return s;
  }
  Ack make_ack(const umaa::Identity& id,const EnginePayload& payload) const {
    Ack ack{};
    ack.command=payload.command;
    decode_id(id.session,ack.sessionID);
    fillid(ack.source.id,0x22);
    ack.timeStamp.seconds=(decltype(ack.timeStamp.seconds))time(nullptr);
    return ack;
  }
};
} // namespace

int main(int argc,char** argv) {
 if(argc!=2){std::fputs("usage: official_engine_host <engine-service-module>\n",stderr);return 2;}
 try {
  std::printf("ENGINE_HOST TypeSupport keys: command=%u status=%u ack=%u\n",
    (unsigned)UMAA_EO_EngineControl_EngineCommandType_desc.m_nkeys,
    (unsigned)UMAA_EO_EngineControl_EngineCommandStatusType_desc.m_nkeys,
    (unsigned)UMAA_EO_EngineControl_EngineCommandAckReportType_desc.m_nkeys);
  umaa::ServiceModule module(argv[1],"UMAA::EO::EngineControl","umaa-6.0-enginecontrol-ignition-subset-v1");
  EngineHandler handler(module);
  uint8_t provider_id[16];fillid(provider_id,0x22);

  umaa::cyclone::ServiceContext context("UMAA::EO::EngineControl",
      umaa::cyclone::ServiceContext::ReviewMode::ExplicitPrototypeOnly);
  dds_entity_t participant=context.participant();
  const dds_entity_t command_topic=context.topic(
      "UMAA::EO::EngineControl::EngineCommandType", "UMAA::EO::EngineControl::EngineCommandType",
      umaa::reviewed::Role::Command, "EngineCommandType",
      &UMAA_EO_EngineControl_EngineCommandType_desc);
  const dds_entity_t status_topic=context.topic(
      "UMAA::EO::EngineControl::EngineCommandStatusType", "UMAA::EO::EngineControl::EngineCommandStatusType",
      umaa::reviewed::Role::Status, "EngineCommandStatusType",
      &UMAA_EO_EngineControl_EngineCommandStatusType_desc);
  const dds_entity_t ack_topic=context.topic(
      "UMAA::EO::EngineControl::EngineCommandAckReportType", "UMAA::EO::EngineControl::EngineCommandAckReportType",
      umaa::reviewed::Role::Ack, "EngineCommandAckReportType",
      &UMAA_EO_EngineControl_EngineCommandAckReportType_desc);
  auto* q=qos();
  using Reader=umaa::cyclone::CommandReader<Cmd>;
  Reader command_reader(participant,command_topic,
                        &UMAA_EO_EngineControl_EngineCommandType_desc,q);
  command_reader.start(1);
  dds_entity_t status_writer=dds_create_writer(participant,status_topic,q,nullptr);
  dds_entity_t ack_writer=dds_create_writer(participant,ack_topic,q,nullptr);
  dds_delete_qos(q);
  report(status_writer,"status writer");report(ack_writer,"ack writer");
  using Writer=umaa::cyclone::CommandWriter<EnginePayload,Status,Ack,EngineMapper>;
  Writer command_wire(status_writer,ack_writer,EngineMapper{});
  umaa::BoundCommandProvider<EnginePayload> engine(hexid(provider_id),handler,command_wire);
  bool accepted=false,disposed=false;
  std::string last_source,last_session;

  const int attempts = std::getenv("UMAA_NETEM_RECOVERY") ? 1600 : 600;
  for(int tick=0;tick<attempts && !disposed;++tick) {
    const bool got = command_reader.poll(1, [&](const Cmd& data, const dds_sample_info_t& info) {
      if (info.valid_data) {
        // Session updates use identical source+session, newer Timestamp; core validates both.
        if(std::memcmp(data.destination.id,provider_id,16)!=0)
          throw umaa::ModuleLoadError("command destination did not match configured provider");
        last_source=hexid(data.source.id);
        last_session=hexid(data.sessionID);
        umaa::Identity id{last_source,hexid(data.destination.id),last_session,
                          (int64_t)data.timeStamp.seconds*1000000+data.timeStamp.nanoseconds/1000};
        // Only the ignition RUN/OFF slice is implemented. Reject optional
        // pointer-bearing samples BEFORE copying borrowed DDS-owned memory.
        if(data.plugState || data.propulsion)
          throw umaa::ModuleLoadError("unsupported optional engine payload");
        EnginePayload payload{};
        payload.command=data; // safe only because optional pointers are null
        payload.ignition=static_cast<int>(data.state);
        auto events=engine.receive(id,payload);
        for(const auto& event:events) {
          if(event.kind==umaa::Event::Kind::Ignored)
            throw umaa::ModuleLoadError("rejected command: "+event.detail);
        }
        accepted=true;
      } else if(info.instance_state==DDS_IST_NOT_ALIVE_DISPOSED && accepted) {
        // One-session proof; identity saved from the preceding valid sample.
        engine.command_disposed(last_source,last_session);
        disposed=true;
      }
    });
    if (!got) dds_sleepfor(DDS_MSECS(25));
  }
  if(!accepted || !disposed || engine.sessions()!=0 || engine.faulted() ||
     command_wire.published_status_instances()!=0)
    throw umaa::ModuleLoadError("command lifecycle incomplete");
  command_reader.stop(1);
  report(dds_wait_for_acks(status_writer,DDS_SECS(3)),"status reliable acknowledgment");
  report(dds_wait_for_acks(ack_writer,DDS_SECS(3)),"ack reliable acknowledgment");
  dds_sleepfor(DDS_MSECS(1500)); // keep writers alive for reader invalid-data observation
  context.close();
  std::puts("ENGINE_HOST: PASS official EngineControl command ack/status/dispose proof");
  return 0;
 }catch(const std::exception& e) {
  std::fprintf(stderr,"ENGINE_HOST: failure: %s\n",e.what());return 3;
 }
}
