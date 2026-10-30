/* THE DDS VENDOR IMPLEMENTATION CONTRACT (C ABI v1).
 * A new middleware vendor implements this ONE table in its private adapter
 * directory; it never edits UMAA command/report engines or user modules.
 * Public SDK headers and UMAA protocol core MUST NOT include DDS headers.
 * The payload is a vendor-neutral, schema-identified representation produced
 * by generated service codecs. It is NOT raw DDS wire bytes. The backend's
 * per-vendor generated TypeBinding must map it to official IDL TypeSupport.
 * Unknown schema, missing TypeBinding or unsupported mandatory QoS FAIL CLOSED.
 * All pointers are borrowed for call duration, except opaque handles created
 * and closed by the same backend. A callback's sample is callback-scoped.
 * On ambiguous write outcome, return UNKNOWN_DELIVERY, never retry implicitly.
 * No exceptions across this ABI. Functions must be safe for independent
 * endpoints on a domain; shutdown must drain callbacks before returning.
 */
#ifndef UMAA_DDS_BACKEND_H
#define UMAA_DDS_BACKEND_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define UMAA_DDS_ABI_VERSION 1u
#define UMAA_DDS_BACKEND_ENTRY "umaa_get_dds_backend_v1"
#if defined(_WIN32)
#define UMAA_DDS_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define UMAA_DDS_EXPORT __attribute__((visibility("default")))
#else
#define UMAA_DDS_EXPORT
#endif
#define UMAA_DDS_CAP_RELIABLE (1ull << 0)
#define UMAA_DDS_CAP_INSTANCE_LIFECYCLE (1ull << 1)
#define UMAA_DDS_CAP_DISCOVERY (1ull << 2)
#define UMAA_DDS_CAP_COHERENT_GROUP (1ull << 3)
#define UMAA_DDS_CAP_ORDERED_ACCESS (1ull << 4)
#define UMAA_DDS_CAP_LIVELINESS (1ull << 5)
#define UMAA_DDS_OK 0
#define UMAA_DDS_UNSUPPORTED -1
#define UMAA_DDS_INVALID -2
#define UMAA_DDS_IO_ERROR -3
#define UMAA_DDS_UNKNOWN_DELIVERY -4
#define UMAA_DDS_NOT_FOUND -5
#define UMAA_DDS_BUFFER_TOO_SMALL -6
#define UMAA_DDS_EVENT_DATA 1u
#define UMAA_DDS_EVENT_DISPOSE 2u
#define UMAA_DDS_EVENT_UNREGISTER 3u

typedef struct UmaaDdsTopicV1 {
  const char* topic_name;
  const char* schema_id;      /* pinned official type+key contract ID */
  const char* qos_profile;    /* reviewed policy ID (not an arbitrary guess) */
  uint64_t required_features; /* required capability mask */
} UmaaDdsTopicV1;

typedef struct UmaaDdsSampleV1 {
  const uint8_t* payload;
  uint32_t payload_size;
  const char* instance_key;
  int64_t source_timestamp_ns;
  uint32_t event_kind;
} UmaaDdsSampleV1;

typedef void (*UmaaDdsOnSampleV1)(void* user, const UmaaDdsSampleV1* sample);

typedef struct UmaaDdsBackendV1 {
  uint32_t abi_version;
  uint32_t struct_size;
  const char* vendor_name;
  uint64_t capabilities;
  void* (*open_domain)(uint32_t domain_id);
  void (*close_domain)(void* domain);
  void* (*open_writer)(void* domain, const UmaaDdsTopicV1* topic);
  void* (*open_reader)(void* domain, const UmaaDdsTopicV1* topic,
                       UmaaDdsOnSampleV1 callback, void* callback_user);
  int32_t (*write)(void* writer, const UmaaDdsSampleV1* sample);
  int32_t (*dispose)(void* writer, const UmaaDdsSampleV1* sample);
  int32_t (*unregister_instance)(void* writer, const UmaaDdsSampleV1* sample);
  int32_t (*poll)(void* reader, uint32_t max_samples);
  int32_t (*wait_for_acks)(void* writer, uint32_t timeout_ms);
  void (*close_writer)(void* writer);
  void (*close_reader)(void* reader);
} UmaaDdsBackendV1;
typedef const UmaaDdsBackendV1* (*UmaaGetDdsBackendV1)(uint32_t requested_abi);
/* Export UMAA_DDS_BACKEND_ENTRY(uint32_t), NULL on ABI mismatch. */
#ifdef __cplusplus
}
#endif
#endif
