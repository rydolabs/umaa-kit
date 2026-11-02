// Strict SDK runtime configuration subset, not a general YAML parser.
// Middleware, modules, and explicitly named flat qos_profiles (no YAML aliases).
// Fail closed on unknown keys, duplicate settings, excessive input or
// malformed/ambiguous values. No dynamic evaluation, file inclusion or
// environment-variable expansion. Paths relative to this file.
#pragma once
#include "umaa/qos_policy.hpp"
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
namespace umaa::internal {
struct RuntimeConfig {
  std::string backend;
  uint32_t domain_id{};
  std::vector<std::string> modules;
  std::unordered_map<std::string, umaa::qos::Policy> qos_profiles;
  static std::string trim(std::string s) {
    auto keep=[](unsigned char c){return !std::isspace(c);};
    while(!s.empty()&&!keep(static_cast<unsigned char>(s.front())))s.erase(s.begin());
    while(!s.empty()&&!keep(static_cast<unsigned char>(s.back())))s.pop_back();
    return s;
  }
  static std::string value(const std::string& raw) {
    auto v=trim(raw);
    if(v.size()>4096||v.empty())throw std::invalid_argument("empty or oversized configuration value");
    if(v.front()=='"'||v.front()=='\''){
      if(v.size()<2||v.back()!=v.front())throw std::invalid_argument("unclosed config quote");
      v=v.substr(1,v.size()-2);
    }
    if(v.empty()||v.find('\n')!=std::string::npos||v.find('\r')!=std::string::npos)
      throw std::invalid_argument("invalid configuration value");
    return v;
  }
  static RuntimeConfig read(const std::string& filename){
    std::ifstream in(filename);
    if(!in)throw std::runtime_error("cannot open UMAA runtime configuration");
    const auto root=std::filesystem::absolute(filename).parent_path();
    enum class Section { None, Middleware, Modules, QoS } section=Section::None;
    std::string selected_qos;
    std::unordered_set<std::string> qos_fields;
    bool qos_section_seen=false;
    bool backend_set=false,domain_set=false;
    std::unordered_set<std::string> modules_seen;
    RuntimeConfig cfg;
    std::string line;
    unsigned line_number=0;
    while(std::getline(in,line)){
      if(++line_number>1024 || line.size()>8192)
        throw std::invalid_argument("UMAA configuration size limit exceeded");
      if(!line.empty()&&line.back()=='\r')line.pop_back();
      if(trim(line).empty()||trim(line)[0]=='#')continue;
      if(line=="middleware:"){section=Section::Middleware;continue;}
      if(line=="modules:"){section=Section::Modules;continue;}
      if(line=="qos_profiles:"){
        if(qos_section_seen)throw std::invalid_argument("duplicate QoS profiles section");
        qos_section_seen=true;
        section=Section::QoS;continue;
      }
      if(section==Section::QoS){
        if(line.rfind("  ",0)==0 && line.rfind("   ",0)!=0){
          const auto text=line.substr(2);
          if(text.size()<2||text.back()!=':')
            throw std::invalid_argument("QoS profile requires a named mapping");
          selected_qos=text.substr(0,text.size()-1);
          if(selected_qos.empty()||selected_qos.size()>128 ||
             selected_qos.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.:-")!=std::string::npos ||
             !cfg.qos_profiles.emplace(selected_qos,umaa::qos::Policy{}).second)
            throw std::invalid_argument("invalid or repeated QoS profile");
          qos_fields.clear();continue;
        }
        if(line.rfind("    ",0)==0 && line.rfind("     ",0)!=0){
          if(selected_qos.empty())throw std::invalid_argument("QoS field without profile");
          const auto text=line.substr(4);
          const auto colon=text.find(':');
          if(colon==std::string::npos)throw std::invalid_argument("invalid QoS setting");
          const auto key=trim(text.substr(0,colon)), v=value(text.substr(colon+1));
          if(!qos_fields.insert(key).second)
            throw std::invalid_argument("duplicate QoS field");
          auto& q=cfg.qos_profiles.at(selected_qos);
          if(key=="reliability"){
            if(v=="reliable")q.reliability=umaa::qos::Reliability::reliable;
            else if(v=="best_effort")q.reliability=umaa::qos::Reliability::best_effort;
            else throw std::invalid_argument("invalid reliability");
          }else if(key=="durability"){
            if(v=="volatile")q.durability=umaa::qos::Durability::volatile_data;
            else if(v=="transient_local")q.durability=umaa::qos::Durability::transient_local;
            else throw std::invalid_argument("invalid durability");
          }else if(key=="history"){
            if(v=="keep_last")q.history=umaa::qos::History::keep_last;
            else if(v=="keep_all")q.history=umaa::qos::History::keep_all;
            else throw std::invalid_argument("invalid history");
          }else if(key=="depth"){
            uint32_t depth=0;
            for(unsigned char c:v){
              if(!std::isdigit(c)||depth>4096)
                throw std::invalid_argument("invalid history depth");
              depth=depth*10+(c-'0');
              if(depth>4096)throw std::invalid_argument("invalid history depth");
            }
            q.depth=depth;
          }else if(key=="presentation_scope"){
            if(v=="instance")q.presentation=umaa::qos::Presentation::instance;
            else if(v=="topic")q.presentation=umaa::qos::Presentation::topic;
            else if(v=="group")q.presentation=umaa::qos::Presentation::group;
            else throw std::invalid_argument("invalid presentation scope");
          }else if(key=="ordered_access"||key=="coherent_access"){
            if(v!="true" && v!="false")throw std::invalid_argument("invalid QoS boolean");
            (key=="ordered_access"?q.ordered_access:q.coherent_access)=(v=="true");
          }else if(key=="partition"){
            q.partition=v;
          }else throw std::invalid_argument("unknown QoS field");
          continue;
        }
        throw std::invalid_argument("invalid QoS profile indentation");
      }
      if(line.rfind("  ",0)!=0||line.rfind("   ",0)==0)
        throw std::invalid_argument("UMAA configuration must use exact two-space indentation");
      const auto entry=line.substr(2);
      if(section==Section::Middleware){
        const auto colon=entry.find(':');
        if(colon==std::string::npos)throw std::invalid_argument("invalid middleware setting");
        const auto key=trim(entry.substr(0,colon));
        const auto v=value(entry.substr(colon+1));
        if(key=="backend"){
          if(backend_set)throw std::invalid_argument("duplicate DDS backend");
          cfg.backend=(root/v).lexically_normal().string();
          backend_set=true;
        }else if(key=="domain_id"){
          if(domain_set)throw std::invalid_argument("duplicate DDS domain");
          uint64_t domain=0;
          for(unsigned char c:v){
            if(!std::isdigit(c))throw std::invalid_argument("non-numeric DDS domain");
            domain=domain*10+(c-'0');
            if(domain>230)throw std::invalid_argument("DDS domain outside portable range 0..230");
          }
          cfg.domain_id=static_cast<uint32_t>(domain);
          domain_set=true;
        }else throw std::invalid_argument("unknown middleware setting");
      }else if(section==Section::Modules){
        if(entry.rfind("- ",0)!=0)throw std::invalid_argument("invalid component module entry");
        auto path=(root/value(entry.substr(2))).lexically_normal().string();
        if(!modules_seen.insert(path).second)throw std::invalid_argument("duplicate module path");
        cfg.modules.push_back(std::move(path));
      }else throw std::invalid_argument("configuration entry outside a section");
    }
    if(!in.eof()&&in.fail())throw std::runtime_error("configuration read failed");
    if(!backend_set||!domain_set)throw std::invalid_argument("DDS backend and domain_id required");
    for(const auto& entry:cfg.qos_profiles)umaa::qos::validate(entry.second);
    return cfg;
  }
};
} // namespace umaa::internal
