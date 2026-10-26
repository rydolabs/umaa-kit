// Transport-independent prototype of UMAA EO ICD 6.0 section 5.2.1.
// DDS adapters must carry out and verify the intents below; these are not
// on-wire calls, DDS TypeSupport, or a query-data RPC implementation.
#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace umaa {
template <class T> struct ReportSample {
  std::string source;
  std::int64_t timestamp;
  T data;
};
template <class T> struct ReportHandler {
  virtual ~ReportHandler() = default;
  virtual T read_report() const = 0;
};
template <class T> class ReportProvider {
public:
  ReportProvider(std::string source, const ReportHandler<T>& handler)
      : source_(std::move(source)), handler_(handler) {}
  ReportSample<T> publish(std::int64_t timestamp) const {
    return {source_, timestamp, handler_.read_report()};
  }
  const std::string& source() const { return source_; }
private:
  std::string source_;
  const ReportHandler<T>& handler_;
};
template <class T> class ReportCache {
public:
  // Timestamp watermarks remain after disposal: replay cannot resurrect
  // previously-disposed source data. DDS instance/source identity is explicit.
  bool receive(const ReportSample<T>& report) {
    if (report.source.empty()) return false;
    auto it = watermarks_.find(report.source);
    if (it != watermarks_.end() && it->second >= report.timestamp) return false;
    watermarks_[report.source] = report.timestamp;
    latest_[report.source] = report;
    return true;
  }
  bool dispose(const std::string& source, std::int64_t timestamp) {
    if (source.empty()) return false;
    auto it = watermarks_.find(source);
    if (it != watermarks_.end() && it->second >= timestamp) return false;
    watermarks_[source] = timestamp;
    latest_.erase(source);
    return true;
  }
  // Compatibility path when a DDS adapter lacks a valid dispose timestamp.
  bool dispose(const std::string& source) { return latest_.erase(source) != 0; }
  std::optional<ReportSample<T>> latest(const std::string& source) const {
    auto it = latest_.find(source);
    if (it == latest_.end()) return std::nullopt;
    return it->second;
  }
  std::size_t size() const { return latest_.size(); }
  void clear() { latest_.clear(); watermarks_.clear(); }
private:
  std::unordered_map<std::string, ReportSample<T>> latest_;
  std::unordered_map<std::string, std::int64_t> watermarks_;
};

// Intents require execution and checked error handling in DDS vendor adapter.
template <class T> struct ReportIntent {
  enum class Kind { RegisterPublisher, Publish, Dispose, UnregisterPublisher,
                    Subscribe, Unsubscribe } kind;
  std::string source;
  std::optional<ReportSample<T>> report;
  std::optional<std::int64_t> timestamp;
};

template <class T> class ReportProviderLifecycle {
public:
  using Kind = typename ReportIntent<T>::Kind;
  ReportProviderLifecycle(std::string source, const ReportHandler<T>& handler)
      : provider_(std::move(source), handler) {
    if (provider_.source().empty()) throw std::invalid_argument("report source required");
  }
  std::vector<ReportIntent<T>> start(std::int64_t timestamp) {
    ensure_not_running();
    // Read handler before changing state; exceptions cannot commit start.
    auto sample = provider_.publish(timestamp);
    active_ = true; live_ = true; last_timestamp_ = timestamp;
    return {{Kind::RegisterPublisher, provider_.source(), {}, {}},
            {Kind::Publish, provider_.source(), sample, timestamp}};
  }
  // On unrecoverable persisted data, remove the stale DDS instance.
  std::vector<ReportIntent<T>> start_unrecoverable(std::int64_t timestamp) {
    ensure_not_running();
    active_ = true; live_ = false; last_timestamp_ = timestamp;
    return {{Kind::RegisterPublisher, provider_.source(), {}, {}},
            {Kind::Dispose, provider_.source(), {}, timestamp}};
  }
  ReportIntent<T> update(std::int64_t timestamp) {
    if (!active_) throw std::logic_error("report provider not running");
    if (last_timestamp_ && timestamp <= *last_timestamp_)
      throw std::invalid_argument("report timestamp must increase");
    auto sample = provider_.publish(timestamp);
    live_ = true; last_timestamp_ = timestamp;
    return {Kind::Publish, provider_.source(), sample, timestamp};
  }
  std::vector<ReportIntent<T>> shutdown(std::int64_t timestamp) {
    if (!active_) return {};
    if (last_timestamp_ && timestamp <= *last_timestamp_)
      throw std::invalid_argument("dispose timestamp must increase");
    std::vector<ReportIntent<T>> events;
    if (live_) events.push_back({Kind::Dispose, provider_.source(), {}, timestamp});
    events.push_back({Kind::UnregisterPublisher, provider_.source(), {}, {}});
    live_ = false; active_ = false;
    return events;
  }
  bool running() const { return active_; }
private:
  void ensure_not_running() const {
    if (active_) throw std::logic_error("report provider already running");
  }
  ReportProvider<T> provider_;
  bool active_{false};
  bool live_{false};
  std::optional<std::int64_t> last_timestamp_;
};

// Query-less UMAA Request/Reply: subscribe to a DDS Report, not RPC.
template <class T> class ReportConsumerLifecycle {
public:
  using Kind = typename ReportIntent<T>::Kind;
  std::vector<ReportIntent<T>> start() {
    if (subscribed_) throw std::logic_error("report consumer already subscribed");
    cache_.clear(); subscribed_ = true; ++epoch_;
    return {{Kind::Subscribe, "", {}, {}}};
  }
  bool receive(const ReportSample<T>& report, std::uint64_t reader_epoch) {
    return subscribed_ && reader_epoch == epoch_ && cache_.receive(report);
  }
  bool receive_dispose(const std::string& source, std::int64_t timestamp,
                       std::uint64_t reader_epoch) {
    return subscribed_ && reader_epoch == epoch_ && cache_.dispose(source, timestamp);
  }
  std::vector<ReportIntent<T>> shutdown() {
    if (!subscribed_) return {};
    subscribed_ = false; cache_.clear();
    return {{Kind::Unsubscribe, "", {}, {}}};
  }
  bool subscribed() const { return subscribed_; }
  std::uint64_t epoch() const { return epoch_; }
  const ReportCache<T>& cache() const { return cache_; }
private:
  bool subscribed_{false};
  std::uint64_t epoch_{0};
  ReportCache<T> cache_;
};
}  // namespace umaa
