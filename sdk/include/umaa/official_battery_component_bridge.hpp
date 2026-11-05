// Host-only, non-normative ABI-v2 projection of the reviewed BatteryStatus
// Cyclone Reader's complete StableList value into a business Component.
// This is NOT DDS CDR, a UMAA official schema, generated service Runtime,
// selected Component profile, native GROUP coherence or formal conformance.
#pragma once
#include "umaa/component_module.hpp"
#include "umaa/official_battery_subscription.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace umaa::official_battery {
namespace preview_detail {
struct Writer {
  std::vector<std::uint8_t> b;
  void u8(std::uint8_t n) { b.push_back(n); }
  void u32(std::uint32_t n) { for (int i=0;i<4;++i) u8(static_cast<std::uint8_t>(n >> (8*i))); }
  void u64(std::uint64_t n) { for (int i=0;i<8;++i) u8(static_cast<std::uint8_t>(n >> (8*i))); }
  void i32(std::int32_t n) { std::uint32_t v; std::memcpy(&v,&n,sizeof(v)); u32(v); }
  void i64(std::int64_t n) { std::uint64_t v; std::memcpy(&v,&n,sizeof(v)); u64(v); }
  void f64(double d) {
    static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,
                  "host preview requires binary64");
    if (!std::isfinite(d)) throw std::invalid_argument("non-finite Battery preview number");
    std::uint64_t bits; std::memcpy(&bits,&d,8); u64(bits);
  }
  void guid(const Guid& g) { b.insert(b.end(),g.begin(),g.end()); }
  void time(const DateTime& t) {
    if (t.nanoseconds<0 || t.nanoseconds>999999999)
      throw std::invalid_argument("Battery preview timestamp out of range");
    i64(t.seconds); i32(t.nanoseconds);
  }
  void opt_time(const std::optional<DateTime>& t) {
    u8(t?1:0); if(t) time(*t);
  }
  void opt_guid(const std::optional<Guid>& g) {
    u8(g?1:0); if(g) guid(*g);
  }
  void opt_double(const std::optional<double>& d) {
    u8(d?1:0); if(d) f64(*d);
  }
};
struct Reader {
  const std::uint8_t* p; std::size_t n, at{0};
  bool take(std::size_t count) const noexcept { return count<=n-at; }
  std::uint8_t u8() {
    if(!take(1))throw std::invalid_argument("truncated host Battery preview");
    return p[at++];
  }
  std::uint32_t u32() {
    std::uint32_t v=0; for(int i=0;i<4;++i)v|=std::uint32_t(u8())<<(i*8); return v;
  }
  std::uint64_t u64() {
    std::uint64_t v=0; for(int i=0;i<8;++i)v|=std::uint64_t(u8())<<(i*8); return v;
  }
  std::int32_t i32() {
    auto v=u32(); std::int32_t r; std::memcpy(&r,&v,4); return r;
  }
  std::int64_t i64() {
    auto v=u64(); std::int64_t r; std::memcpy(&r,&v,8); return r;
  }
  double f64() {
    auto v=u64(); double r; std::memcpy(&r,&v,8);
    if(!std::isfinite(r))throw std::invalid_argument("non-finite host Battery preview");
    return r;
  }
  Guid guid() {
    if(!take(16))throw std::invalid_argument("truncated Battery GUID");
    Guid g{};std::memcpy(g.data(),p+at,16);at+=16;return g;
  }
  DateTime time() {
    DateTime t{i64(),i32()};
    if(t.nanoseconds<0||t.nanoseconds>999999999)
      throw std::invalid_argument("invalid Battery timestamp");
    return t;
  }
  bool flag() {
    auto f=u8();if(f>1)throw std::invalid_argument("invalid optional flag");return f==1;
  }
  std::optional<DateTime> opt_time() {return flag()?std::optional<DateTime>(time()):std::nullopt;}
  std::optional<Guid> opt_guid() {return flag()?std::optional<Guid>(guid()):std::nullopt;}
  std::optional<double> opt_double() {return flag()?std::optional<double>(f64()):std::nullopt;}
};
} // namespace preview_detail

// ABI payload is a canonical **host-local** record with all currently
// represented BatteryStatus Host Report fields and each ordered Cell.
// The original vendor TypeSupport still performs actual DDS wire decoding.
struct BatteryStatusHostPreview {
  static constexpr const char* id="UMAA::EO::BatteryStatus";
  static constexpr const char* schema_id="umaa-kit:host-only-battery-stable-v1:NOT-UMAA-CDR";
  using Report=StableList;
  static std::vector<std::uint8_t> encode_report(const Report& v) {
    if (v.nativeDdsCoherenceProved || !v.generation || v.ordered.size()>4096 ||
        v.report.source.id==Guid{} ||
        v.report.cellsListMetadata.listID==Guid{} ||
        v.report.cellsListMetadata.size<0 ||
        v.ordered.size()!=static_cast<std::size_t>(v.report.cellsListMetadata.size))
      throw std::invalid_argument("invalid Battery host snapshot for ABI");
    preview_detail::Writer w;
    w.u8('U');w.u8('B');w.u8('H');w.u8(1);
    const auto& r=v.report;
    w.guid(r.source.id);w.guid(r.source.parentID);
    w.guid(r.cellsListMetadata.listID);
    w.u64(v.generation);w.time(r.timeStamp);w.i32(static_cast<std::int32_t>(r.state));
    w.opt_double(r.chargeRemaining);w.opt_double(r.current);
    w.opt_double(r.energyUsageRate);w.opt_double(r.hours);
    w.opt_double(r.temp);w.opt_double(r.voltage);
    w.guid(r.cellsListMetadata.updateElementID);
    w.opt_time(r.cellsListMetadata.updateElementTimestamp);
    w.guid(r.cellsListMetadata.startingElementID);
    w.i32(r.cellsListMetadata.size);
    w.u32(static_cast<std::uint32_t>(v.ordered.size()));
    for(const auto& e:v.ordered) {
      if(e.listID!=r.cellsListMetadata.listID)
        throw std::invalid_argument("Battery preview element belongs to foreign List");
      w.guid(e.listID);w.guid(e.elementID);w.time(e.elementTimestamp);
      w.opt_guid(e.nextElementID);
      w.f64(e.element.current);w.f64(e.element.temperature);w.f64(e.element.voltage);
    }
    return std::move(w.b);
  }
  static bool decode_report(const std::uint8_t* p,std::size_t n,Report& out) noexcept {
    if(!p||n<4||n>512*1024)return false;
    try {
      preview_detail::Reader r{p,n};
      if(r.u8()!='U'||r.u8()!='B'||r.u8()!='H'||r.u8()!=1)return false;
      Report v{};
      auto& b=v.report;
      b.source.id=r.guid();b.source.parentID=r.guid();
      b.cellsListMetadata.listID=r.guid();
      v.generation=r.u64();b.timeStamp=r.time();
      b.state=static_cast<State>(r.i32());
      b.chargeRemaining=r.opt_double();b.current=r.opt_double();
      b.energyUsageRate=r.opt_double();b.hours=r.opt_double();
      b.temp=r.opt_double();b.voltage=r.opt_double();
      b.cellsListMetadata.updateElementID=r.guid();
      b.cellsListMetadata.updateElementTimestamp=r.opt_time();
      b.cellsListMetadata.startingElementID=r.guid();
      b.cellsListMetadata.size=r.i32();
      const auto count=r.u32();
      if(!v.generation||b.source.id==Guid{}||b.cellsListMetadata.listID==Guid{}||
         static_cast<std::int32_t>(b.state)<0||static_cast<std::int32_t>(b.state)>2||
         b.cellsListMetadata.size<0||count>4096||
         count!=static_cast<std::uint32_t>(b.cellsListMetadata.size))
        return false;
      std::set<Guid> seen;
      for(std::uint32_t i=0;i<count;++i){
        ListElement e{};
        e.listID=r.guid();e.elementID=r.guid();
        e.elementTimestamp=r.time();e.nextElementID=r.opt_guid();
        e.element.current=r.f64();e.element.temperature=r.f64();e.element.voltage=r.f64();
        if(e.listID!=b.cellsListMetadata.listID||e.elementID==Guid{}||
           !seen.insert(e.elementID).second)return false;
        if(i+1<count && (!e.nextElementID ||
            *e.nextElementID==Guid{}))return false;
        if(i+1==count && e.nextElementID)return false;
        v.ordered.push_back(std::move(e));
      }
      if(r.at!=r.n)return false;
      if(count && v.ordered.front().elementID!=b.cellsListMetadata.startingElementID)
        return false;
      for(std::size_t i=1;i<v.ordered.size();++i)
        if(v.ordered[i-1].nextElementID!=v.ordered[i].elementID)return false;
      out=std::move(v);return true;
    } catch (...) { return false; }
  }
};

// Business Module only receives verified, immutable completed snapshots and
// *actual* keyed Report Dispose. ABI v2 has no invalidate action; this bridge
// therefore never represents intermediate updates as an accepted current
// snapshot and makes no real-time cache coherence or update guarantee.
class BatteryStatusComponentBridge final : public BatteryStatusHandler {
 public:
  explicit BatteryStatusComponentBridge(::umaa::ComponentModule& module)
      : module_(module) {
    bool found=false;
    for(const auto& service:module.services())
      if(service.service==BatteryStatusHostPreview::id &&
         service.schema==BatteryStatusHostPreview::schema_id &&
         (service.actions & (8u|16u))==(8u|16u))
        found=true;
    if(!found)throw std::invalid_argument(
        "Component lacks exact host-only BatteryStatus Consumer ABI contract");
  }
  void on_stable_list(const StableList& v,ReaderEpoch epoch) override {
    require_epoch(epoch);
    try{
      auto bytes=BatteryStatusHostPreview::encode_report(v);
      const auto& s=v.report.source;
      const auto list=v.report.cellsListMetadata.listID;
      if(retired_.count(s.id))throw std::runtime_error("retired Battery Source reappeared");
      auto it=sources_.find(s.id);
      if(it==sources_.end()){
        if(next_token_==0 || next_token_==std::numeric_limits<std::uint64_t>::max()||
           sources_.size()==128)
          throw std::runtime_error("Battery ABI token capacity exhausted");
        for(const auto& kv:sources_)
          if(kv.second.list==list)throw std::runtime_error("Battery List owner collision");
        it=sources_.emplace(s.id,Entry{s.parentID,list,next_token_++,0}).first;
      } else if(it->second.parent!=s.parentID || it->second.list!=list ||
                v.generation<=it->second.generation)
        throw std::runtime_error("Battery ABI Source binding or generation changed");
      it->second.generation=v.generation;
      invoke(UMAA_COMPONENT_REPORT_ACCEPT,bytes,it->second.token);
      ++accepted_;
    }catch(...){faulted_=true;throw;}
  }
  void on_list_invalidated(const Guid& list,ReaderEpoch epoch) override {
    require_epoch(epoch);
    // ABI-v2 cannot signal a non-Dispose invalidation. Mark that the
    // application must NOT rely on latest cached content as current.
    invalidated_.insert(list);
  }
  void on_source_disposed(const Identifier& source,const Guid& list,
                          ReaderEpoch epoch) override {
    require_epoch(epoch);
    try {
      auto it=sources_.find(source.id);
      if(it==sources_.end()||it->second.parent!=source.parentID||
         it->second.list!=list)
        throw std::runtime_error("unbound or spoofed native Battery Dispose");
      invoke(UMAA_COMPONENT_REPORT_DISPOSE,{},it->second.token);
      sources_.erase(it);
      retired_.insert(source.id);
      invalidated_.erase(list);
      ++disposed_;
    }catch(...){faulted_=true;throw;}
  }
  std::size_t accepted() const noexcept{return accepted_;}
  std::size_t disposed() const noexcept{return disposed_;}
  std::size_t active_sources() const noexcept{return sources_.size();}
  bool current_known(const Guid& list) const noexcept {
    return !faulted_ && !invalidated_.count(list);
  }
  bool faulted() const noexcept{return faulted_;}
 private:
  struct Entry{ Guid parent,list; std::uint64_t token,generation; };
  void require_epoch(ReaderEpoch e){
    if(faulted_||!e||(epoch_&&epoch_!=e)){
      faulted_=true;
      throw std::runtime_error("Battery ABI reader epoch is stale or faulted");
    }
    epoch_=e;
  }
  void invoke(std::uint32_t action,const std::vector<std::uint8_t>& payload,
              std::uint64_t token){
    std::vector<std::uint8_t> output;
    std::int32_t result=-1;
    const auto status=module_.call(BatteryStatusHostPreview::id,action,
                                  payload,output,result,token);
    if(status!=UMAA_COMPONENT_OK ||
       result!=static_cast<std::int32_t>(::umaa::Result::Completed) ||
       !output.empty())
      throw std::runtime_error("business Battery ABI module refused Host event");
  }
  ::umaa::ComponentModule& module_;
  ReaderEpoch epoch_{0};
  std::uint64_t next_token_{1};
  std::size_t accepted_{0},disposed_{0};
  bool faulted_{false};
  std::map<Guid,Entry> sources_;
  std::set<Guid> retired_,invalidated_;
};
} // namespace umaa::official_battery
