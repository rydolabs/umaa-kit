// UMAA-Kit template API v1. Reuses existing protocol engines; never duplicates
// the Command FSM or claims that these facades are DDS endpoints.
#pragma once
#include "umaa/sdk_v1_core.hpp"
#include "umaa/command_engine.hpp"
#include "umaa/remote_command_monitor.hpp"
#include "umaa/report_engine.hpp"
#include "umaa/large_collection.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace umaa::sdk_v1 {
inline std::string canonical_id(const Identifier& id) {
  if(id.id.is_nil())throw std::invalid_argument("missing UMAA entity ID");
  return id.parent_id.str()+"/"+id.id.str();
}
inline umaa::Identity legacy_command_identity(const CommandIdentity& id){
  if(!id.valid())throw std::invalid_argument("invalid UMAA Command identity");
  return {canonical_id(id.source),canonical_id(id.destination),
          id.session_id.str(),id.timestamp_ns};
}
inline void require_flow(const ServiceDescriptor& d,FlowKind expected){
  if(d.flow!=expected)throw std::invalid_argument("service flow contract mismatch");
}

template<class Service> class ReportProvider final {
public:
  using Report=typename ServiceTraits<Service>::Report;
  explicit ReportProvider(std::string source,const umaa::ReportHandler<Report>& handler)
    :lifecycle_(std::move(source),handler) {
    require_flow(descriptor<Service>(),FlowKind::QuerylessRequestReply);
  }
  std::vector<umaa::ReportIntent<Report>> start(std::int64_t timestamp) {
    return lifecycle_.start(timestamp);
  }
  std::vector<umaa::ReportIntent<Report>> start_unrecoverable(std::int64_t timestamp) {
    return lifecycle_.start_unrecoverable(timestamp);
  }
  umaa::ReportIntent<Report> update(std::int64_t timestamp) {
    return lifecycle_.update(timestamp);
  }
  std::vector<umaa::ReportIntent<Report>> stop(std::int64_t timestamp) {
    return lifecycle_.shutdown(timestamp);
  }
private:
  umaa::ReportProviderLifecycle<Report> lifecycle_;
};

template<class Service> class ReportConsumer final {
public:
  using Report=typename ServiceTraits<Service>::Report;
  ReportConsumer() {require_flow(descriptor<Service>(),FlowKind::QuerylessRequestReply);}
  std::uint64_t start(){lifecycle_.start();return lifecycle_.epoch();}
  bool receive(const umaa::ReportSample<Report>& sample,std::uint64_t epoch){
    return lifecycle_.receive(sample,epoch);
  }
  bool dispose(const std::string& source,std::int64_t timestamp,std::uint64_t epoch){
    return lifecycle_.receive_dispose(source,timestamp,epoch);
  }
  std::optional<umaa::ReportSample<Report>> latest(const std::string& source) const {
    return lifecycle_.cache().latest(source);
  }
  void stop(){lifecycle_.shutdown();}
private:
  umaa::ReportConsumerLifecycle<Report> lifecycle_;
};

template<class Service> class CommandProvider final {
public:
  using Command=typename ServiceTraits<Service>::Command;
  CommandProvider(Identifier provider,umaa::Handler<Command>& handler)
    :engine_(canonical_id(provider),handler){
    require_flow(descriptor<Service>(),FlowKind::CommandResponse);
  }
  std::vector<umaa::Event> receive(const CommandIdentity& id,const Command& command){
    return engine_.receive(legacy_command_identity(id),command);
  }
  std::vector<umaa::Event> complete(Guid128 session,bool success,const std::string& reason="OBJECTIVE_FAILED"){
    return engine_.complete(session.str(),success,reason);
  }
  std::vector<umaa::Event> timeout(Guid128 session){return engine_.timeout(session.str());}
  std::vector<umaa::Event> command_disposed(Identifier source,Guid128 session){
    return engine_.command_disposed(canonical_id(source),session.str());
  }
  std::vector<umaa::Event> fail_unrecoverable(){return engine_.fail_unrecoverable();}
  std::size_t active_sessions() const {return engine_.size();}
private:
  umaa::CommandEngine<Command> engine_;
};

template<class Service> class CommandConsumer final {
public:
  explicit CommandConsumer(std::size_t max_sessions=1024):monitor_(max_sessions){
    require_flow(descriptor<Service>(),FlowKind::CommandResponse);
  }
  void issue(const CommandIdentity& id){monitor_.issue(legacy_command_identity(id));}
  bool observe_status(Guid128 session,Identifier provider,umaa::State state,std::int64_t timestamp){
    return monitor_.status(session.str(),canonical_id(provider),state,timestamp);
  }
  bool observe_ack(Guid128 session,Identifier provider,std::int64_t timestamp){
    return monitor_.ack(session.str(),canonical_id(provider),timestamp);
  }
  umaa::RemoteCommandResult result(Guid128 session) const{return monitor_.result(session.str());}
  bool ready_for_update(Guid128 session) const{return monitor_.ready_for_update(session.str());}
  void begin_update(Guid128 session){monitor_.begin_update(session.str());}
  bool disposed_status(Guid128 session,Identifier provider){
    return monitor_.disposed_status(session.str(),canonical_id(provider));
  }
  bool disposed_ack(Guid128 session,Identifier provider){
    return monitor_.disposed_ack(session.str(),canonical_id(provider));
  }
  bool release(Guid128 session){return monitor_.release(session.str());}
  void ambiguous_delivery(Guid128 session){monitor_.ambiguous_delivery(session.str());}
  bool needs_recovery(Guid128 session) const{return monitor_.needs_recovery(session.str());}
private:
  umaa::RemoteCommandMonitor monitor_;
};

template<class Service> class CollectionConsumer final {
public:
  using Element=typename ServiceTraits<Service>::Element;
  explicit CollectionConsumer(std::size_t maximum=10000)
    :assembler_(shape(),maximum) {}
  bool upsert(umaa::CollectionElement<Element> item){return assembler_.upsert(std::move(item));}
  bool dispose(const std::string& id,std::int64_t timestamp){
    return assembler_.dispose(id,timestamp);
  }
  void metadata(umaa::CollectionMetadata m){assembler_.metadata(std::move(m));}
  std::optional<std::vector<Element>> snapshot() const {return assembler_.snapshot();}
  std::uint64_t commit_count() const {return assembler_.commit_count();}
  void clear(){assembler_.clear();}
private:
  static umaa::CollectionKind shape(){
    const auto s=descriptor<Service>().collection;
    if(s==CollectionShape::LargeSet)return umaa::CollectionKind::Set;
    if(s==CollectionShape::LargeList)return umaa::CollectionKind::List;
    throw std::invalid_argument("service has no Large Collection contract");
  }
  umaa::CollectionAssembler<Element> assembler_;
};

} // namespace umaa::sdk_v1
