// Application API: typed Handler + provide<Service>() with zero DDS calls.
// A service contract is generated from a reviewed schema and supplies:
//   static constexpr const char* id, schema_id;
//   using Command, Report;
//   static bool decode_command(const uint8_t*, size_t, Command&);
//   static std::vector<uint8_t> encode_report(const Report&);
// Those generated codecs are vendor-neutral application representations;
// vendor DDS TypeSupport mapping belongs exclusively in the adapter.
#pragma once
#include "umaa/module_api_v2.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace umaa {
enum class Result : int32_t { Completed=0, Rejected=1, Pending=2, Failed=3 };
template<class Service> struct CommandHandler {
  virtual ~CommandHandler() = default;
  virtual Result execute(const typename Service::Command& command) = 0;
  virtual Result cancel() { return Result::Rejected; }
  virtual Result execute_session(const typename Service::Command& c,uint64_t) {return execute(c);}
  virtual Result cancel_session(uint64_t) {return cancel();}
  // Device reports pending completion, default NEVER creates success.
  virtual Result poll_session(uint64_t) {return Result::Pending;}
};
template<class Service> struct ReportSource {
  virtual ~ReportSource() = default;
  virtual typename Service::Report read() = 0;
};
class Component final {
public:
  struct Registration {
    std::string service, schema;
    uint32_t kinds;
    std::function<int32_t(const UmaaComponentCallV2&, UmaaComponentResponseV2&)> invoke;
  };
  template<class Service>
  void provide(CommandHandler<Service>& h) {
    add({Service::id, Service::schema_id, 3u,
      [&h](const UmaaComponentCallV2& call, UmaaComponentResponseV2& out) -> int32_t {
        try {
          Result result;
          if (call.action == UMAA_COMPONENT_COMMAND) {
            typename Service::Command c{};
            if (!Service::decode_command(call.payload, call.payload_size, c))
              return UMAA_COMPONENT_INVALID;
            result = h.execute_session(c,call.session_id);
          } else if (call.action == UMAA_COMPONENT_CANCEL) {
            result = h.cancel_session(call.session_id);
          } else if(call.action==UMAA_COMPONENT_COMMAND_POLL && call.session_id!=0) {
            result=h.poll_session(call.session_id);
          } else return UMAA_COMPONENT_INVALID;
          out.size = 0;
          out.business_result = static_cast<int32_t>(result);
          return UMAA_COMPONENT_OK;
        } catch (...) { return UMAA_COMPONENT_EXCEPTION; }
      }});
  }
  template<class Service>
  void provide(ReportSource<Service>& h) {
    add({Service::id, Service::schema_id, 4u,
      [&h](const UmaaComponentCallV2& call, UmaaComponentResponseV2& out) -> int32_t {
        if (call.action != UMAA_COMPONENT_REPORT) return UMAA_COMPONENT_INVALID;
        try {
          auto bytes = Service::encode_report(h.read());
          if (bytes.size() > UINT32_MAX) return UMAA_COMPONENT_INVALID;
          out.size = static_cast<uint32_t>(bytes.size());
          if (out.capacity < out.size || (out.size && !out.payload))
            return UMAA_COMPONENT_BUFFER_TOO_SMALL;
          if (out.size) std::memcpy(out.payload, bytes.data(), out.size);
          out.business_result = static_cast<int32_t>(Result::Completed);
          return UMAA_COMPONENT_OK;
        } catch (...) { return UMAA_COMPONENT_EXCEPTION; }
      }});
  }
  uint32_t service_count() const { return static_cast<uint32_t>(registered_.size()); }
  int32_t service_at(uint32_t index, UmaaComponentServiceV2* out) const {
    if (!out || index >= registered_.size()) return UMAA_COMPONENT_INVALID;
    const auto& r = registered_[index];
    *out = {r.service.c_str(), r.schema.c_str(), r.kinds, 0u};
    return UMAA_COMPONENT_OK;
  }
  int32_t dispatch(const UmaaComponentCallV2* call, UmaaComponentResponseV2* out) {
    if (!active_ || !call || !out || !call->service_id ||
        (call->payload_size && !call->payload)) return UMAA_COMPONENT_NOT_ACTIVE;
    for (auto& r : registered_) {
      if (r.service == call->service_id && ((call->action >= 1 && call->action <= 3 &&
           (r.kinds & (1u << (call->action - 1)))) ||
           (call->action==UMAA_COMPONENT_COMMAND_POLL && (r.kinds & 1u))))
        return r.invoke(*call, *out);
    }
    return UMAA_COMPONENT_NOT_FOUND;
  }
  void start() { active_ = true; }
  void stop() { active_ = false; }
private:
  void add(Registration r) {
    if (r.service.empty() || r.schema.empty())
      throw std::invalid_argument("UMAA service needs name and schema");
    for (const auto& s : registered_)
      if (s.service == r.service)
        throw std::invalid_argument("duplicate UMAA service in component");
    registered_.push_back(std::move(r));
  }
  bool active_{false};
  std::vector<Registration> registered_;
};
// Only macro in user code. Generates all C ABI plumbing inside the module.
// User implementation simply calls component.provide<Service>(handler).
#define UMAA_EXPORT_COMPONENT_V2(MODULE_ID, REGISTER_FN)                            \
namespace {                                                                        \
struct UmaaGeneratedComponentInstance {                                            \
  umaa::Component component;                                                        \
  std::mutex mutex;                                                                 \
};                                                                                  \
void* umaa_generated_create() noexcept {                                           \
  try { auto* p = new UmaaGeneratedComponentInstance(); REGISTER_FN(p->component); \
        return p; } catch (...) { return nullptr; }                                 \
}                                                                                   \
uint32_t umaa_generated_count(void* ctx) noexcept {                                \
  return ctx ? static_cast<UmaaGeneratedComponentInstance*>(ctx)->component.service_count() : 0u; \
}                                                                                   \
int32_t umaa_generated_at(void* ctx, uint32_t index, UmaaComponentServiceV2* out) noexcept { \
  return ctx ? static_cast<UmaaGeneratedComponentInstance*>(ctx)->component.service_at(index, out) : UMAA_COMPONENT_INVALID; \
}                                                                                   \
int32_t umaa_generated_start(void* ctx) noexcept {                                 \
  if (!ctx) return UMAA_COMPONENT_INVALID;                                          \
  auto& p = *static_cast<UmaaGeneratedComponentInstance*>(ctx);                     \
  std::lock_guard<std::mutex> lock(p.mutex); p.component.start(); return 0;         \
}                                                                                   \
int32_t umaa_generated_call(void* ctx, const UmaaComponentCallV2* call, UmaaComponentResponseV2* out) noexcept { \
  if (!ctx) return UMAA_COMPONENT_INVALID;                                          \
  auto& p = *static_cast<UmaaGeneratedComponentInstance*>(ctx);                     \
  std::lock_guard<std::mutex> lock(p.mutex);                                        \
  try { return p.component.dispatch(call, out); } catch (...) { return UMAA_COMPONENT_EXCEPTION; } \
}                                                                                   \
int32_t umaa_generated_stop(void* ctx) noexcept {                                  \
  if (!ctx) return UMAA_COMPONENT_INVALID;                                          \
  auto& p = *static_cast<UmaaGeneratedComponentInstance*>(ctx);                     \
  std::lock_guard<std::mutex> lock(p.mutex); p.component.stop(); return 0;          \
}                                                                                   \
void umaa_generated_destroy(void* ctx) noexcept {                                  \
  delete static_cast<UmaaGeneratedComponentInstance*>(ctx);                        \
}                                                                                   \
const UmaaComponentModuleV2 umaa_generated_api = {                                 \
  UMAA_COMPONENT_ABI_VERSION, sizeof(UmaaComponentModuleV2), MODULE_ID,             \
  &umaa_generated_create, &umaa_generated_count, &umaa_generated_at,                \
  &umaa_generated_start, &umaa_generated_call, &umaa_generated_stop,                \
  &umaa_generated_destroy};                                                         \
}                                                                                   \
extern "C" UMAA_COMPONENT_EXPORT const UmaaComponentModuleV2* umaa_get_component_v2(uint32_t version) { \
  return version == UMAA_COMPONENT_ABI_VERSION ? &umaa_generated_api : nullptr;     \
}
} // namespace umaa
