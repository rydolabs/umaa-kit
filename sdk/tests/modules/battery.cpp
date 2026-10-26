#include "umaa/module_api.h"
#include <new>
struct Battery { bool running{false}; };
static void* create_battery() { return new (std::nothrow) Battery(); }
static int32_t start_battery(void* ctx) { static_cast<Battery*>(ctx)->running = true; return 0; }
static int32_t call_battery(void* ctx, int32_t operation, int64_t, int64_t* output) {
  if (!ctx || !output || !static_cast<Battery*>(ctx)->running || operation != 2) return -1;
  *output = 80; // demo-only virtual battery value
  return 0;
}
static int32_t stop_battery(void* ctx) { static_cast<Battery*>(ctx)->running = false; return 0; }
static void destroy_battery(void* ctx) { delete static_cast<Battery*>(ctx); }
static const UmaaModuleV1 api = {UMAA_MODULE_ABI_VERSION, sizeof(UmaaModuleV1),
    "Demo::EO::BatteryStatus", "demo-only", &create_battery, &start_battery,
    &call_battery, &stop_battery, &destroy_battery};
extern "C" UMAA_MODULE_EXPORT const UmaaModuleV1* umaa_get_module_v1(uint32_t version) {
  return version == UMAA_MODULE_ABI_VERSION ? &api : nullptr;
}
