#include "umaa/module_api.h"
#include <new>
#ifndef UMAA_SERVICE_NAME
#error UMAA_SERVICE_NAME required
#endif
#ifndef UMAA_SERVICE_OP
#error UMAA_SERVICE_OP required
#endif
struct Device { bool started{false}; };
static void* make() { return new (std::nothrow) Device; }
static int32_t start(void* p) { static_cast<Device*>(p)->started=true; return 0; }
static int32_t invoke(void* p, int32_t op, int64_t input, int64_t* output) {
  if (!p || !output || !static_cast<Device*>(p)->started || op != UMAA_SERVICE_OP) return -1;
  *output = input + UMAA_RESULT_OFFSET; return 0;
}
static int32_t stop(void* p) { static_cast<Device*>(p)->started=false; return 0; }
static void destroy(void* p) { delete static_cast<Device*>(p); }
static const UmaaModuleV1 descriptor = {
  UMAA_MODULE_ABI_VERSION, sizeof(UmaaModuleV1), UMAA_SERVICE_NAME, "synthetic-demo",
  make, start, invoke, stop, destroy
};
extern "C" UMAA_MODULE_EXPORT const UmaaModuleV1* umaa_get_module_v1(uint32_t abi) {
  return abi == UMAA_MODULE_ABI_VERSION ? &descriptor : nullptr;
}
