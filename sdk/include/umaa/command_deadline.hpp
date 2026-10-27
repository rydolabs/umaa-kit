// Host-owned monotonic deadline policy for a prototype UMAA Command.
// The owner supplies a *reviewed* per-service duration; this class does not
// derive deadlines/QoS from normative ICDs. No hidden threads or wall clocks.
#pragma once
#include "umaa/command_transport.hpp"
#include <chrono>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace umaa {
template<class Payload, class Clock = std::chrono::steady_clock>
class TimedCommandProvider final {
 public:
  using TimePoint = typename Clock::time_point;
  using Duration = typename Clock::duration;
  TimedCommandProvider(std::string provider, Handler<Payload>& handler,
                       CommandTransport<Payload>& transport, Duration deadline)
      : bound_(std::move(provider), handler, transport), deadline_(deadline) {
    if (deadline_ <= Duration::zero())
      throw std::invalid_argument("Command deadline must be positive");
  }
  TimedCommandProvider(const TimedCommandProvider&) = delete;
  TimedCommandProvider& operator=(const TimedCommandProvider&) = delete;

  std::vector<Event> receive(const Identity& id, const Payload& payload,
                             TimePoint now = Clock::now()) {
    auto events = bound_.receive(id, payload);
    // Do not arm a rejected/replayed command or an already terminal one.
    // Accepted updates replace their previous *local* monotonic deadline.
    if (last_status(events) == "EXECUTING")
      deadlines_.insert_or_assign(id.session, now + deadline_);
    else if (is_terminal(events)) deadlines_.erase(id.session);
    return events;
  }
  std::vector<Event> complete(const std::string& session, bool success,
                              const std::string& reason = "OBJECTIVE_FAILED") {
    auto events = bound_.complete(session, success, reason);
    if (is_terminal(events)) deadlines_.erase(session);
    return events;
  }
  std::vector<Event> command_disposed(const std::string& source,
                                      const std::string& session) {
    auto events = bound_.command_disposed(source, session);
    for (const auto& event : events)
      if (event.kind == Event::Kind::DisposeStatus) {
        deadlines_.erase(session); break;
      }
    return events;
  }
  // Caller drives this from the DDS event loop. Cancellation refusal retains
  // the deadline for a later attempt; it never fabricates terminal status.
  std::vector<Event> expire(TimePoint now = Clock::now()) {
    std::vector<Event> result;
    for (auto it = deadlines_.begin(); it != deadlines_.end();) {
      if (it->second > now) { ++it; continue; }
      auto events = bound_.timeout(it->first); // failures fault transport
      bool inactive = false;
      for (const auto& event : events)
        if (event.kind == Event::Kind::Ignored && event.detail == "NOT_ACTIVE")
          inactive = true;
      result.insert(result.end(), events.begin(), events.end());
      if (is_terminal(events) || inactive) it = deadlines_.erase(it);
      else ++it;
    }
    return result;
  }
  std::vector<Event> fail_unrecoverable() {
    auto events = bound_.fail_unrecoverable();
    deadlines_.clear();
    return events;
  }
  bool faulted() const { return bound_.faulted(); }
  std::size_t sessions() const { return bound_.sessions(); }
  std::size_t armed_deadlines() const { return deadlines_.size(); }
 private:
  static std::string last_status(const std::vector<Event>& events) {
    std::string value;
    for (const auto& e : events)
      if (e.kind == Event::Kind::Status) value = e.detail;
    return value;
  }
  static bool is_terminal(const std::vector<Event>& events) {
    const auto value = last_status(events);
    return value == "COMPLETED" || value == "FAILED" || value == "CANCELED";
  }
  BoundCommandProvider<Payload> bound_;
  Duration deadline_;
  std::map<std::string, TimePoint> deadlines_;
};
} // namespace umaa
