// Component ABI v2 business Handler -> CommandEngine status/ack pipeline.
// This is the host-side bridge. CommandTransport still needs a reviewed DDS
// writer/reader TypeBinding; this header alone does NOT implement wire protocol.
#pragma once
#include "umaa/command_transport.hpp"
#include "umaa/component.hpp"
#include "umaa/internal/component_runtime.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace umaa {
template<class Service> class ComponentCommandProvider final {
  using Payload=typename Service::Command;
  struct Executor final:Handler<Payload> {
    std::shared_ptr<internal::ComponentRuntime> runtime;
    std::unordered_map<std::string,std::uint64_t> session_ids;
    explicit Executor(std::shared_ptr<internal::ComponentRuntime> host)
      :runtime(std::move(host)) {
      if(!runtime)throw std::invalid_argument("missing Component runtime");
    }
    static std::uint64_t next_number() {
      static std::atomic<std::uint64_t> counter{1};
      const auto id=counter.fetch_add(1,std::memory_order_relaxed);
      if(!id)throw std::overflow_error("component session identifiers exhausted");
      return id;
    }
    bool validate(const Payload&) override {return true;}
    Outcome execute(const Payload&) override {
      throw std::logic_error("session identity required for device command");
    }
    bool cancel(const Payload&) override {return false;}
    Outcome execute_session(const Identity& id,const Payload& command) override {
      auto [it,inserted]=session_ids.emplace(id.session,0);
      if(inserted)it->second=next_number();
      const auto bytes=Service::encode_command(command);
      std::vector<std::uint8_t> output;
      std::int32_t business=-1;
      if(runtime->call(Service::id,UMAA_COMPONENT_COMMAND,bytes,output,business,
                       it->second)!=UMAA_COMPONENT_OK)
        throw std::runtime_error("Component COMMAND failed");
      switch(static_cast<Result>(business)) {
        case Result::Completed:return Outcome::Completed;
        case Result::Pending:return Outcome::Pending;
        case Result::Rejected:
        case Result::Failed:return Outcome::Failed;
      }
      throw std::runtime_error("unrecognized Component command result");
    }
    bool cancel_session(const Identity& id,const Payload&) override {
      auto it=session_ids.find(id.session);
      if(it==session_ids.end())return false;
      std::vector<std::uint8_t> empty,output;
      std::int32_t business=-1;
      try {
        if(runtime->call(Service::id,UMAA_COMPONENT_CANCEL,empty,output,business,
                         it->second)!=UMAA_COMPONENT_OK)return false;
        // An ambiguous Failed result never proves hardware cancellation.
        return business==static_cast<std::int32_t>(Result::Completed);
      }catch(...){return false;}
    }
    Result poll(const std::string& session) {
      const auto it=session_ids.find(session);
      if(it==session_ids.end())throw std::out_of_range("untracked device session");
      std::vector<std::uint8_t> empty,output;
      std::int32_t business=-1;
      if(runtime->call(Service::id,UMAA_COMPONENT_COMMAND_POLL,empty,output,
                       business,it->second)!=UMAA_COMPONENT_OK)
        throw std::runtime_error("Component session poll failed");
      if(business<static_cast<std::int32_t>(Result::Completed) ||
         business>static_cast<std::int32_t>(Result::Failed))
        throw std::runtime_error("invalid Component session status");
      return static_cast<Result>(business);
    }
  };
public:
  ComponentCommandProvider(std::string provider,
      std::shared_ptr<internal::ComponentRuntime> host,CommandTransport<Payload>& wire)
    :executor_(std::move(host)),provider_(std::move(provider),executor_,wire){}
  std::vector<Event> receive(const Identity& id,const Payload& payload) {
    ensure_active();
    try{return provider_.receive(id,payload);}
    catch(...){faulted_=true;throw;}
  }
  std::vector<Event> poll_device(const std::string& session) {
    ensure_active();
    try {
      const auto state=executor_.poll(session);
      if(state==Result::Pending)
        return {{Event::Kind::Ignored,session,"DEVICE_PENDING",""}};
      return provider_.complete(session,state==Result::Completed,
                                state==Result::Completed?"SUCCEEDED":"OBJECTIVE_FAILED");
    }catch(...){faulted_=true;throw;}
  }
  std::vector<Event> command_disposed(const std::string& source,
                                      const std::string& session) {
    ensure_active();
    try {
      auto events=provider_.command_disposed(source,session);
      if(!provider_.faulted() && !provider_.sessions())
        executor_.session_ids.clear();
      return events;
    }catch(...){faulted_=true;throw;}
  }
  bool faulted() const noexcept {return faulted_||provider_.faulted();}
  std::size_t sessions() const{return provider_.sessions();}
private:
  void ensure_active() const {
    if(faulted())throw std::logic_error("Component command provider faulted");
  }
  Executor executor_;
  BoundCommandProvider<Payload> provider_;
  bool faulted_{false};
};
} // namespace umaa
