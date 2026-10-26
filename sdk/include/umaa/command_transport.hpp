// Vendor-neutral, fail-closed Command/Response event transport.
//
// This binds the tested CommandEngine state machine to a concrete host-owned
// DDS CommandStatus/Ack writer. It does not create DDS entities, choose QoS,
// map official enum values, or claim conformance to the UMAA specification.
// A vendor implementation must use reviewed type/instance keys and check every
// writer/dispose operation. No retries are safe after an unknown delivery result.
#pragma once

#include "umaa/command_engine.hpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace umaa {

template<class Payload> struct CommandTransport {
  virtual ~CommandTransport() = default;
  virtual void publish_status(const Identity& command, const Event& status) = 0;
  virtual void publish_ack(const Identity& command, const Payload& payload) = 0;
  virtual void dispose_status(const Identity& command) = 0;
  virtual void dispose_ack(const Identity& command, const Payload& payload) = 0;
};

template<class Payload> class BoundCommandProvider {
public:
  enum class State { Active, Faulted };
  BoundCommandProvider(std::string provider, Handler<Payload>& handler,
                       CommandTransport<Payload>& transport)
      : engine_(std::move(provider), handler), transport_(transport) {}

  // All CommandEngine events are delivered, in order, before this returns.
  // Ignored events never generate on-wire writes. A delivery failure faults
  // this binding permanently; callers must rebuild the host-owned DDS entities.
  std::vector<Event> receive(const Identity& id, const Payload& payload) {
    require_active();
    std::vector<Event> events;
    try {
      events = engine_.receive(id, payload);
      // Only accepted commands can change the tracked initiating identity.
      if (accepted(events)) sessions_.insert_or_assign(id.session, SessionData{id, payload});
      deliver(events);
    } catch (...) {
      state_ = State::Faulted;
      throw;
    }
    return events;
  }

  std::vector<Event> complete(const std::string& session, bool success,
                              const std::string& reason = "OBJECTIVE_FAILED") {
    require_active();
    try {
      auto events = engine_.complete(session, success, reason);
      deliver(events);
      if (!engine_.contains(session)) sessions_.erase(session);
      return events;
    } catch (...) {
      state_ = State::Faulted;
      throw;
    }
  }

  std::vector<Event> command_disposed(const std::string& source,
                                      const std::string& session) {
    require_active();
    try {
      auto events = engine_.command_disposed(source, session);
      deliver(events);
      if (!engine_.contains(session)) sessions_.erase(session);
      return events;
    } catch (...) {
      state_ = State::Faulted;
      throw;
    }
  }

  // Deadline detection is a host responsibility; no hidden timer/QoS.
  std::vector<Event> timeout(const std::string& session) {
    require_active();
    try {
      auto events = engine_.timeout(session);
      deliver(events);
      if (!engine_.contains(session)) sessions_.erase(session);
      return events;
    } catch (...) {
      state_ = State::Faulted;
      throw;
    }
  }

  std::vector<Event> fail_unrecoverable() {
    require_active();
    try {
      auto events = engine_.fail_unrecoverable();
      deliver(events);
      return events;
    } catch (...) {
      state_ = State::Faulted;
      throw;
    }
  }

  State state() const { return state_; }
  bool faulted() const { return state_ == State::Faulted; }
  std::size_t sessions() const { return engine_.size(); }

private:
  static bool accepted(const std::vector<Event>& events) {
    for (const auto& event : events) {
      if (event.kind != Event::Kind::Ignored) return true;
    }
    return false;
  }
  void require_active() const {
    if (state_ != State::Active)
      throw std::logic_error("Command transport faulted; recreate DDS entities");
  }
  void deliver(const std::vector<Event>& events) {
    for (const auto& event : events) {
      if (event.kind == Event::Kind::Ignored) continue;
      const auto it = sessions_.find(event.session);
      if (it == sessions_.end())
        throw std::logic_error("Command event has no accepted identity");
      switch (event.kind) {
        case Event::Kind::Status:
          transport_.publish_status(it->second.identity, event); break;
        case Event::Kind::Ack:
          transport_.publish_ack(it->second.identity, it->second.payload); break;
        case Event::Kind::DisposeStatus:
          transport_.dispose_status(it->second.identity); break;
        case Event::Kind::DisposeAck:
          transport_.dispose_ack(it->second.identity, it->second.payload); break;
        case Event::Kind::Ignored:
          break;
      }
    }
  }
  struct SessionData { Identity identity; Payload payload; };
  CommandEngine<Payload> engine_;
  CommandTransport<Payload>& transport_;
  State state_{State::Active};
  std::unordered_map<std::string, SessionData> sessions_;
};

}  // namespace umaa
