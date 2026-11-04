// GENERATED from contracts/qos/umaa6/eo-large-collections.yaml; DO NOT EDIT
// Verified EO ICD 6.0 §3.8.1 minimum; not an entire Service QoS profile.
#pragma once
#include "umaa/qos_policy.hpp"
#include <optional>
#include <string>
namespace umaa::generated {
inline std::optional<umaa::qos::Requirements> reviewed_qos_requirements(
    const std::string& service_id, const std::string& topic_idl_type) {
  if(service_id=="UMAA::EO::BatteryStatus" &&
     topic_idl_type=="UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement") {
    umaa::qos::Requirements out;
    out.presentation=umaa::qos::Presentation::topic;
    out.require_ordered_access=true;
    return out;
  }
  return std::nullopt;
}
} // namespace umaa::generated
