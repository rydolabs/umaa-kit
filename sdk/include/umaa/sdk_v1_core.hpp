// UMAA-Kit SDK core API v1: vendor-neutral contracts, identity, and correlation.
// This is application-layer logic, not a DDS TypeSupport or UMAA conformance claim.
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace umaa::sdk_v1 {

// Official UMAA NumericGUID is an octet[16]. Preserve all 128 bits: never
// truncate a session, source, or specialization identity to uint64_t.
struct Guid128 {
  std::array<std::uint8_t,16> bytes{};
  bool is_nil() const noexcept {
    for (auto b : bytes) if (b) return false;
    return true;
  }
  friend bool operator==(const Guid128& a,const Guid128& b) noexcept {return a.bytes==b.bytes;}
  friend bool operator!=(const Guid128& a,const Guid128& b) noexcept {return !(a==b);}
  friend bool operator<(const Guid128& a,const Guid128& b) noexcept {return a.bytes<b.bytes;}
  static Guid128 parse(std::string_view s) {
    if(s.size()!=36 || s[8]!='-' || s[13]!='-' || s[18]!='-' || s[23]!='-')
      throw std::invalid_argument("invalid 128-bit GUID format");
    auto nibble=[](char c)->std::uint8_t {
      if(c>='0'&&c<='9')return static_cast<std::uint8_t>(c-'0');
      if(c>='a'&&c<='f')return static_cast<std::uint8_t>(c-'a'+10);
      if(c>='A'&&c<='F')return static_cast<std::uint8_t>(c-'A'+10);
      throw std::invalid_argument("invalid GUID hex digit");
    };
    Guid128 out; std::size_t n=0;
    for(std::size_t i=0;i<s.size();) {
      if(s[i]=='-') {++i;continue;}
      out.bytes[n++]=static_cast<std::uint8_t>((nibble(s[i])<<4)|nibble(s[i+1]));
      i+=2;
    }
    return out;
  }
  std::string str() const {
    constexpr char hex[]="0123456789abcdef";
    std::string s; s.reserve(36);
    for(std::size_t i=0;i<bytes.size();++i){
      if(i==4||i==6||i==8||i==10)s.push_back('-');
      s.push_back(hex[bytes[i]>>4]);s.push_back(hex[bytes[i]&0x0f]);
    }
    return s;
  }
};
struct Identifier {
  Guid128 id{};
  Guid128 parent_id{};
  friend bool operator==(const Identifier& a,const Identifier& b) noexcept {
    return a.id==b.id && a.parent_id==b.parent_id;
  }
};
struct CommandIdentity {
  Identifier source{};
  Identifier destination{};
  Guid128 session_id{};
  std::int64_t timestamp_ns{};
  bool valid() const noexcept {
    return !source.id.is_nil() && !destination.id.is_nil() &&
           !session_id.is_nil() && timestamp_ns>0;
  }
};

enum class FlowKind { CommandResponse, QuerylessRequestReply, QueryDataRequestReply };
enum class ServiceKind { Config, Control, Specs, Status, Report };
enum class CollectionShape { None, LargeSet, LargeList };
struct ServiceDescriptor {
  std::string_view id;
  std::string_view schema_id;
  FlowKind flow;
  ServiceKind kind;
  CollectionShape collection{CollectionShape::None};
  bool has_specializations{false};
};

template<class Service> struct ServiceTraits;  // generated from reviewed ICD/IDL later

template<class S,class=void> struct has_service_traits : std::false_type {};
template<class S> struct has_service_traits<S,std::void_t<decltype(ServiceTraits<S>::descriptor)>>
  : std::true_type {};

template<class Service>
const ServiceDescriptor& descriptor() {
  static_assert(has_service_traits<Service>::value,
    "ServiceTraits<Service>::descriptor must be explicitly defined");
  const auto& d=ServiceTraits<Service>::descriptor;
  if(d.id.empty()||d.schema_id.empty())
    throw std::invalid_argument("incomplete UMAA service contract");
  return d;
}

// A component can expose any mix of services; duplicate IDs must never be
// accepted with different type/flow mappings.
class ServiceCatalog final {
public:
  template<class Service> void add() {
    const auto& d=descriptor<Service>();
    const auto inserted=entries_.emplace(std::string(d.id),d);
    if(!inserted.second)throw std::invalid_argument("duplicate service contract");
  }
  bool contains(std::string_view id) const {return entries_.count(std::string(id))!=0;}
  const ServiceDescriptor& at(std::string_view id) const {return entries_.at(std::string(id));}
  std::size_t size() const noexcept {return entries_.size();}
private:
  std::map<std::string,ServiceDescriptor> entries_;
};

// Generalization and specialization are separate DDS topics. A matching
// specialization is visible ONLY when both halves carry the same revision.
// Time/order checks are per identity and use a bounded tombstone cache.
struct SpecializationRef {
  Guid128 id{};
  std::int64_t timestamp_ns{};
};
template<class General,class Specialized> class SpecializationJoin final {
public:
  struct Pair {General general; Specialized specialization; std::int64_t timestamp_ns;};
  explicit SpecializationJoin(std::size_t max_instances=10000)
    :max_instances_(max_instances) {
    if(max_instances_==0)throw std::invalid_argument("zero specialization capacity");
  }
  bool put_general(Guid128 id,std::int64_t time,General value){
    return put(id,time,std::move(value),true);
  }
  bool put_specialization(Guid128 id,std::int64_t time,Specialized value){
    return put(id,time,std::move(value),false);
  }
  // Do not infer that DDS UNREGISTER means a confirmed entity deletion.
  bool dispose(Guid128 id,std::int64_t time){
    require_identity(id,time);
    auto it=slots_.find(id);
    if(it==slots_.end()){
      if(slots_.size()>=max_instances_)throw std::length_error("specialization capacity exceeded");
      it=slots_.emplace(id,Slot{}).first;
    }
    Slot& s=it->second;
    const auto general_time=s.general ? s.general->first : std::int64_t{0};
    const auto specialized_time=s.specialized ? s.specialized->first : std::int64_t{0};
    if(time<=s.watermark || time<=general_time || time<=specialized_time)return false;
    s.watermark=time;s.general.reset();s.specialized.reset();
    return true;
  }
  std::optional<Pair> resolve(SpecializationRef ref) const {
    if(ref.id.is_nil()||ref.timestamp_ns<=0)return std::nullopt;
    const auto it=slots_.find(ref.id);
    if(it==slots_.end())return std::nullopt;
    const Slot& s=it->second;
    if(!s.general || !s.specialized || s.general->first!=ref.timestamp_ns ||
       s.specialized->first!=ref.timestamp_ns)return std::nullopt;
    return Pair{s.general->second,s.specialized->second,ref.timestamp_ns};
  }
  // The logical collection is visible only after the collection assembler
  // committed AND every referenced specialization can be resolved.
  template<class Projector>
  std::optional<std::vector<Pair>> resolve_collection(
      const std::optional<std::vector<General>>& collection,Projector project) const {
    if(!collection)return std::nullopt;
    std::vector<Pair> out;out.reserve(collection->size());
    for(const auto& item:*collection){
      auto pair=resolve(project(item));
      if(!pair)return std::nullopt;
      out.push_back(std::move(*pair));
    }
    return out;
  }
  std::size_t tracked() const noexcept{return slots_.size();}
private:
  struct Slot {
    std::optional<std::pair<std::int64_t,General>> general;
    std::optional<std::pair<std::int64_t,Specialized>> specialized;
    std::int64_t watermark{0};
  };
  static void require_identity(Guid128 id,std::int64_t time){
    if(id.is_nil()||time<=0)throw std::invalid_argument("invalid specialization identity/timestamp");
  }
  template<class Value>
  bool put(Guid128 id,std::int64_t time,Value value,bool is_general){
    require_identity(id,time);
    auto it=slots_.find(id);
    if(it==slots_.end()){
      if(slots_.size()>=max_instances_)throw std::length_error("specialization capacity exceeded");
      it=slots_.emplace(id,Slot{}).first;
    }
    Slot& s=it->second;
    if(time<=s.watermark)return false;
    // A new revision invalidates the opposite half until it catches up.
    if constexpr(std::is_same_v<Value,General> && std::is_same_v<Value,Specialized>) {
      if(is_general){
        if(s.general&&time<=s.general->first)return false;
        s.general=std::make_pair(time,std::move(value));
      }else{
        if(s.specialized&&time<=s.specialized->first)return false;
        s.specialized=std::make_pair(time,std::move(value));
      }
    }else if constexpr(std::is_same_v<Value,General>){
      (void)is_general;
      if(s.general&&time<=s.general->first)return false;
      s.general=std::make_pair(time,std::move(value));
    }else{
      (void)is_general;
      if(s.specialized&&time<=s.specialized->first)return false;
      s.specialized=std::make_pair(time,std::move(value));
    }
    return true;
  }
  std::size_t max_instances_;
  std::map<Guid128,Slot> slots_;
};

} // namespace umaa::sdk_v1
