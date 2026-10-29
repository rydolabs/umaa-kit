/* UMAA development component module ABI v2.
 * Single, stable C boundary between a user device/business module DLL/SO
 * and the host. This ABI is NOT an official UMAA or K-MOSA wire ABI.
 * No STL, exceptions, compiler-specific objects, or cross-module ownership.
 * All strings/buffers used as inputs are borrowed for the call duration.
 * The caller owns response output. A buffer-too-small response reports length.
 * stop() must drain in-flight calls before returning zero; the host must NOT
 * unload a module for which stop() fails.
 */
#ifndef UMAA_MODULE_API_V2_H
#define UMAA_MODULE_API_V2_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define UMAA_COMPONENT_ABI_VERSION 2u
#define UMAA_COMPONENT_ENTRY_NAME "umaa_get_component_v2"
#if defined(_WIN32)
#define UMAA_COMPONENT_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define UMAA_COMPONENT_EXPORT __attribute__((visibility("default")))
#else
#define UMAA_COMPONENT_EXPORT
#endif
#define UMAA_COMPONENT_COMMAND 1u
#define UMAA_COMPONENT_CANCEL  2u
#define UMAA_COMPONENT_REPORT  3u
#define UMAA_COMPONENT_OK 0
#define UMAA_COMPONENT_REJECTED 1
#define UMAA_COMPONENT_NOT_FOUND -1
#define UMAA_COMPONENT_INVALID -2
#define UMAA_COMPONENT_BUFFER_TOO_SMALL -3
#define UMAA_COMPONENT_EXCEPTION -4
#define UMAA_COMPONENT_NOT_ACTIVE -5

typedef struct UmaaComponentServiceV2 {
  const char* service_id;
  const char* schema_id;
  uint32_t kinds; /* bitmask: 1=COMMAND, 2=CANCEL, 4=REPORT */
  uint32_t reserved;
} UmaaComponentServiceV2;

typedef struct UmaaComponentCallV2 {
  const char* service_id;
  uint32_t action;
  uint32_t reserved;
  const uint8_t* payload;
  uint32_t payload_size;
  uint64_t session_id;
} UmaaComponentCallV2;

typedef struct UmaaComponentResponseV2 {
  uint8_t* payload;
  uint32_t capacity;
  uint32_t size;
  int32_t business_result;
} UmaaComponentResponseV2;

typedef struct UmaaComponentModuleV2 {
  uint32_t abi_version;
  uint32_t struct_size;
  const char* module_id;
  void* (*create)(void);
  uint32_t (*service_count)(void* instance);
  int32_t (*service_at)(void* instance, uint32_t index, UmaaComponentServiceV2* out);
  int32_t (*start)(void* instance);
  int32_t (*call)(void* instance, const UmaaComponentCallV2*, UmaaComponentResponseV2*);
  int32_t (*stop)(void* instance);
  void (*destroy)(void* instance);
} UmaaComponentModuleV2;
typedef const UmaaComponentModuleV2* (*UmaaGetComponentV2)(uint32_t requested_abi);
/* Export UMAA_COMPONENT_ENTRY_NAME(uint32_t), returning NULL on ABI mismatch. */
#ifdef __cplusplus
}
#endif
#endif
