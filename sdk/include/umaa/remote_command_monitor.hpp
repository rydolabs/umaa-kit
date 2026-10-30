// Transport-neutral correlation of UMAA CommandStatus and CommandAck events.
// DDS readers must be supplied by a reviewed vendor-specific TypeBinding.
// No network I/O or official wire interoperability is claimed here.
#pragma once
#include "umaa/command_engine.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
namespace umaa {
enum class RemoteCommandResult { Pending, Completed, Failed, Canceled };
class RemoteCommandMonitor final {
public:
  explicit RemoteCommandMonitor(std::size_t limit=1024):limit_(limit) {
    if(!limit)throw std::invalid_argument("zero command session limit");
  }
  void issue(const Identity& id) {
    if(id.source.empty()||id.destination.empty()||id.session.empty())
      throw std::invalid_argument("invalid command identity");
    if(sessions_.size()>=limit_)throw std::length_error("command session limit");
    auto inserted=sessions_.emplace(id.session,Entry{id});
    if(!inserted.second)throw std::invalid_argument("duplicate command session");
  }
  // Returns false for out-of-order or mismatched observations. A newer
  // illegal state transition faults only that session; it cannot become success.
  bool status(const std::string& session,const std::string& provider,
              State next,std::int64_t source_timestamp) {
    auto it=sessions_.find(session);
    if(it==sessions_.end() || it->second.id.destination!=provider)return false;
    Entry& e=it->second;
    if(e.faulted || e.status_disposed || source_timestamp<=e.last_status_time ||
       source_timestamp<e.id.timestamp)return false;
    if(e.awaiting_update && next==State::ISSUED) {
      e.status=State::ISSUED;e.awaiting_update=false;
      e.last_status_time=source_timestamp;return true;
    }
    if(e.awaiting_update || !valid(e.status,next)){
      e.faulted=true;return false;
    }
    e.status=next;
    e.last_status_time=source_timestamp;
    return true;
  }
  // ACK is correlated by provider identity and session. It is not synthesized
  // from Status; without both inputs terminal_result() remains Pending.
  bool ack(const std::string& session,const std::string& provider,
           std::int64_t source_timestamp) {
    auto it=sessions_.find(session);
    if(it==sessions_.end() || it->second.id.destination!=provider)return false;
    Entry& e=it->second;
    if(e.faulted||e.ack_disposed||e.acked ||
       source_timestamp<e.id.timestamp)return false;
    e.acked=true;return true;
  }

  bool ready_for_update(const std::string& session) const {
    const auto it=sessions_.find(session);
    if(it==sessions_.end())throw std::out_of_range("unknown Command session");
    const auto& e=it->second;
    return !e.faulted && !e.awaiting_update && e.acked && e.status &&
           *e.status==State::EXECUTING && !e.status_disposed && !e.ack_disposed;
  }
  void begin_update(const std::string& session) {
    if(!ready_for_update(session))
      throw std::logic_error("previous revision not acknowledged and executing");
    auto& e=sessions_.at(session);
    e.awaiting_update=true;e.acked=false;
  }
  bool acknowledged(const std::string& session) const {
    const auto it=sessions_.find(session);
    if(it==sessions_.end())throw std::out_of_range("unknown Command session");
    return !it->second.faulted && it->second.acked;
  }
  RemoteCommandResult result(const std::string& session) const {
    const auto it=sessions_.find(session);
    if(it==sessions_.end())throw std::out_of_range("unknown command session");
    const Entry& e=it->second;
    if(e.faulted || !e.acked || !e.status)return RemoteCommandResult::Pending;
    switch(*e.status) {
      case State::COMPLETED:return RemoteCommandResult::Completed;
      case State::FAILED:return RemoteCommandResult::Failed;
      case State::CANCELED:return RemoteCommandResult::Canceled;
      default:return RemoteCommandResult::Pending;
    }
  }
  // Lifetime retention until provider has disposed both Status and Ack.
  // Disposal is never used to fabricate a terminal business result.
  bool disposed_status(const std::string& session,const std::string& provider) {
    auto it=sessions_.find(session);
    if(it==sessions_.end()||it->second.id.destination!=provider)return false;
    it->second.status_disposed=true;
    return true;
  }
  bool disposed_ack(const std::string& session,const std::string& provider) {
    auto it=sessions_.find(session);
    if(it==sessions_.end()||it->second.id.destination!=provider)return false;
    it->second.ack_disposed=true;
    return true;
  }
  bool release(const std::string& session) {
    auto it=sessions_.find(session);
    if(it==sessions_.end() || !it->second.status_disposed ||
       !it->second.ack_disposed)return false;
    // Never discard an otherwise healthy Pending session just because both
    // DDS instances disappeared: that would silently lose a device command.
    // Faulted sessions can be explicitly quarantined and released.
    const Entry& e=it->second;
    if(!e.faulted && (!e.acked || !e.status || !terminal(*e.status)))
      return false;
    sessions_.erase(it);return true;
  }
  void ambiguous_delivery(const std::string& session) {
    auto it=sessions_.find(session);
    if(it==sessions_.end())throw std::out_of_range("unknown command session");
    it->second.faulted=true;
  }
  bool needs_recovery(const std::string& session) const {
    auto it=sessions_.find(session);
    if(it==sessions_.end())throw std::out_of_range("unknown command session");
    return it->second.faulted;
  }
  std::size_t size() const noexcept{return sessions_.size();}
private:
  static bool terminal(State s) {
    return s==State::COMPLETED || s==State::FAILED || s==State::CANCELED;
  }
  static bool valid(const std::optional<State>& prev,State next) {
    if(!prev)return next==State::ISSUED;
    if(terminal(*prev))return false;
    if(next==State::FAILED || next==State::CANCELED)return true;
    if(*prev==State::ISSUED)return next==State::COMMANDED;
    if(*prev==State::COMMANDED)return next==State::EXECUTING;
    return *prev==State::EXECUTING && next==State::COMPLETED;
  }
  struct Entry {
    Identity id;
    bool acked{false},faulted{false},awaiting_update{false};
    bool status_disposed{false},ack_disposed{false};
    std::optional<State> status;
    std::int64_t last_status_time{INT64_MIN};
  };
  std::size_t limit_;
  std::unordered_map<std::string,Entry> sessions_;
};
} // namespace umaa
