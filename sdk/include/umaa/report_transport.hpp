// Fail-closed transport binding for UMAA queryless Report publish intents.
// Not an official QoS policy or service-specific DDS TypeSupport.
//
// The sink owns DDS entities and is responsible for providing externally
// reviewed QoS and checking each vendor operation. If an operation fails,
// the bound provider faults and will never issue another operation; a fresh
// writer/instance lifecycle must be established by its owning host.
#pragma once
#include "umaa/report_engine.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace umaa {

template<class T> struct ReportTransport {
  virtual ~ReportTransport() = default;
  virtual void register_source(const std::string& source) = 0;
  virtual void publish(const ReportSample<T>& report) = 0;
  virtual void dispose(const std::string& source, std::int64_t timestamp) = 0;
  virtual void unregister_source(const std::string& source) = 0;
};

template<class T> class BoundReportProvider {
public:
  enum class State { Idle, Active, Stopped, Faulted };
  BoundReportProvider(std::string source, const ReportHandler<T>& handler,
                      ReportTransport<T>& transport)
      : lifecycle_(std::move(source), handler), transport_(transport) {}

  void start(std::int64_t timestamp) {
    require(State::Idle);
    // Handler exceptions happen before any transport side effect. Leave Idle.
    auto intents = lifecycle_.start(timestamp);
    deliver(intents);
    state_ = State::Active;
  }

  void start_unrecoverable(std::int64_t timestamp) {
    require(State::Idle);
    auto intents = lifecycle_.start_unrecoverable(timestamp);
    deliver(intents);
    state_ = State::Active;
  }

  void update(std::int64_t timestamp) {
    require(State::Active);
    // Handler exceptions may leave the underlying DDS writer intact.
    auto intent = lifecycle_.update(timestamp);
    deliver_one(intent);
  }

  void shutdown(std::int64_t timestamp) {
    if (state_ == State::Stopped) return;
    require(State::Active);
    auto intents = lifecycle_.shutdown(timestamp);
    deliver(intents);
    state_ = State::Stopped;
  }

  State state() const { return state_; }
  bool faulted() const { return state_ == State::Faulted; }

private:
  using Intent = ReportIntent<T>;
  void require(State s) const {
    if (state_ != s)
      throw std::logic_error("Report transport not in required state; restart DDS entity on fault");
  }
  void deliver(const std::vector<Intent>& intents) {
    for (const auto& intent : intents) deliver_one(intent);
  }
  void deliver_one(const Intent& intent) {
    try {
      switch (intent.kind) {
        case Intent::Kind::RegisterPublisher:
          transport_.register_source(intent.source); break;
        case Intent::Kind::Publish:
          if (!intent.report) throw std::logic_error("missing Report payload");
          transport_.publish(*intent.report); break;
        case Intent::Kind::Dispose:
          if (!intent.timestamp) throw std::logic_error("missing dispose timestamp");
          transport_.dispose(intent.source, *intent.timestamp); break;
        case Intent::Kind::UnregisterPublisher:
          transport_.unregister_source(intent.source); break;
        default:
          throw std::logic_error("consumer Report intent on publisher transport");
      }
    } catch (...) {
      // Unknown vendor delivery outcome: do not retry and risk stale,
      // duplicate or out-of-order on-wire messages.
      state_ = State::Faulted;
      throw;
    }
  }

  ReportProviderLifecycle<T> lifecycle_;
  ReportTransport<T>& transport_;
  State state_{State::Idle};
};
// Query-less Request/Reply binding: subscribe to a DDS Report, not an RPC.
// On any subscription lifecycle failure, reject callbacks until the caller
// destroys this reader binding. Reader callback epochs prevent restart races.
struct ReportSubscriptionTransport {
  virtual ~ReportSubscriptionTransport() = default;
  virtual void subscribe(std::uint64_t reader_epoch) = 0;
  virtual void unsubscribe(std::uint64_t reader_epoch) = 0;
};

template<class T> class BoundReportConsumer {
public:
  enum class State { Idle, Active, Stopped, Faulted };
  explicit BoundReportConsumer(ReportSubscriptionTransport& transport)
      : transport_(transport) {}

  std::uint64_t start() {
    if (state_ != State::Idle && state_ != State::Stopped)
      throw std::logic_error("Report reader must be Idle or Stopped");
    auto intents = lifecycle_.start();
    if (intents.size() != 1 ||
        intents[0].kind != ReportIntent<T>::Kind::Subscribe)
      throw std::logic_error("Report lifecycle emitted invalid subscribe");
    auto epoch = lifecycle_.epoch();
    try { transport_.subscribe(epoch); }
    catch (...) { state_ = State::Faulted; throw; }
    state_ = State::Active;
    return epoch;
  }
  bool receive(const ReportSample<T>& sample, std::uint64_t epoch) {
    return state_ == State::Active && lifecycle_.receive(sample, epoch);
  }
  bool receive_dispose(const std::string& source, std::int64_t timestamp,
                       std::uint64_t epoch) {
    return state_ == State::Active &&
           lifecycle_.receive_dispose(source, timestamp, epoch);
  }
  void shutdown() {
    if (state_ == State::Stopped) return;
    if (state_ != State::Active) throw std::logic_error("Report reader not Active");
    auto epoch = lifecycle_.epoch();
    auto intents = lifecycle_.shutdown();
    if (intents.size() != 1 ||
        intents[0].kind != ReportIntent<T>::Kind::Unsubscribe)
      throw std::logic_error("Report lifecycle emitted invalid unsubscribe");
    try { transport_.unsubscribe(epoch); }
    catch (...) { state_ = State::Faulted; throw; }
    state_ = State::Stopped;
  }
  State state() const { return state_; }
  std::uint64_t epoch() const { return lifecycle_.epoch(); }
  const ReportCache<T>& cache() const { return lifecycle_.cache(); }
private:
  ReportConsumerLifecycle<T> lifecycle_;
  ReportSubscriptionTransport& transport_;
  State state_{State::Idle};
};

} // namespace umaa
