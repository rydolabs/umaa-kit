// Internal cross-platform loader. Never expose vendor/system loader APIs to app.
#pragma once
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace umaa::internal {
class NativeLibrary final {
public:
  explicit NativeLibrary(const std::string& path) {
#ifdef _WIN32
    h_ = LoadLibraryA(path.c_str());
#else
    h_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    if (!h_) throw std::runtime_error("cannot load native module: " + path);
  }
  NativeLibrary(const NativeLibrary&) = delete;
  NativeLibrary& operator=(const NativeLibrary&) = delete;
  ~NativeLibrary() { close(); }
  void* symbol(const char* name) const {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(h_, name));
#else
    return dlsym(h_, name);
#endif
  }
  // Never unload a module whose callbacks/threads have not been drained.
  void release_mapping() noexcept { h_ = nullptr; }
  void close() noexcept {
    if (!h_) return;
#ifdef _WIN32
    FreeLibrary(h_);
#else
    dlclose(h_);
#endif
    h_ = nullptr;
  }
private:
#ifdef _WIN32
  HMODULE h_{nullptr};
#else
  void* h_{nullptr};
#endif
};
} // namespace umaa::internal
