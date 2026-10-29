// Host-side loader for typed-user-component ABI v2. Legacy V1 remains intact.
#pragma once
#include "umaa/module_api_v2.h"
#include "umaa/internal/native_library.hpp"
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <unordered_set>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace umaa {
class ComponentModule final {
public:
  explicit ComponentModule(const std::string& filename)
      : library_(std::make_unique<internal::NativeLibrary>(filename)) {
    auto symbol = reinterpret_cast<UmaaGetComponentV2>(library_->symbol(UMAA_COMPONENT_ENTRY_NAME));
    if (!symbol) throw std::runtime_error("missing component ABI v2 entry");
    api_ = symbol(UMAA_COMPONENT_ABI_VERSION);
    if (!api_ || api_->abi_version != UMAA_COMPONENT_ABI_VERSION ||
        api_->struct_size != sizeof(UmaaComponentModuleV2) || !api_->module_id ||
        !api_->create || !api_->service_count || !api_->service_at ||
        !api_->start || !api_->call || !api_->stop || !api_->destroy)
      throw std::runtime_error("component ABI mismatch or incomplete table");
    if (!*api_->module_id) throw std::runtime_error("empty component module id");
    instance_ = api_->create();
    if (!instance_) throw std::runtime_error("component create failed");
    try {
      const uint32_t count = api_->service_count(instance_);
      if (count == 0 || count > 1024) throw std::runtime_error("invalid component service count");
      for (uint32_t i = 0; i < count; ++i) {
        UmaaComponentServiceV2 descriptor{};
        if (api_->service_at(instance_, i, &descriptor) != UMAA_COMPONENT_OK ||
            !descriptor.service_id || !*descriptor.service_id ||
            !descriptor.schema_id || !*descriptor.schema_id ||
            descriptor.kinds == 0 || (descriptor.kinds & ~7u))
          throw std::runtime_error("invalid component service descriptor");
        for (const auto& known : services_) {
          if (known.service == descriptor.service_id)
            throw std::runtime_error("duplicate component service");
        }
        services_.push_back({descriptor.service_id, descriptor.schema_id, descriptor.kinds});
      }
      if (api_->start(instance_) != UMAA_COMPONENT_OK)
        throw std::runtime_error("component start failed");
      started_ = true;
    } catch (...) {
      // A failed start may have launched plugin threads. Keep the mapping
      // unless the plugin explicitly confirms drained stop().
      if (api_->stop(instance_) == UMAA_COMPONENT_OK) api_->destroy(instance_);
      else library_->release_mapping();
      instance_ = nullptr;
      throw;
    }
  }
  ~ComponentModule() { shutdown(); }
  ComponentModule(const ComponentModule&) = delete;
  ComponentModule& operator=(const ComponentModule&) = delete;
  struct Service { std::string service, schema; uint32_t actions; };
  const std::vector<Service>& services() const { return services_; }
  std::string name() const { return api_->module_id; }
  int32_t call(const std::string& service, uint32_t action,
               const std::vector<uint8_t>& payload, std::vector<uint8_t>& output,
               int32_t& business_result, uint64_t session = 0) {
    if (payload.size() > UINT32_MAX) throw std::runtime_error("oversized component payload");
    bool authorized = false;
    for (const auto& s : services_)
      if (s.service == service &&
          ((action >= 1 && action <= 3 && (s.actions & (1u << (action - 1)))) ||
           (action == UMAA_COMPONENT_COMMAND_POLL && (s.actions & 1u))))
        authorized = true;
    if (!authorized) return UMAA_COMPONENT_NOT_FOUND;
    // Admit a call before stop can drain/unload it. Hardware operations
    // must never be replayed after an ambiguous delivery outcome.
    {
      std::lock_guard<std::mutex> lock(gate_);
      if (!started_ || closing_ || failed_stop_ || !instance_)
        throw std::runtime_error("component stopping or not active");
      if (action == UMAA_COMPONENT_COMMAND && session != 0) {
        PendingSession id{service,session};
        if (pending_.count(id))
          throw std::runtime_error("duplicate device session; command not replayed");
        // Reserve before invoking hardware; even an exception is ambiguous.
        pending_.insert(std::move(id));
      }
      if (action == UMAA_COMPONENT_COMMAND && session == 0)
        untracked_pending_ = true;
      ++in_flight_;
    }
    auto leave = [this]() noexcept {
      std::lock_guard<std::mutex> lock(gate_);
      --in_flight_;
      drained_.notify_all();
    };
    try {
      UmaaComponentCallV2 request{service.c_str(), action, 0,
                                 payload.empty() ? nullptr : payload.data(),
                                 static_cast<uint32_t>(payload.size()), session};
      if (output.size() < 1024 * 1024) output.resize(1024 * 1024);
      UmaaComponentResponseV2 response{output.data(),
                                      static_cast<uint32_t>(output.size()), 0, 0};
      const int32_t status = api_->call(instance_, &request, &response);
      if (status == UMAA_COMPONENT_BUFFER_TOO_SMALL)
        throw std::runtime_error("component output exceeds bounded single-call buffer");
      if (status == UMAA_COMPONENT_OK) {
        if (response.size > output.size()) throw std::runtime_error("component buffer overrun");
        output.resize(response.size);
        business_result = response.business_result;
        if (action == UMAA_COMPONENT_COMMAND && session == 0 && business_result != 2) {
          std::lock_guard<std::mutex> lock(gate_);
          untracked_pending_ = false;
        }
        if (session != 0 && (action == UMAA_COMPONENT_COMMAND ||
            action == UMAA_COMPONENT_COMMAND_POLL || action == UMAA_COMPONENT_CANCEL)) {
          std::lock_guard<std::mutex> lock(gate_);
          const PendingSession id{service,session};
          if (action == UMAA_COMPONENT_COMMAND && business_result != 2)
            pending_.erase(id);
          else if (action == UMAA_COMPONENT_COMMAND_POLL && business_result != 2)
            pending_.erase(id);
          else if (action == UMAA_COMPONENT_CANCEL &&
                   (business_result == 0 || business_result == 3))
            pending_.erase(id);
        }
      }
      leave();
      return status;
    } catch (...) {
      leave();
      throw;
    }
  }
  // Cooperative stop. A pending device command cannot be silently lost.
  // Concurrent callers serialize; no module callback may run after unload.
  bool stop() noexcept {
    std::unique_lock<std::mutex> lock(gate_);
    drained_.wait(lock, [this] { return !stopping_; });
    if (!instance_) return true;
    if (failed_stop_) return false;
    stopping_ = true;
    closing_ = true;
    drained_.wait(lock, [this] { return in_flight_ == 0; });
    if (untracked_pending_ || !pending_.empty()) {
      // A previously admitted call became Pending while stop was draining.
      closing_ = false;
      stopping_ = false;
      lock.unlock();
      drained_.notify_all();
      return false;
    }
    lock.unlock();
    int32_t result = UMAA_COMPONENT_EXCEPTION;
    try { result = started_ ? api_->stop(instance_) : UMAA_COMPONENT_OK; }
    catch (...) { result = UMAA_COMPONENT_EXCEPTION; }
    if (result == UMAA_COMPONENT_OK) {
      try { api_->destroy(instance_); }
      catch (...) { result = UMAA_COMPONENT_EXCEPTION; }
    }
    lock.lock();
    if (result == UMAA_COMPONENT_OK) {
      instance_ = nullptr;
      started_ = false;
    } else {
      failed_stop_ = true;
    }
    // Failed stop is irreversible. Never re-enter unknown device state.
    stopping_ = false;
    lock.unlock();
    drained_.notify_all();
    return result == UMAA_COMPONENT_OK;
  }
private:
  void shutdown() noexcept {
    if (!stop()) {
      std::fputs("UMAA component stop failed: retaining library mapping\n", stderr);
      library_->release_mapping();
      instance_ = nullptr;
    }
  }
  std::unique_ptr<internal::NativeLibrary> library_;
  const UmaaComponentModuleV2* api_{nullptr};
  void* instance_{nullptr};
  bool started_{false};
  struct PendingSession {
    std::string service;
    uint64_t id;
    bool operator==(const PendingSession& other) const noexcept {
      return id == other.id && service == other.service;
    }
  };
  struct PendingHash {
    std::size_t operator()(const PendingSession& s) const {
      return std::hash<std::string>{}(s.service) ^
             (std::hash<uint64_t>{}(s.id) << 1);
    }
  };
  std::mutex gate_;
  std::condition_variable drained_;
  std::unordered_set<PendingSession,PendingHash> pending_;
  std::size_t in_flight_{0};
  bool closing_{false};
  bool stopping_{false};
  bool failed_stop_{false};
  bool untracked_pending_{false};
  std::vector<Service> services_;
};
} // namespace umaa
