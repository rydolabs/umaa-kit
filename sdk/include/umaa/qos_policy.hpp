// Vendor-independent, reviewed DDS QoS policy and contract validation.
// DDS C++ vendor headers and policy mapping belong exclusively in backends.
#pragma once
#include "umaa/internal/dds_backend_qos.h"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
namespace umaa::qos {
enum class Reliability : uint32_t { best_effort=0, reliable=1 };
enum class Durability : uint32_t { volatile_data=0, transient_local=1 };
enum class History : uint32_t { keep_last=0, keep_all=1 };
enum class Presentation : uint32_t { instance=0, topic=1, group=2 };
struct Policy {
  Reliability reliability{Reliability::reliable};
  Durability durability{Durability::volatile_data};
  History history{History::keep_last};
  uint32_t depth{16};
  Presentation presentation{Presentation::instance};
  bool ordered_access{false};
  bool coherent_access{false};
  std::string partition;
};
struct Requirements {
  std::optional<Reliability> reliability;
  std::optional<Durability> durability;
  std::optional<Presentation> presentation;
  bool require_ordered_access{false};
  bool require_coherent_access{false};
  uint32_t minimum_history_depth{0};
};
inline void validate(const Policy& p) {
  if ((p.reliability!=Reliability::best_effort && p.reliability!=Reliability::reliable) ||
      (p.durability!=Durability::volatile_data && p.durability!=Durability::transient_local) ||
      (p.history!=History::keep_last && p.history!=History::keep_all) ||
      (p.presentation!=Presentation::instance && p.presentation!=Presentation::topic &&
       p.presentation!=Presentation::group))
    throw std::invalid_argument("unrecognized DDS QoS enum value");
  if ((p.history==History::keep_last && (p.depth==0 || p.depth>4096)) ||
      (p.history==History::keep_all && p.depth!=0))
    throw std::invalid_argument("DDS QoS history depth invalid");
  if ((p.ordered_access || p.coherent_access) && p.presentation==Presentation::instance)
    throw std::invalid_argument("DDS ordered/coherent access requires TOPIC or GROUP presentation");
  if (p.partition.size()>255 || p.partition.find('\0')!=std::string::npos ||
      p.partition.find('\n')!=std::string::npos || p.partition.find('\r')!=std::string::npos)
    throw std::invalid_argument("DDS QoS partition invalid");
}
inline void validate(const Policy& p,const Requirements& c) {
  validate(p);
  if ((c.reliability && *c.reliability!=p.reliability) ||
      (c.durability && *c.durability!=p.durability) ||
      (c.presentation && *c.presentation!=p.presentation) ||
      (c.require_ordered_access && !p.ordered_access) ||
      (c.require_coherent_access && !p.coherent_access) ||
      (c.minimum_history_depth &&
        (p.history!=History::keep_all && p.depth<c.minimum_history_depth)))
    throw std::invalid_argument("runtime QoS violates reviewed UMAA/Component requirements");
}
inline void validate_compatible(const Policy& writer,const Policy& reader) {
  validate(writer); validate(reader);
  if (reader.reliability==Reliability::reliable && writer.reliability!=Reliability::reliable)
    throw std::invalid_argument("DDS reader requires RELIABLE but writer offers BEST_EFFORT");
  if (reader.durability==Durability::transient_local &&
      writer.durability!=Durability::transient_local)
    throw std::invalid_argument("DDS reader requires TRANSIENT_LOCAL durability");
  if (writer.partition!=reader.partition)
    throw std::invalid_argument("DDS publisher/subscriber partition mismatch");
  if (reader.ordered_access && !writer.ordered_access)
    throw std::invalid_argument("DDS ordered access mismatch");
  if (reader.coherent_access && !writer.coherent_access)
    throw std::invalid_argument("DDS coherent access mismatch");
}
inline uint64_t required_capabilities(const Policy& p) {
  validate(p);
  uint64_t mask=0;
  if(p.reliability==Reliability::reliable)mask|=UMAA_DDS_CAP_RELIABLE;
  if(p.ordered_access)mask|=UMAA_DDS_CAP_ORDERED_ACCESS;
  if(p.coherent_access)mask|=UMAA_DDS_CAP_COHERENT_GROUP;
  return mask;
}
inline UmaaDdsQosV2 to_abi(const Policy& p) {
  validate(p);
  return {sizeof(UmaaDdsQosV2),
    static_cast<uint32_t>(p.reliability),
    static_cast<uint32_t>(p.durability),
    static_cast<uint32_t>(p.history),
    p.depth,static_cast<uint32_t>(p.presentation),
    p.ordered_access?1u:0u,p.coherent_access?1u:0u,p.partition.c_str()};
}
} // namespace umaa::qos
