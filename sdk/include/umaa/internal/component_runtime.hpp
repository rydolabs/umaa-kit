// Host-only runtime orchestration. Application code uses Component::provide.
// Typed service-to-wire event translation is a separate, reviewed task.
#pragma once
#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#if defined(_WIN32)
# if defined(UMAA_RUNTIME_BUILD)
#  define UMAA_RUNTIME_API __declspec(dllexport)
# else
#  define UMAA_RUNTIME_API __declspec(dllimport)
# endif
#else
# define UMAA_RUNTIME_API __attribute__((visibility("default")))
#endif
namespace umaa::internal {
class UMAA_RUNTIME_API ReportSubscription final {
public:
  ~ReportSubscription();
  ReportSubscription(const ReportSubscription&) = delete;
  ReportSubscription& operator=(const ReportSubscription&) = delete;
  int32_t poll(uint32_t max_samples=32);
  bool latest(std::vector<uint8_t>& data,std::string& key) const;
  bool disposed() const;
  // Snapshot of most recently delivered event, including its Dispose key.
  bool last_event(std::vector<uint8_t>& payload,std::string& key,uint32_t& kind,uint64_t& revision) const;
  bool latest_for(const std::string& key,std::vector<uint8_t>& out) const;
  std::vector<std::string> keys() const;
  bool disposed_for(const std::string& key) const;
private:
  struct State;
  explicit ReportSubscription(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend class ComponentRuntime;
};
class UMAA_RUNTIME_API ReportPublisher final {
public:
  ~ReportPublisher();
  ReportPublisher(const ReportPublisher&) = delete;
  ReportPublisher& operator=(const ReportPublisher&) = delete;
  void update();
  void dispose();
  void close();
  void start_periodic(std::chrono::milliseconds interval);
  void stop_periodic() noexcept;
  void rethrow_periodic_error();
  bool faulted() const noexcept;
private:
  struct State;
  explicit ReportPublisher(std::unique_ptr<State> state);
  std::unique_ptr<State> state_;
  friend class ComponentRuntime;
};
class UMAA_RUNTIME_API ComponentRuntime final {
public:
  ComponentRuntime();
  ~ComponentRuntime();
  ComponentRuntime(const ComponentRuntime&) = delete;
  ComponentRuntime& operator=(const ComponentRuntime&) = delete;
  void load_component(const std::string& filename);
  // Cooperative unload. Rejects Pending command sessions and active publishers.
  // Module administration must be serialized by the host orchestrator.
  bool unload_component(const std::string& module_id);
  void load_backend(const std::string& filename, uint32_t domain_id);
  std::vector<std::string> services() const;
  std::unique_ptr<ReportSubscription> subscribe_report_fixture(const std::string& service,
     const std::string& topic,const std::string& schema,const std::string& qos);
  int32_t call(const std::string& service_id, uint32_t action,
               const std::vector<uint8_t>& payload,
               std::vector<uint8_t>& output, int32_t& business_result,
               uint64_t session_id = 0);
  // Development-only vertical boundary exercise, not a complete UMAA
  // Report lifecycle. A generated ServiceBinding will own reviewed topic/QoS.
  std::unique_ptr<ReportPublisher> open_report_fixture(
    const std::string& service_id,const std::string& topic_name,
    const std::string& schema_id,const std::string& instance_key);
  void publish_report_fixture(const std::string& service_id,
                              const std::string& topic_name,
                              const std::string& schema_id,
                              const std::string& instance_key, bool dispose_after=false);
private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace umaa::internal
