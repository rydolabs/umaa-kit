// ABI contract filter is shared by Cyclone and Fast DDS implementations.
// This C++17 native test needs no vendor SDK. Release checks must stay active.
#include "../../dds/vendors/common/demo_type_binding_rules.hpp"
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char* why) {
  if (!ok) throw std::runtime_error(why);
}
}

int main() {
  using umaa::vendor::demo::Family;
  using umaa::vendor::demo::accepts;
  const struct Example {
    Family family;
    const char* topic;
    const char* schema;
  } examples[] = {
      {Family::Report, "demo/BatteryStatus", "demo:battery-v1"},
      {Family::Command, "demo/AsyncEngineCommand", "demo:async-command-v1"},
      {Family::Command, "demo/AsyncEngineAck", "demo:async-ack-v1"},
      {Family::Command, "demo/AsyncEngineStatus", "demo:async-status-v1"},
      {Family::Collection, "demo/CollectionBatch", "demo:collection-batch-v1"},
  };
  require(!accepts(nullptr, Family::Report), "null Topic must fail");
  for (const auto& x : examples) {
    UmaaDdsTopicV1 request{x.topic, x.schema, "demo-reliable",
      UMAA_DDS_CAP_RELIABLE | UMAA_DDS_CAP_INSTANCE_LIFECYCLE};
    require(accepts(&request, x.family), "reviewed synthetic fixture rejected");
    request.qos_profile = "unknown-qos";
    require(!accepts(&request, x.family), "unapproved QoS silently accepted");
    request.qos_profile = "demo-reliable";
    request.schema_id = "official:unreviewed";
    require(!accepts(&request, x.family), "unreviewed official TypeSupport accepted");
    request.schema_id = x.schema;
    request.required_features = UMAA_DDS_CAP_COHERENT_GROUP;
    require(!accepts(&request, x.family), "native coherent GROUP falsely advertised");
    request.required_features = UMAA_DDS_CAP_ORDERED_ACCESS;
    require(!accepts(&request, x.family), "native ordered access falsely advertised");
    request.required_features = UMAA_DDS_CAP_LIVELINESS;
    require(!accepts(&request, x.family), "unverified liveliness falsely advertised");
    request.required_features = 0;
    for (auto other : {Family::Report, Family::Command, Family::Collection})
      if (other != x.family)
        require(!accepts(&request, other), "cross-family Topic matched");
    request.topic_name = "fake/Topic";
    require(!accepts(&request, x.family), "unknown Topic accepted");
  }
  UmaaDdsTopicV1 missing{nullptr, "demo:battery-v1", "demo-reliable", 0};
  require(!accepts(&missing, Family::Report), "missing topic accepted");
  missing = {"demo/BatteryStatus", nullptr, "demo-reliable", 0};
  require(!accepts(&missing, Family::Report), "missing schema accepted");
  missing = {"demo/BatteryStatus", "demo:battery-v1", nullptr, 0};
  require(!accepts(&missing, Family::Report), "missing QoS accepted");
  require(umaa::vendor::demo::capabilities ==
          (UMAA_DDS_CAP_RELIABLE | UMAA_DDS_CAP_INSTANCE_LIFECYCLE),
          "synthetic capabilities changed without explicit approval");
}
