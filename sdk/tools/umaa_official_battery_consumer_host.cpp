// SDK development Host: genuine source-generated original Cyclone DDS
// Battery Report/Element Reader -> host-local ABI v2 business DLL/SO.
// Only the EO §3.8.1 ordered TOPIC Presentation minimum is reviewed;
// native reliability/history are demonstration choices, not full UMAA QoS.
#include <dds/dds.h>
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

namespace {
volatile std::sig_atomic_t interrupted=0;
extern "C" void on_signal(int) { interrupted=1; }

struct Config {
  std::string module;
  dds_domainid_t domain{DDS_DOMAIN_DEFAULT};
  std::uint32_t until_report_disposes{};
  std::uint32_t until_element_disposes{};
  std::uint32_t timeout_ms{};
};

std::uint32_t decimal(const std::string& raw, std::uint32_t maximum,
                      const char* label) {
  if(raw.empty()) throw std::invalid_argument(std::string("missing ")+label);
  std::uint32_t n=0;
  for(unsigned char ch:raw) {
    if(ch<'0'||ch>'9'||n>maximum/10)
      throw std::invalid_argument(std::string("invalid ")+label);
    n=n*10+static_cast<std::uint32_t>(ch-'0');
    if(n>maximum) throw std::invalid_argument(std::string("out-of-range ")+label);
  }
  return n;
}
Config arguments(int argc,char** argv) {
  if(argc<3 || argc%2!=1)
    throw std::invalid_argument(
      "usage: host --module FILE [--domain-id 0..230] "
      "[--until-report-disposes 1..128] [--until-element-disposes 1..4096] "
      "[--timeout-ms 1..600000]");
  Config cfg;
  bool seen_module=false,seen_domain=false,seen_until=false;
  bool seen_elements=false,seen_timeout=false;
  for(int i=1;i<argc;i+=2) {
    const std::string key=argv[i], val=argv[i+1] ? argv[i+1] : "";
    if(key=="--module" && !seen_module) {
      cfg.module=val;seen_module=true;
    } else if(key=="--domain-id" && !seen_domain) {
      cfg.domain=static_cast<dds_domainid_t>(decimal(val,230,"domain ID"));
      seen_domain=true;
    } else if(key=="--until-report-disposes" && !seen_until) {
      cfg.until_report_disposes=decimal(val,128,"Report Dispose threshold");
      if(!cfg.until_report_disposes)
        throw std::invalid_argument("Report Dispose threshold must be positive");
      seen_until=true;
    } else if(key=="--until-element-disposes" && !seen_elements) {
      cfg.until_element_disposes=decimal(val,4096,"Element Dispose threshold");
      if(!cfg.until_element_disposes)
        throw std::invalid_argument("Element Dispose threshold must be positive");
      seen_elements=true;
    } else if(key=="--timeout-ms" && !seen_timeout) {
      cfg.timeout_ms=decimal(val,600000,"timeout");
      if(!cfg.timeout_ms)
        throw std::invalid_argument("timeout must be positive");
      seen_timeout=true;
    } else throw std::invalid_argument("duplicate/unknown Host option");
  }
  if(!seen_module || cfg.module.empty())
    throw std::invalid_argument("Component DLL/SO is required");
  if((seen_until || seen_elements) && !seen_timeout)
    throw std::invalid_argument("Dispose threshold requires bounded timeout");
  return cfg;
}
}  // namespace

int main(int argc,char** argv) {
  try {
    const Config config=arguments(argc,argv);
    umaa::ComponentModule module(config.module);
    // Bridge checks exact official BatteryStatus service ID, host-local
    // schema, and required ABI-v2 Consumer actions before DDS allocation.
    umaa::official_battery::BatteryStatusComponentBridge business(module);
    using Reader=umaa::cyclone::official_battery::OriginalBatteryStatusReader;
    Reader reader(business,config.domain,16,4096);
    std::signal(SIGINT,on_signal);
    std::signal(SIGTERM,on_signal);
    std::cout<<"UMAA_BATTERY_HOST_READY transport=CycloneDDS"
             <<" schema=host-only-battery-stable-v1" <<std::endl;
    const auto start=std::chrono::steady_clock::now();
    while(!interrupted) {
      const bool got=reader.poll_once();
      const auto& statistics=reader.stats();
      // Report and Collection Element are distinct keyed DDS Topics.
      // Report Dispose alone does NOT prove all Element tombstones arrived.
      if((config.until_report_disposes || config.until_element_disposes) &&
         (!config.until_report_disposes ||
          statistics.report_disposes>=config.until_report_disposes) &&
         (!config.until_element_disposes ||
          statistics.element_disposes>=config.until_element_disposes) &&
         reader.active_lists()==0 && business.active_sources()==0) break;
      if(config.timeout_ms &&
         std::chrono::steady_clock::now()-start>=
           std::chrono::milliseconds(config.timeout_ms))
        throw std::runtime_error(
          "Battery Reader timeout: missing genuine Dispose or List completion");
      if(!got)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto statistics=reader.stats();
    if(business.faulted() || business.active_sources()!=0 ||
       reader.active_lists()!=0 || business.accepted()!=business.disposed() ||
       (config.until_report_disposes &&
        statistics.report_disposes<config.until_report_disposes) ||
       (config.until_element_disposes &&
        statistics.element_disposes<config.until_element_disposes))
      throw std::runtime_error("Battery Consumer Host has unresolved active state");
    reader.close();
    if(!module.stop())
      throw std::runtime_error("Battery Component refused safe stop");
    std::cout<<"UMAA_BATTERY_HOST_STOPPED"
             <<" accepted="<<business.accepted()
             <<" disposed="<<business.disposed()
             <<" nativeReportDisposes="<<statistics.report_disposes
             <<" nativeElementDisposes="<<statistics.element_disposes
             <<std::endl;
    return 0;
  } catch(const std::exception& e) {
    std::cerr<<"UMAA_BATTERY_HOST_ERROR: "<<e.what()<<std::endl;
    return 3;
  } catch(...) {
    std::cerr<<"UMAA_BATTERY_HOST_ERROR: unknown failure"<<std::endl;
    return 3;
  }
}
