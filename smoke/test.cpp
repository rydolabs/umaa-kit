#include "umaa/module_api.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
using NativeLib = HMODULE;
static NativeLib open_lib(const char* p) { return LoadLibraryA(p); }
static void* symbol(NativeLib h) { return reinterpret_cast<void*>(GetProcAddress(h,"umaa_get_module_v1")); }
static void close_lib(NativeLib h) { FreeLibrary(h); }
#else
#include <dlfcn.h>
using NativeLib = void*;
static NativeLib open_lib(const char* p) { return dlopen(p,RTLD_NOW|RTLD_LOCAL); }
static void* symbol(NativeLib h) { return dlsym(h,"umaa_get_module_v1"); }
static void close_lib(NativeLib h) { dlclose(h); }
#endif
static int test_one(const char* path, const char* expected, int op, int offset) {
  NativeLib lib = open_lib(path);
  if (!lib) return 1;
  auto getter = reinterpret_cast<UmaaGetModuleV1>(symbol(lib));
  if (!getter) return 2;
  if (getter(99) != nullptr) return 3; // ABI rejection
  const UmaaModuleV1* api = getter(UMAA_MODULE_ABI_VERSION);
  if (!api || api->abi_version != UMAA_MODULE_ABI_VERSION ||
      api->struct_size != sizeof(UmaaModuleV1) ||
      std::string(api->service_name) != expected ||
      std::string(api->schema_id) != "synthetic-demo") return 4;
  void* instance = api->create();
  if (!instance || api->start(instance) != 0) return 5;
  int64_t out=0;
  if (api->invoke(instance,op,1200,&out) != 0 || out != 1200+offset) return 6;
  if (api->invoke(instance,op+10,1200,&out) == 0) return 7;
  if (api->stop(instance) != 0) return 8;
  api->destroy(instance);
  close_lib(lib);
  return 0;
}
int main(int argc,char** argv) {
  if (argc != 3) return 9;
  for (int iteration=0;iteration<2;++iteration) {
    int a=test_one(argv[1],"Demo::EO::EngineControl",1,0);
    int b=test_one(argv[2],"Demo::EO::BatteryStatus",2,80);
    if (a || b) { std::cerr << "module test failed " << a << " " << b << "\n"; return 10; }
  }
  std::cout << "PASS: Windows DLL / Linux SO service ABI load, call, stop, unload, reload\n";
}
