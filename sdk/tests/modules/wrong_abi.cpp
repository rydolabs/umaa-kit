#include "umaa/module_api.h"
static const UmaaModuleV1 incompatible = {99u, sizeof(UmaaModuleV1), "Demo::EO::EngineControl", "demo-only",
    nullptr, nullptr, nullptr, nullptr, nullptr};
extern "C" UMAA_MODULE_EXPORT const UmaaModuleV1* umaa_get_module_v1(uint32_t) {
  return &incompatible;
}
