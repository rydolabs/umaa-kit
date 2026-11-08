// Source-pinned native SDK Host factories. An admitted *experimental* native
// binding is NOT a complete UMAA DDS QoS or formal conformance approval.
//
// The only factory compiled by the original-source Cyclone build is the
// reviewed BatteryStatus Report/Cell native Reader + business ABI-v2 bridge.
// The normal SDK build compiles no such factory and cannot execute it.
#pragma once
#include <array>
#include <stdexcept>
#include <string_view>

namespace umaa::internal {
struct NativeHostSourceBinding {
  std::string_view id;
  std::string_view service;
  std::string_view abi_schema;
  std::string_view report_topic_type;
  std::string_view element_topic_type;
  std::string_view report_key;
  std::string_view element_keys;
  std::string_view idl_zip_sha256;
  std::string_view icd_sha256;
  bool source_topic_ordered_presentation;
  bool complete_native_qos_verified;
};
inline constexpr std::array<NativeHostSourceBinding,1> source_native_host_bindings{{
  {
    "UMAA::EO::BatteryStatus/cyclone/original-6.0-host-preview-v1",
    "UMAA::EO::BatteryStatus",
    "umaa-kit:host-only-battery-stable-v1:NOT-UMAA-CDR",
    "UMAA::EO::BatteryStatus::BatteryReportType",
    "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement",
    "source",
    "listID,elementID",
    "15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08",
    "47e88ecec73e37562ad04f5cf1ee5aef62b4de7f75e5ef993828f6dc8440d453",
    true,
    false
  },
}};

inline const NativeHostSourceBinding& require_native_host_source_binding(
    std::string_view binding,std::string_view idl_hash,
    std::string_view icd_hash) {
  for(const auto& entry:source_native_host_bindings) {
    if(entry.id==binding&&entry.idl_zip_sha256==idl_hash&&
       entry.icd_sha256==icd_hash) return entry;
  }
  throw std::invalid_argument(
      "unapproved native source binding, original IDL/ICD hashes mismatch");
}

// Implemented only in the *optional* compiled native factory. The standard
// SDK Host never references it and remains vendor-free.
int run_source_bound_native_host(int argc,char** argv);
} // namespace umaa::internal
