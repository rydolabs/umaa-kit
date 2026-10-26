#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define UMAA_MODULE_ABI_VERSION 1u
#if defined(_WIN32)
#define UMAA_MODULE_EXPORT __declspec(dllexport)
#else
#define UMAA_MODULE_EXPORT __attribute__((visibility("default")))
#endif
typedef struct UmaaModuleV1 {
  uint32_t abi_version;
  uint32_t struct_size;
  const char* service_name;
  const char* schema_id;
  void* (*create)(void);
  int32_t (*start)(void*);
  int32_t (*invoke)(void*, int32_t, int64_t, int64_t*);
  int32_t (*stop)(void*);
  void (*destroy)(void*);
} UmaaModuleV1;
typedef const UmaaModuleV1* (*UmaaGetModuleV1)(uint32_t requested_abi);
#ifdef __cplusplus
}
#endif
