/* Optional, versioned QoS extension for a V1 DDS backend.
 * V1 remains binary unchanged; hosts must not silently fall back to V1 when
 * a caller requires explicit QoS. All pointers are borrowed during call.
 */
#ifndef UMAA_DDS_BACKEND_QOS_H
#define UMAA_DDS_BACKEND_QOS_H
#include "umaa/internal/dds_backend.h"
#ifdef __cplusplus
extern "C" {
#endif
#define UMAA_DDS_QOS_ABI_VERSION 2u
#define UMAA_DDS_QOS_BACKEND_ENTRY "umaa_get_dds_qos_backend_v2"
#define UMAA_DDS_QOS_BEST_EFFORT 0u
#define UMAA_DDS_QOS_RELIABLE 1u
#define UMAA_DDS_QOS_VOLATILE 0u
#define UMAA_DDS_QOS_TRANSIENT_LOCAL 1u
#define UMAA_DDS_QOS_KEEP_LAST 0u
#define UMAA_DDS_QOS_KEEP_ALL 1u
#define UMAA_DDS_QOS_PRESENTATION_INSTANCE 0u
#define UMAA_DDS_QOS_PRESENTATION_TOPIC 1u
#define UMAA_DDS_QOS_PRESENTATION_GROUP 2u
typedef struct UmaaDdsQosV2 {
  uint32_t struct_size;
  uint32_t reliability;
  uint32_t durability;
  uint32_t history_kind;
  uint32_t history_depth; /* 1..4096 when KEEP_LAST; 0 for KEEP_ALL */
  uint32_t presentation_scope;
  uint32_t ordered_access;
  uint32_t coherent_access;
  const char* partition; /* single partition, empty means default */
} UmaaDdsQosV2;
typedef struct UmaaDdsQosBackendV2 {
  uint32_t abi_version;
  uint32_t struct_size;
  uint64_t capabilities;
  void* (*open_writer_qos)(void*, const UmaaDdsTopicV1*, const UmaaDdsQosV2*);
  void* (*open_reader_qos)(void*, const UmaaDdsTopicV1*, const UmaaDdsQosV2*,
                          UmaaDdsOnSampleV1, void*);
} UmaaDdsQosBackendV2;
typedef const UmaaDdsQosBackendV2* (*UmaaGetDdsQosBackendV2)(uint32_t);
#ifdef __cplusplus
}
#endif
#endif
