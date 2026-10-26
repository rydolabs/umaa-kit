// Prototype service host for native Windows DLL / POSIX shared objects.
#pragma once
#include "umaa/module_api.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace umaa {
class ModuleLoadError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class ServiceModule final {
public:
  ServiceModule(const std::string& filename, const std::string& expected_service,
                const std::string& expected_schema) {
#ifdef _WIN32
    lib_ = LoadLibraryA(filename.c_str());
    if (!lib_) throw ModuleLoadError("LoadLibrary failed: " + filename);
    auto symbol = reinterpret_cast<UmaaGetModuleV1>(
        GetProcAddress(lib_, UMAA_MODULE_ENTRY_NAME));
#else
    lib_ = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!lib_) throw ModuleLoadError(std::string("dlopen failed: ") + dlerror());
    auto symbol = reinterpret_cast<UmaaGetModuleV1>(dlsym(lib_, UMAA_MODULE_ENTRY_NAME));
#endif
    try {
      if (!symbol) throw ModuleLoadError("missing UMAA module entry point");
      api_ = symbol(UMAA_MODULE_ABI_VERSION);
      if (!api_ || api_->abi_version != UMAA_MODULE_ABI_VERSION ||
          api_->struct_size != sizeof(UmaaModuleV1))
        throw ModuleLoadError("module ABI version/size mismatch");
      if (!api_->service_name || !api_->schema_id || !api_->create ||
          !api_->start || !api_->invoke || !api_->stop || !api_->destroy)
        throw ModuleLoadError("incomplete module contract");
      if (expected_service != api_->service_name || expected_schema != api_->schema_id)
        throw ModuleLoadError("service or schema contract mismatch");
      instance_ = api_->create();
      if (!instance_) throw ModuleLoadError("module create failed");
      if (api_->start(instance_) != 0) throw ModuleLoadError("module start failed");
      started_ = true;
    } catch (...) {
      teardown();
      throw;
    }
  }
  ~ServiceModule() { teardown(); }
  ServiceModule(const ServiceModule&) = delete;
  ServiceModule& operator=(const ServiceModule&) = delete;
  ServiceModule(ServiceModule&&) = delete;
  ServiceModule& operator=(ServiceModule&&) = delete;

  int64_t invoke(int32_t operation, int64_t input) const {
    if (!started_) throw ModuleLoadError("service not started");
    int64_t result = 0;
    if (api_->invoke(instance_, operation, input, &result) != 0)
      throw ModuleLoadError("service invocation failed");
    return result;
  }
  const std::string name() const { return api_->service_name; }

private:
  void teardown() noexcept {
    if (instance_ && api_) {
      if (started_) {
        // Prototype has no session-drain capability. A failed stop must not
        // destroy or unload potentially active code: retain the OS mapping.
        // A real host must report and quarantine this module instance.
        int32_t code = -1;
        try { code = api_->stop(instance_); } catch (...) { code = -1; }
        if (code != 0) {
          std::fputs("UMAA module stop failed: keeping native library mapped\n", stderr);
          instance_ = nullptr;
          api_ = nullptr;
          lib_ = nullptr; // intentionally retained until process termination
          started_ = false;
          return;
        }
      }
      api_->destroy(instance_);
      instance_ = nullptr;
    }
    started_ = false;
#ifdef _WIN32
    if (lib_) { FreeLibrary(lib_); lib_ = nullptr; }
#else
    if (lib_) { dlclose(lib_); lib_ = nullptr; }
#endif
  }
#ifdef _WIN32
  HMODULE lib_{nullptr};
#else
  void* lib_{nullptr};
#endif
  const UmaaModuleV1* api_{nullptr};
  void* instance_{nullptr};
  bool started_{false};
};
}
