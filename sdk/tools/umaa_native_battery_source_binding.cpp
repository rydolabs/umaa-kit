// Optional *original-source* native SDK Host factory for UMAA 6.0
// BatteryStatus, compiled ONLY with authenticated AUVSI IDL Cyclone
// TypeSupport. This source is linked into the normal Host main in an
// opt-in native executable; the default SDK binary has no native factory.
//
// This uses the same narrow, independently tested Reader/Bridge as the
// historic dedicated Battery Host. Only EO ICD section 3.8.1 TOPIC ordered
// presentation is source-backed; the full mandatory QoS set is not approved.
#include "umaa/internal/native_host_source_bindings.hpp"
#include "umaa/component_module.hpp"
#include "umaa/official_battery_component_bridge.hpp"
#include "umaa/cyclone_official_battery_reader.hpp"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace umaa::internal {
namespace {
volatile std::sig_atomic_t native_stopping=0;
extern "C" void native_signal(int) {native_stopping=1;}

std::uint32_t parse_bounded(const std::string& raw,std::uint32_t max,
                            const char* label) {
  if(raw.empty())throw std::invalid_argument(std::string("missing ")+label);
  std::uint32_t value=0;
  for(unsigned char c:raw) {
    if(c<'0'||c>'9')throw std::invalid_argument(std::string("invalid ")+label);
    const auto digit=static_cast<std::uint32_t>(c-'0');
    if(value>(max-digit)/10u)
      throw std::invalid_argument(std::string("out-of-range ")+label);
    value=value*10u+digit;
  }
  return value;
}

struct NativeConfig {
  std::string module;
  std::string binding;
  std::string idl_hash;
  std::string icd_hash;
  dds_domainid_t domain{DDS_DOMAIN_DEFAULT};
  std::uint32_t until_report_disposes{};
  std::uint32_t until_element_disposes{};
  std::uint32_t timeout_ms{};
};
NativeConfig parse(int argc,char** argv) {
  if(argc<5 || argc%2!=1 ||
     std::string(argv[1])!="--native-binding")
    throw std::invalid_argument(
        "usage: Host --native-binding ID --module FILE "
        "--source-idl-sha256 HASH --source-icd-sha256 HASH "
        "[--domain-id 0..230] "
        "[--until-report-disposes 1..128] "
        "[--until-element-disposes 1..4096] "
        "[--timeout-ms 1..600000]");
  NativeConfig cfg;
  bool seen_module=false,seen_domain=false,seen_report=false;
  bool seen_elements=false,seen_timeout=false,seen_idl=false,seen_icd=false;
  cfg.binding=argv[2]?argv[2]:"";
  for(int i=3;i<argc;i+=2) {
    const std::string key=argv[i]?argv[i]:"";
    const std::string value=argv[i+1]?argv[i+1]:"";
    if(key=="--module" && !seen_module) {
      cfg.module=value;seen_module=true;
    } else if(key=="--source-idl-sha256" && !seen_idl) {
      cfg.idl_hash=value;seen_idl=true;
    } else if(key=="--source-icd-sha256" && !seen_icd) {
      cfg.icd_hash=value;seen_icd=true;
    } else if(key=="--domain-id" && !seen_domain) {
      cfg.domain=static_cast<dds_domainid_t>(parse_bounded(value,230,"DDS domain"));
      seen_domain=true;
    } else if(key=="--until-report-disposes" && !seen_report) {
      cfg.until_report_disposes=parse_bounded(value,128,"Report Dispose threshold");
      if(!cfg.until_report_disposes)
        throw std::invalid_argument("Report Dispose threshold must be positive");
      seen_report=true;
    } else if(key=="--until-element-disposes" && !seen_elements) {
      cfg.until_element_disposes=parse_bounded(value,4096,"Cell Dispose threshold");
      if(!cfg.until_element_disposes)
        throw std::invalid_argument("Cell Dispose threshold must be positive");
      seen_elements=true;
    } else if(key=="--timeout-ms" && !seen_timeout) {
      cfg.timeout_ms=parse_bounded(value,600000,"native Host timeout");
      if(!cfg.timeout_ms)
        throw std::invalid_argument("native Host timeout must be positive");
      seen_timeout=true;
    } else {
      throw std::invalid_argument("duplicate or unknown native binding option");
    }
  }
  // Check the exact source mapping *before* opening the business module or
  // DDS entities. This is a provenance fence, not proof of full QoS.
  if(!seen_module||cfg.module.empty()||!seen_idl||!seen_icd)
    throw std::invalid_argument("native binding requires module and source hashes");
  if((seen_report||seen_elements)&&!seen_timeout)
    throw std::invalid_argument("native Dispose threshold needs bounded timeout");
  const auto& record=require_native_host_source_binding(
      cfg.binding,cfg.idl_hash,cfg.icd_hash);
  if(record.service!="UMAA::EO::BatteryStatus" ||
     record.abi_schema!="umaa-kit:host-only-battery-stable-v1:NOT-UMAA-CDR" ||
     !record.source_topic_ordered_presentation ||
     record.complete_native_qos_verified)
    throw std::logic_error("native battery factory source metadata mismatch");
  return cfg;
}
}

int run_source_bound_native_host(int argc,char** argv) {
  const auto cfg=parse(argc,argv);
  ::umaa::ComponentModule module(cfg.module);
  ::umaa::official_battery::BatteryStatusComponentBridge business(module);
  using Reader=::umaa::cyclone::official_battery::OriginalBatteryStatusReader;
  Reader reader(business,cfg.domain,16,4096);

  native_stopping=0;
  std::signal(SIGINT,native_signal);
  std::signal(SIGTERM,native_signal);
  std::cout<<"UMAA_HOST_NATIVE_READY"
           <<" binding="<<cfg.binding
           <<" transport=CycloneDDS"
           <<" verifiedScope=EO-3.8.1-TOPIC-ordered-only"
           <<std::endl;
  const auto start=std::chrono::steady_clock::now();
  while(!native_stopping) {
    const bool got=reader.poll_once();
    const auto& stats=reader.stats();
    if((cfg.until_report_disposes||cfg.until_element_disposes) &&
       (!cfg.until_report_disposes||
        stats.report_disposes>=cfg.until_report_disposes) &&
       (!cfg.until_element_disposes||
        stats.element_disposes>=cfg.until_element_disposes) &&
       reader.active_lists()==0 && business.active_sources()==0)
      break;
    if(cfg.timeout_ms &&
       std::chrono::steady_clock::now()-start>=
          std::chrono::milliseconds(cfg.timeout_ms))
      throw std::runtime_error(
          "native Report Host timed out with outstanding source/list state");
    if(!got)std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  const auto stats=reader.stats();
  if(business.faulted()||business.active_sources()!=0||
     reader.active_lists()!=0||business.accepted()!=business.disposed()||
     (cfg.until_report_disposes&&
      stats.report_disposes<cfg.until_report_disposes)||
     (cfg.until_element_disposes&&
      stats.element_disposes<cfg.until_element_disposes))
    throw std::runtime_error("native Host has unresolved original DDS state");
  reader.close();
  if(!module.stop())
    throw std::runtime_error("native Component rejected safe stop");
  std::cout<<"UMAA_HOST_NATIVE_STOPPED"
           <<" accepted="<<business.accepted()
           <<" disposed="<<business.disposed()
           <<" nativeReportDisposes="<<stats.report_disposes
           <<" nativeElementDisposes="<<stats.element_disposes
           <<std::endl;
  return 0;
}
} // namespace umaa::internal
