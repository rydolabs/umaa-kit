// Host-only runtime orchestration. Application code uses Component::provide.
// Typed service-to-wire event translation is a separate, reviewed task.
#pragma once
#include <cstdint>
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
class UMAA_RUNTIME_API ComponentRuntime final {
public:
  ComponentRuntime();
  ~ComponentRuntime();
  ComponentRuntime(const ComponentRuntime&) = delete;
  ComponentRuntime& operator=(const ComponentRuntime&) = delete;
  void load_component(const std::string& filename);
  void load_backend(const std::string& filename, uint32_t domain_id);
  std::vector<std::string> services() const;
  int32_t call(const std::string& service_id, uint32_t action,
               const std::vector<uint8_t>& payload,
               std::vector<uint8_t>& output, int32_t& business_result,
               uint64_t session_id = 0);
  // Development-only vertical boundary exercise, not a complete UMAA
  // Report lifecycle. A generated ServiceBinding will own reviewed topic/QoS.
  void publish_report_fixture(const std::string& service_id,
                              const std::string& topic_name,
                              const std::string& schema_id,
                              const std::string& instance_key);
private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace umaa::internal
