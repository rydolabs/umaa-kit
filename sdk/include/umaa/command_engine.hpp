// Prototype protocol engine extracted as a clean-room minimal implementation from
// UMAA EO ICD v6.0 5.1 (NOT the official AUVSI SDK and NOT conformance certified).
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string_view>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace umaa {
struct Identity {
  std::string source;
  std::string destination;
  std::string session;
  std::int64_t timestamp{};
};
enum class State { ISSUED, COMMANDED, EXECUTING, COMPLETED, FAILED, CANCELED };
enum class Outcome { Pending, Completed, Failed };

// UMAA EO ICD 6.0 Figure 23: the transition reason is part of the
// application protocol, not an optional DDS transport decoration.
inline bool command_transition_allowed(std::optional<State> prior,State next,
                                       std::string_view reason) {
  if(!prior)return next==State::ISSUED && reason=="SUCCEEDED";
  const State before=*prior;
  if(before==State::COMPLETED||before==State::FAILED||before==State::CANCELED)return false;
  if(next==State::ISSUED)return reason=="UPDATED";
  if(next==State::CANCELED)return reason=="CANCELED";
  if(next==State::FAILED) {
    if(before==State::ISSUED)
      return reason=="VALIDATION_FAILED"||reason=="RESOURCE_FAILED"||
        reason=="INTERRUPTED"||reason=="TIMEOUT"||reason=="SERVICE_FAILED";
    if(before==State::COMMANDED)
      return reason=="RESOURCE_REJECTED"||reason=="INTERRUPTED"||
        reason=="TIMEOUT"||reason=="SERVICE_FAILED";
    return before==State::EXECUTING &&
      (reason=="OBJECTIVE_FAILED"||reason=="RESOURCE_FAILED"||
       reason=="INTERRUPTED"||reason=="TIMEOUT"||reason=="SERVICE_FAILED");
  }
  if(before==State::ISSUED)return next==State::COMMANDED&&reason=="SUCCEEDED";
  if(before==State::COMMANDED)return next==State::EXECUTING&&reason=="SUCCEEDED";
  return before==State::EXECUTING&&next==State::COMPLETED&&reason=="SUCCEEDED";
}
inline const char* to_string(State s) {
  switch (s) {
    case State::ISSUED: return "ISSUED";
    case State::COMMANDED: return "COMMANDED";
    case State::EXECUTING: return "EXECUTING";
    case State::COMPLETED: return "COMPLETED";
    case State::FAILED: return "FAILED";
    case State::CANCELED: return "CANCELED";
  }
  return "UNKNOWN";
}
struct Event {
  enum class Kind { Status, Ack, DisposeStatus, DisposeAck, Ignored } kind;
  std::string session;
  std::string detail;
  std::string reason;
};
template <class Payload> struct Handler {
  virtual ~Handler() = default;
  virtual bool validate(const Payload&) { return true; }
  virtual Outcome execute(const Payload&) = 0;
  // Return false if the device cannot safely cancel; terminal status isn't
  // synthesized until it truly cancels/completes.
  virtual bool cancel(const Payload&) = 0;
  // Optional identity-aware hooks. Existing handlers remain source-compatible.
  // Implement these when routing concurrent device sessions via Component ABI.
  virtual Outcome execute_session(const Identity&,const Payload& p) {return execute(p);}
  virtual bool cancel_session(const Identity&,const Payload& p) {return cancel(p);}
};

template <class Payload> class CommandEngine {
public:
  explicit CommandEngine(std::string provider, Handler<Payload>& handler)
      : provider_(std::move(provider)), handler_(handler) {}

  // The caller must translate actual DDS samples into these calls. The core
  // does not conceal the fact that DDS/QoS/recovery integration is missing.
  std::vector<Event> receive(const Identity& id, const Payload& payload) {
    std::vector<Event> events;
    if (id.destination != provider_ || id.source.empty() || id.session.empty()) {
      events.push_back({Event::Kind::Ignored, id.session, "INVALID_ROUTING", ""});
      return events;
    }
    if(lost_sources_.count(id.source)) {
      events.push_back({Event::Kind::Ignored,id.session,"CONSUMER_NOT_ALIVE",""});
      return events;
    }
    auto it = sessions_.find(id.session);
    if (it != sessions_.end()) {
      auto& current = it->second;
      if (id.source != current.identity.source) {
        events.push_back({Event::Kind::Ignored, id.session, "SOURCE_CONFLICT", ""});
        return events;
      }
      // A previously stranded hardware command cannot be updated when a
      // source returns after liveliness loss: reconcile it out of band first.
      if(current.consumer_lost) {
        events.push_back({Event::Kind::Ignored,id.session,"ORPHANED_COMMAND",""});
        return events;
      }
      // Once an initiating Command has been disposed, an arriving newer
      // sample must not reactivate work even when cancellation was refused.
      if (current.command_gone) {
        events.push_back({Event::Kind::Ignored, id.session, "COMMAND_DISPOSED", ""});
        return events;
      }
      if (id.timestamp <= current.identity.timestamp) {
        events.push_back({Event::Kind::Ignored, id.session, "STALE_OR_REPLAY", ""});
        return events;
      }
      if (terminal(current.state)) {
        events.push_back({Event::Kind::Ignored, id.session, "TERMINAL_UPDATE", ""});
        return events;
      }
      // First request device to cancel previous activity; never claim an
      // update succeeded while old hardware work is still active.
      if (!safe_cancel(current.identity,current.payload)) {
        events.push_back({Event::Kind::Ignored, id.session, "UPDATE_NOT_SAFE", ""});
        return events;
      }
      current.identity = id;
      current.payload = payload;
      transition(current, State::ISSUED, "UPDATED", events);
      execute(current, events);
      return events;
    }
    Session s{id, payload, State::ISSUED};
    auto result = sessions_.emplace(id.session, std::move(s));
    auto& inserted = result.first->second;
    events.push_back({Event::Kind::Status, id.session, "ISSUED", "SUCCEEDED"});
    execute(inserted, events);
    return events;
  }

  std::vector<Event> complete(const std::string& session, bool success,
                              const std::string& failure_reason = "OBJECTIVE_FAILED") {
    std::vector<Event> events;
    auto it = sessions_.find(session);
    if (it == sessions_.end() || it->second.state != State::EXECUTING)
      return {{Event::Kind::Ignored, session, "NOT_EXECUTING", ""}};
    transition(it->second, success ? State::COMPLETED : State::FAILED,
               success ? "SUCCEEDED" : failure_reason, events);
    if (it->second.command_gone || it->second.consumer_lost ||
        lost_sources_.count(it->second.identity.source)) {
      // No live Consumer remains to dispose the initiating Command. Retire
      // the responses once the *real* device outcome reaches a terminal state.
      cleanup(session, events);
      sessions_.erase(it);
    }
    return events;
  }

  // The host owns clocks and deadline policy; call this only after an
  // independently established deadline. Fail closed: never report timeout as
  // terminal while the device refuses or cannot confirm safe cancellation.
  std::vector<Event> timeout(const std::string& session) {
    std::vector<Event> events;
    auto it = sessions_.find(session);
    if (it == sessions_.end() || terminal(it->second.state))
      return {{Event::Kind::Ignored, session, "NOT_ACTIVE", ""}};
    if (!safe_cancel(it->second.identity,it->second.payload))
      return {{Event::Kind::Ignored, session, "TIMEOUT_CANCEL_NOT_SAFE", ""}};
    transition(it->second, State::FAILED, "TIMEOUT", events);
    if (it->second.command_gone || it->second.consumer_lost ||
        lost_sources_.count(it->second.identity.source)) {
      // No live Consumer remains to dispose the initiating Command. Retire
      // the responses once the *real* device outcome reaches a terminal state.
      cleanup(session, events);
      sessions_.erase(it);
    }
    return events;
  }

  // Terminal states must remain available until the initiating Command DDS
  // instance is disposed; only then may provider dispose ack/status instances.
  std::vector<Event> command_disposed(const std::string& source, const std::string& session) {
    std::vector<Event> events;
    auto it = sessions_.find(session);
    if (it == sessions_.end() || it->second.identity.source != source)
      return {{Event::Kind::Ignored, session, "UNKNOWN_SESSION", ""}};
    it->second.command_gone = true;
    if (!terminal(it->second.state)) {
      if (!safe_cancel(it->second.identity,it->second.payload)) {
        return {{Event::Kind::Ignored, session, "CANCEL_NOT_SAFE", ""}};
      }
      transition(it->second, State::CANCELED, "CANCELED", events);
    }
    cleanup(session, events);
    sessions_.erase(it);
    return events;
  }

  // EO ICD 6.0 Section 5.1.4.5: loss of consumer liveliness requires
  // cancellation of all active commands from that source, followed by
  // Status/Ack retirement. A device that cannot confirm safe cancellation
  // remains tracked; never fabricate a terminal hardware outcome.
  // Called only by a trusted DDS liveliness observer, not by a timer guess.
  std::vector<Event> consumer_liveliness_lost(const std::string& source) {
    if(source.empty())throw std::invalid_argument("missing liveliness source");
    // Fence even *new* incoming sessions until discovery confirms restoration.
    // A stale/replayed DDS sample must not restart hardware after the loss.
    lost_sources_.insert(source);
    std::vector<std::string> candidates;
    for(const auto& pair:sessions_)
      if(pair.second.identity.source==source && !terminal(pair.second.state))
        candidates.push_back(pair.first);
    std::sort(candidates.begin(),candidates.end());
    std::vector<Event> events;
    for(const auto& session:candidates) {
      auto it=sessions_.find(session);
      if(it==sessions_.end())continue;
      it->second.consumer_lost=true;
      if(!safe_cancel(it->second.identity,it->second.payload)) {
        events.push_back({Event::Kind::Ignored,session,"LIVELINESS_CANCEL_NOT_SAFE",""});
        continue;
      }
      transition(it->second,State::CANCELED,"CANCELED",events);
      cleanup(session,events);
      sessions_.erase(it);
    }
    return events;
  }

  // Only a trusted DDS participant discovery/liveliness event may release
  // this fence; an arriving Command sample is not restoration evidence.
  void consumer_liveliness_restored(const std::string& source) {
    if(source.empty())throw std::invalid_argument("missing liveliness source");
    lost_sources_.erase(source);
  }

  // Policy for persisted commands on provider startup: fail if cannot resume.
  // The bus adapter must enumerate persisted DDS samples and invoke this path.
  std::vector<Event> fail_unrecoverable() {
    std::vector<Event> events;
    for (auto& pair : sessions_) {
      auto& s = pair.second;
      if (!terminal(s.state)) transition(s, State::FAILED, "SERVICE_FAILED", events);
    }
    return events;
  }

  bool contains(const std::string& session) const { return sessions_.count(session) != 0; }
  std::size_t size() const { return sessions_.size(); }

private:
  struct Session {
    Identity identity;
    Payload payload;
    State state;
    bool command_gone{false};
    bool consumer_lost{false};
  };
  static void cleanup(const std::string& session, std::vector<Event>& events) {
    events.push_back({Event::Kind::DisposeStatus, session, "", ""});
    events.push_back({Event::Kind::DisposeAck, session, "", ""});
  }
  static bool terminal(State state) {
    return state == State::COMPLETED || state == State::FAILED || state == State::CANCELED;
  }
  static bool valid(State before,State after,const std::string& reason) {
    return command_transition_allowed(before,after,reason);
  }
  static void transition(Session& s, State state, const std::string& reason,
                         std::vector<Event>& events) {
    if (!valid(s.state, state, reason)) throw std::logic_error("invalid UMAA state transition");
    s.state = state;
    events.push_back({Event::Kind::Status, s.identity.session, to_string(state), reason});
  }
  bool safe_cancel(const Identity& id,const Payload& payload) {
    try { return handler_.cancel_session(id,payload); }
    catch (...) { return false; } // Never claim cancellation if user code throws.
  }
  void execute(Session& s, std::vector<Event>& events) {
    events.push_back({Event::Kind::Ack, s.identity.session, "", ""});
    bool valid_payload = false;
    try { valid_payload = handler_.validate(s.payload); }
    catch (...) {
      transition(s, State::FAILED, "SERVICE_FAILED", events);
      return;
    }
    if (!valid_payload) {
      transition(s, State::FAILED, "VALIDATION_FAILED", events);
      return;
    }
    transition(s, State::COMMANDED, "SUCCEEDED", events);
    transition(s, State::EXECUTING, "SUCCEEDED", events);
    Outcome out = Outcome::Pending;
    try { out = handler_.execute_session(s.identity,s.payload); }
    catch (...) { transition(s, State::FAILED, "SERVICE_FAILED", events); return; }
    if (out == Outcome::Completed) transition(s, State::COMPLETED, "SUCCEEDED", events);
    if (out == Outcome::Failed) transition(s, State::FAILED, "OBJECTIVE_FAILED", events);
  }
  std::string provider_;
  Handler<Payload>& handler_;
  std::unordered_map<std::string, Session> sessions_;
  std::unordered_set<std::string> lost_sources_;
};
}  // namespace umaa
