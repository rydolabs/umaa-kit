// Strict SDK runtime configuration subset, not a general YAML parser.
// Only middleware.backend, middleware.domain_id, and modules: - path.
// Fail closed on unknown keys, duplicate settings, excessive input or
// malformed/ambiguous values. No dynamic evaluation, file inclusion or
// environment-variable expansion. Paths relative to this file.
#pragma once
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
namespace umaa::internal {
struct RuntimeConfig {
  std::string backend;
  uint32_t domain_id{};
  std::vector<std::string> modules;
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
    enum class Section { None, Middleware, Modules } section=Section::None;
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
    return cfg;
  }
};
} // namespace umaa::internal
