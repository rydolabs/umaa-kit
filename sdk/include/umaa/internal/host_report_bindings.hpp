// Explicit admitted runtime wire bindings for the development Host.
// This file is deliberately *not* the 53-Service reviewed Topic catalog:
// a literal ICD Topic alone is NOT original TypeSupport/Key/QoS admission.
// Only already supported, non-normative demo mappings are executable here.
// Source-gated official bindings require separately reviewed/generated native
// factories and actual vendor validation before they may be added.
#pragma once
#include <array>
#include <stdexcept>
#include <string>

namespace umaa::internal {
struct HostReportBinding {
  const char* service;
  const char* topic;
  const char* schema;
  const char* qos;
  bool development_only;
};
inline constexpr std::array<HostReportBinding,1> executable_host_reports{{
  {"Demo::EO::BatteryStatus","demo/BatteryStatus","demo:battery-v1",
   "demo-reliable",true},
}};
inline const HostReportBinding& require_host_report_binding(
    const std::string& service,const std::string& topic,
    const std::string& schema,const std::string& qos) {
  for(const auto& entry:executable_host_reports) {
    if(service==entry.service&&topic==entry.topic&&
       schema==entry.schema&&qos==entry.qos&&entry.development_only)
      return entry;
  }
  throw std::invalid_argument(
      "unapproved Host ServiceBinding: original TypeSupport/Key/QoS not verified");
}
} // namespace umaa::internal
