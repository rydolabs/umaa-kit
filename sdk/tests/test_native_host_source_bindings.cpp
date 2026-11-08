#ifdef NDEBUG
#undef NDEBUG
#endif
// Vendor-free admission proof: the reviewed literal Topic catalog is not
// automatically an executable original DDS factory. This binding records
// only one external-source-verified *experimental* Cyclone vertical slice.
#include "umaa/internal/native_host_source_bindings.hpp"
#include <cassert>
#include <stdexcept>
#include <string>

template<class F> void rejects(F&& f) {
  bool caught=false;
  try {f();}catch(const std::invalid_argument&) {caught=true;}
  assert(caught);
}
int main() {
  using umaa::internal::source_native_host_bindings;
  using umaa::internal::require_native_host_source_binding;
  static_assert(source_native_host_bindings.size()==1);
  const auto& s=source_native_host_bindings.front();
  assert(s.service=="UMAA::EO::BatteryStatus");
  assert(s.report_topic_type=="UMAA::EO::BatteryStatus::BatteryReportType");
  assert(s.element_topic_type=="UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement");
  assert(s.report_key=="source");
  assert(s.element_keys=="listID,elementID");
  assert(s.source_topic_ordered_presentation);
  assert(!s.complete_native_qos_verified);
  const auto& known=require_native_host_source_binding(
    s.id,s.idl_zip_sha256,s.icd_sha256);
  assert(&s==&known);
  rejects([&]{require_native_host_source_binding(
    "UMAA::EO::EngineControl",s.idl_zip_sha256,s.icd_sha256);});
  rejects([&]{require_native_host_source_binding(s.id,"",s.icd_sha256);});
  rejects([&]{require_native_host_source_binding(s.id,s.idl_zip_sha256,"");});
  rejects([&]{require_native_host_source_binding(
    s.id,std::string(64,'0'),s.icd_sha256);});
  rejects([&]{require_native_host_source_binding(
    s.id,s.idl_zip_sha256,std::string(64,'0'));});
  return 0;
}
