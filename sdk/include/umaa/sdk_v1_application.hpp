// Typed SDK v1 -> actual Application / ComponentRuntime / DDS plugin bridge.
// WireService is an explicitly reviewed TypeBinding. Nothing in here guesses
// IDL mappings, constructs a DDS entity in business code, or changes C ABIs.
#pragma once
#include "umaa/application.hpp"
#include "umaa/sdk_v1_core.hpp"
#include "umaa/native_collection_endpoint.hpp"
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace umaa::sdk_v1 {

// ServiceTraits<T>::WireService identifies an explicit generated/reviewed
// Application binding. The bridge checks the binding ID and schema every time
// an endpoint is created, failing closed instead of aliasing a similar Topic.
template<class Service> struct RuntimeBinding {
  using Traits=ServiceTraits<Service>;
  using Wire=typename Traits::WireService;
  static const ServiceDescriptor& checked(FlowKind flow) {
    const auto& d=descriptor<Service>();
    if(d.flow!=flow || std::string_view(Wire::id)!=d.id)
      throw std::invalid_argument("SDK v1 ServiceTraits/WireService mismatch");
    if constexpr(Wire::is_command) {
      if(flow!=FlowKind::CommandResponse ||
         std::string_view(Wire::command_schema)!=d.schema_id)
        throw std::invalid_argument("SDK v1 Command schema mismatch");
    }else {
      if(flow!=FlowKind::QuerylessRequestReply ||
         std::string_view(Wire::schema_id)!=d.schema_id)
        throw std::invalid_argument("SDK v1 Report schema mismatch");
    }
    return d;
  }
};

class RuntimeApplication final {
public:
  RuntimeApplication()=default;
  void use_backend(const std::string& path,std::uint32_t domain){
    app_.use_backend(path,domain);
    backend_path_=path;domain_id_=domain;
  }
  void load_component(const std::string& file){app_.load_component(file);}
  void configure_qos_profiles(
      const std::unordered_map<std::string,umaa::qos::Policy>& profiles){
    app_.configure_qos_profiles(profiles);
  }
  // For a Command service, the SDK owns the Request Reader, Status/Ack
  // writers, FSM and Component COMMAND/CANCEL/POLL dispatch. Business code
  // remains in the loaded shared object.
  template<class Service>
  auto provide_command(const std::string& source,const std::string& provider,
      std::chrono::milliseconds retirement_delay=std::chrono::milliseconds(180),
      std::chrono::milliseconds poll_interval=std::chrono::milliseconds(15)) {
    using Wire=typename RuntimeBinding<Service>::Wire;
    static_assert(Wire::is_command,"provide_command requires a Command TypeBinding");
    static_assert(std::is_same_v<typename ServiceTraits<Service>::Command,
                                 typename Wire::Command>,
                  "ServiceTraits Command must match the TypeBinding payload");
    (void)RuntimeBinding<Service>::checked(FlowKind::CommandResponse);
    return app_.start_remote<Wire>(source,provider,retirement_delay,poll_interval);
  }
  // Cooperative host event loop: useful for deterministic cancel/update
  // tests and embedded runtimes that already own a scheduler.
  template<class Service>
  auto serve_command(const std::string& source,const std::string& provider,
      std::chrono::milliseconds retirement_delay=std::chrono::milliseconds(180)) {
    using Wire=typename RuntimeBinding<Service>::Wire;
    static_assert(Wire::is_command,"serve_command requires a Command TypeBinding");
    static_assert(std::is_same_v<typename ServiceTraits<Service>::Command,
                                 typename Wire::Command>,
                  "ServiceTraits Command must match the TypeBinding payload");
    (void)RuntimeBinding<Service>::checked(FlowKind::CommandResponse);
    return app_.serve_remote<Wire>(source,provider,retirement_delay);
  }
  // A wire session key is explicit, not silently truncated from Guid128.
  // Official UMAA multi-key 128-bit session mapping requires a future
  // versioned DDS ABI; the current synthetic bindings use decimal keys.
  template<class Service>
  auto issue_command(const typename ServiceTraits<Service>::Command& command,
      const std::string& wire_session_key,const std::string& source,
      const std::string& destination) {
    using Wire=typename RuntimeBinding<Service>::Wire;
    static_assert(Wire::is_command,"issue_command requires a Command TypeBinding");
    static_assert(std::is_same_v<typename ServiceTraits<Service>::Command,
                                 typename Wire::Command>,
                  "ServiceTraits Command must match the TypeBinding payload");
    (void)RuntimeBinding<Service>::checked(FlowKind::CommandResponse);
    if(wire_session_key.empty())throw std::invalid_argument("empty Command session key");
    return app_.issue_remote<Wire>(command,wire_session_key,source,destination);
  }
  template<class Service>
  auto publish_report(const std::string& instance_key) {
    using Wire=typename RuntimeBinding<Service>::Wire;
    static_assert(!Wire::is_command,"publish_report requires a Report TypeBinding");
    static_assert(std::is_same_v<typename ServiceTraits<Service>::Report,
                                 typename Wire::Report>,
                  "ServiceTraits Report must match the TypeBinding payload");
    (void)RuntimeBinding<Service>::checked(FlowKind::QuerylessRequestReply);
    return app_.publish<Wire>(instance_key);
  }
  template<class Service>
  auto consume_report() {
    using Wire=typename RuntimeBinding<Service>::Wire;
    static_assert(!Wire::is_command,"consume_report requires a Report TypeBinding");
    static_assert(std::is_same_v<typename ServiceTraits<Service>::Report,
                                 typename Wire::Report>,
                  "ServiceTraits Report must match the TypeBinding payload");
    (void)RuntimeBinding<Service>::checked(FlowKind::QuerylessRequestReply);
    return app_.consume<Wire>();
  }
  // Real two-topic Element/Metadata wire, not a synthetic single-frame
  // batch. Requires a reviewed WireService with independent Topic schemas.
  template<class Service>
  auto publish_native_collection(const std::string& key) const {
    using Traits=ServiceTraits<Service>;
    using Wire=typename Traits::WireService;
    static_assert(std::is_same_v<typename Traits::Element,typename Wire::Value>,
                  "native Collection element TypeBinding mismatch");
    check_collection<Service>();
    return std::make_unique<DdsNativeCollectionPublisher<Wire>>(
      backend_path_,domain_id_,key);
  }
  template<class Service>
  auto consume_native_collection(const std::string& key,
      std::size_t maximum=1024) const {
    using Traits=ServiceTraits<Service>;
    using Wire=typename Traits::WireService;
    static_assert(std::is_same_v<typename Traits::Element,typename Wire::Value>,
                  "native Collection element TypeBinding mismatch");
    check_collection<Service>();
    return std::make_unique<DdsNativeCollectionConsumer<Wire>>(
      backend_path_,domain_id_,key,maximum);
  }
  template<class Service>
  auto publish_native_specialization() const {
    using Wire=typename ServiceTraits<Service>::WireService;
    check_collection<Service>();
    if(!descriptor<Service>().has_specializations)
      throw std::invalid_argument("no approved Specialization binding");
    return std::make_unique<DdsNativeSpecializationPublisher<Wire>>(
      backend_path_,domain_id_);
  }
  template<class Service>
  auto consume_native_specialized_collection(const std::string& key,
      std::size_t maximum=1024) const {
    using Wire=typename ServiceTraits<Service>::WireService;
    check_collection<Service>();
    if(!descriptor<Service>().has_specializations)
      throw std::invalid_argument("no approved Specialization binding");
    return std::make_unique<DdsNativeSpecializedCollectionConsumer<Wire>>(
      backend_path_,domain_id_,key,maximum);
  }
private:
  template<class Service> void check_collection() const {
    using Wire=typename ServiceTraits<Service>::WireService;
    const auto& d=descriptor<Service>();
    if(backend_path_.empty() ||
       d.collection==CollectionShape::None ||
       ((d.collection==CollectionShape::LargeSet)!=(Wire::kind==umaa::CollectionKind::Set)) ||
       d.flow!=FlowKind::QuerylessRequestReply ||
       d.schema_id!=std::string_view(Wire::metadata_schema) ||
       d.id!=std::string_view(Wire::service_id))
      throw std::invalid_argument("unapproved/mismatched native Collection binding");
  }
  umaa::Application app_;
  std::string backend_path_;
  std::uint32_t domain_id_{};
};
} // namespace umaa::sdk_v1
