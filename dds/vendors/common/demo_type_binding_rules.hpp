// Shared synthetic TypeBinding allow-list for all DDS vendor plugins.
//
// This checks exact SDK-facing topic/schema/QoS/capability identities. It is
// NOT a substitute for the vendor-native IDL TypeSupport, reviewed official
// service Key/QoS mappings or an UMAA 6.0 conformance certificate.
// Keep this header in dds/vendors/common, never in application or SDK code.
#pragma once
#include "umaa/internal/dds_backend.h"
#include <cstdint>
#include <cstring>

namespace umaa::vendor::demo {

enum class Family : std::uint8_t { Report, Command, Collection };
constexpr std::uint64_t capabilities =
    UMAA_DDS_CAP_RELIABLE | UMAA_DDS_CAP_INSTANCE_LIFECYCLE;

struct Binding {
  const char* topic;
  const char* schema;
};

inline bool accepts(const UmaaDdsTopicV1* request, Family family) noexcept {
  if (!request || !request->topic_name || !request->schema_id ||
      !request->qos_profile ||
      std::strcmp(request->qos_profile, "demo-reliable") != 0 ||
      (request->required_features & ~capabilities) != 0)
    return false;
  constexpr Binding report[] = {
      {"demo/BatteryStatus", "demo:battery-v1"}};
  constexpr Binding command[] = {
      {"demo/AsyncEngineStatus", "demo:async-status-v1"},
      {"demo/AsyncEngineAck", "demo:async-ack-v1"},
      {"demo/AsyncEngineCommand", "demo:async-command-v1"}};
  constexpr Binding collection[] = {
      {"demo/CollectionBatch", "demo:collection-batch-v1"}};
  const Binding* bindings = nullptr;
  std::size_t count = 0;
  switch (family) {
    case Family::Report:
      bindings = report;
      count = sizeof(report) / sizeof(report[0]);
      break;
    case Family::Command:
      bindings = command;
      count = sizeof(command) / sizeof(command[0]);
      break;
    case Family::Collection:
      bindings = collection;
      count = sizeof(collection) / sizeof(collection[0]);
      break;
  }
  for (std::size_t i = 0; i < count; ++i)
    if (std::strcmp(request->topic_name, bindings[i].topic) == 0 &&
        std::strcmp(request->schema_id, bindings[i].schema) == 0)
      return true;
  return false;
}

}  // namespace umaa::vendor::demo
