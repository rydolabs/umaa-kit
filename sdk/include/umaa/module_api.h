/* Prototype stable C boundary for UMAA service plugins (NOT an official K-MOSA ABI).
 * One shared library per service. Host owns all wire protocol, DDS objects and
 * command sessions; plugin owns only the device/business implementation.
 */
#ifndef UMAA_MODULE_API_H
#define UMAA_MODULE_API_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define UMAA_MODULE_ABI_VERSION 1u
#define UMAA_MODULE_ENTRY_NAME "umaa_get_module_v1"
#if defined(_WIN32)
#define UMAA_MODULE_EXPORT __declspec(dllexport)
#else
#define UMAA_MODULE_EXPORT __attribute__((visibility("default")))
#endif
/* All pointers remain plugin-owned. Strings must remain valid until unload.
 * Functions must never throw exceptions across the boundary. Invocation uses
 * DEMO-ONLY scalar operations; it is NOT a UMAA message/IDL serialization API.
 * Return 0 on success, nonzero on error. 'stop' must finish safely before
 * unload. No raw C++ objects, STL types or shared allocation across ABI.
 */
typedef struct UmaaModuleV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    const char* service_name;
    const char* schema_id;
    void* (*create)(void);
    int32_t (*start)(void* instance);
    int32_t (*invoke)(void* instance, int32_t operation, int64_t input, int64_t* output);
    int32_t (*stop)(void* instance);
    void (*destroy)(void* instance);
} UmaaModuleV1;
typedef const UmaaModuleV1* (*UmaaGetModuleV1)(uint32_t requested_abi);
/* Every service library must export:
 * UMAA_MODULE_EXPORT const UmaaModuleV1* umaa_get_module_v1(uint32_t);
 */
#ifdef __cplusplus
}
#endif
#endif
