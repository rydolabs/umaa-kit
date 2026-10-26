// Test-only ABI shim. Never installed and never used by production DDS builds.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef int32_t dds_entity_t;
typedef int32_t dds_return_t;
dds_return_t dds_write(dds_entity_t writer, const void* data);
dds_return_t dds_dispose(dds_entity_t writer, const void* data);
const char* dds_strretcode(int32_t error);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct dds_qos dds_qos_t;
typedef struct dds_topic_descriptor dds_topic_descriptor_t;
typedef struct dds_sample_info {
  bool valid_data;
  uint32_t instance_state;
} dds_sample_info_t;
#define DDS_FREE_CONTENTS (1u)
#define DDS_IST_NOT_ALIVE_DISPOSED (2u)
dds_entity_t dds_create_reader(dds_entity_t participant, dds_entity_t topic,
                               const dds_qos_t* qos, const void* listener);
dds_return_t dds_take(dds_entity_t reader, void** samples, dds_sample_info_t* info,
                      size_t bufsz, uint32_t maxs);
dds_return_t dds_delete(dds_entity_t entity);
void dds_sample_free(void* sample, const dds_topic_descriptor_t* descriptor, uint32_t op);
#ifdef __cplusplus
}
#endif
typedef struct dds_publication_matched_status {
  int32_t current_count;
} dds_publication_matched_status_t;
#define DDS_MSECS(n) ((int64_t)(n) * 1000000LL)
#define DDS_SECS(n) ((int64_t)(n) * 1000000000LL)
#ifdef __cplusplus
extern "C" {
#endif
dds_entity_t dds_create_writer(dds_entity_t participant, dds_entity_t topic,
                               const dds_qos_t* qos, const void* listener);
dds_return_t dds_get_publication_matched_status(dds_entity_t writer,
                                                dds_publication_matched_status_t* status);
dds_return_t dds_wait_for_acks(dds_entity_t writer, int64_t timeout);
void dds_sleepfor(int64_t duration);
#ifdef __cplusplus
}
#endif
