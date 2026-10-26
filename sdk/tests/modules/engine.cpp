#include "umaa/module_api.h"
#include <new>
struct Engine { bool running{false}; };
static void* create_engine() { return new (std::nothrow) Engine(); }
static int32_t start_engine(void* ctx) { static_cast<Engine*>(ctx)->running = true; return 0; }
static int32_t call_engine(void* ctx, int32_t operation, int64_t rpm, int64_t* output) {
  if (!ctx || !output || !static_cast<Engine*>(ctx)->running || operation != 1 || rpm < 0 || rpm > 10000) return -1;
  *output = rpm; // demo-only action, not real engine actuation
  return 0;
}
static int32_t stop_engine(void* ctx) { static_cast<Engine*>(ctx)->running = false; return 0; }
static void destroy_engine(void* ctx) { delete static_cast<Engine*>(ctx); }
static const UmaaModuleV1 api = {UMAA_MODULE_ABI_VERSION, sizeof(UmaaModuleV1),
    "Demo::EO::EngineControl", "demo-only", &create_engine, &start_engine,
    &call_engine, &stop_engine, &destroy_engine};
extern "C" UMAA_MODULE_EXPORT const UmaaModuleV1* umaa_get_module_v1(uint32_t version) {
  return version == UMAA_MODULE_ABI_VERSION ? &api : nullptr;
}
