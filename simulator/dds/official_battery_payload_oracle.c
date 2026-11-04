/* Independent C oracle: actual original AUVSI BatteryReportType and
 * BatteryReportTypeCellsListElement, no SDK headers or codec implementation.
 * Verifies two DDS Topics and keyed Dispose, not full service conformance.
 */
#include <dds/dds.h>
#include "UMAA/EO/BatteryStatus/BatteryReportType.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int guid_is(const uint8_t id[16], uint8_t first) {
  if (id[0] != first) return 0;
  for (int i = 1; i < 16; ++i) if (id[i] != 0) return 0;
  return 1;
}

static int verify_report(const UMAA_EO_BatteryStatus_BatteryReportType* x,
                         int64_t* checkpoint_seconds, int32_t* checkpoint_nanos) {
  if (!x->chargeRemaining || *x->chargeRemaining != 80.0 ||
      !x->current || *x->current != -3.25 ||
      !x->energyUsageRate || *x->energyUsageRate != 120.5 ||
      !x->hours || *x->hours != 8.0 ||
      !x->temp || *x->temp != 25.0 ||
      !x->voltage || *x->voltage != 24.5 ||
      (int)x->state != 2 ||
      x->timeStamp.seconds != 1700000000 ||
      x->timeStamp.nanoseconds != 250 ||
      !guid_is(x->source.id, 0x33) || !guid_is(x->source.parentID, 0) ||
      !guid_is(x->cellsListMetadata.listID, 0x11) ||
      !guid_is(x->cellsListMetadata.updateElementID, 0x22) ||
      !guid_is(x->cellsListMetadata.startingElementID, 0x22) ||
      x->cellsListMetadata.size != 1 ||
      !x->cellsListMetadata.updateElementTimestamp) return 0;
  *checkpoint_seconds = x->cellsListMetadata.updateElementTimestamp->seconds;
  *checkpoint_nanos = x->cellsListMetadata.updateElementTimestamp->nanoseconds;
  return 1;
}

static int verify_element(
    const UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement* x,
    int64_t* timestamp_seconds, int32_t* timestamp_nanos) {
  if (!guid_is(x->listID, 0x11) || !guid_is(x->elementID, 0x22) ||
      x->nextElementID != NULL ||
      x->element.current != -0.25 ||
      x->element.temperature != 26.0 ||
      x->element.voltage != 3.75 ||
      x->elementTimestamp.seconds != 1700000001 ||
      x->elementTimestamp.nanoseconds != 110) return 0;
  *timestamp_seconds = x->elementTimestamp.seconds;
  *timestamp_nanos = x->elementTimestamp.nanoseconds;
  return 1;
}

int main(void) {
  dds_entity_t participant = 0;
  participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0) return 2;
  int rc = 2;
  dds_qos_t* ordered_qos = dds_create_qos();
  dds_qos_t* endpoint_qos = dds_create_qos();
  if (!ordered_qos || !endpoint_qos) goto done;
  dds_qset_presentation(ordered_qos, DDS_PRESENTATION_TOPIC, false, true);
  dds_qset_reliability(endpoint_qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
  dds_qset_history(endpoint_qos, DDS_HISTORY_KEEP_LAST, 16);
  const dds_data_representation_id_t representation = DDS_DATA_REPRESENTATION_XCDR2;
  dds_qset_data_representation(endpoint_qos, 1, &representation);
  dds_entity_t subscriber = dds_create_subscriber(participant, ordered_qos, NULL);
  if (subscriber < 0) goto done;
  dds_entity_t report_topic = dds_create_topic(
      participant, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
      "BatteryReportType", NULL, NULL);
  dds_entity_t cell_topic = dds_create_topic(
      participant, &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
      "BatteryReportTypeCellsListElement", NULL, NULL);
  if (report_topic < 0 || cell_topic < 0) goto done;
  dds_entity_t report_reader =
      dds_create_reader(subscriber, report_topic, endpoint_qos, NULL);
  dds_entity_t cell_reader =
      dds_create_reader(subscriber, cell_topic, endpoint_qos, NULL);
  if (report_reader < 0 || cell_reader < 0) goto done;

  int got_report = 0, got_cell = 0, report_dispose = 0, cell_dispose = 0;
  int64_t report_sec = 0, cell_sec = 0;
  int32_t report_ns = 0, cell_ns = 0;
  for (int tries = 0; tries < 320 &&
          !(got_report && got_cell && report_dispose && cell_dispose); ++tries) {
    for (int topic_id = 0; topic_id < 2; ++topic_id) {
      dds_sample_info_t info[1];
      if (topic_id == 0) {
        UMAA_EO_BatteryStatus_BatteryReportType sample = {0};
        void* samples[1] = {&sample};
        dds_return_t n = dds_take(report_reader, samples, info, 1, 1);
        if (n < 0) { rc = 3; goto done; }
        if (n > 0) {
          if (info[0].valid_data) {
            if (got_report || !verify_report(&sample, &report_sec, &report_ns)) {
              rc = 4; goto done;
            }
            got_report = 1;
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            if (!got_report) { rc = 5; goto done; }
            report_dispose = 1;
          }
          dds_sample_free(&sample, &UMAA_EO_BatteryStatus_BatteryReportType_desc,
                          DDS_FREE_CONTENTS);
        }
      } else {
        UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement sample = {0};
        void* samples[1] = {&sample};
        dds_return_t n = dds_take(cell_reader, samples, info, 1, 1);
        if (n < 0) { rc = 6; goto done; }
        if (n > 0) {
          if (info[0].valid_data) {
            if (got_cell || !verify_element(&sample, &cell_sec, &cell_ns)) {
              rc = 7; goto done;
            }
            got_cell = 1;
          } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
            if (!got_cell) { rc = 8; goto done; }
            cell_dispose = 1;
          }
          dds_sample_free(&sample,
              &UMAA_EO_BatteryStatus_BatteryReportTypeCellsListElement_desc,
              DDS_FREE_CONTENTS);
        }
      }
    }
    dds_sleepfor(DDS_MSECS(25));
  }

  if (!got_report || !got_cell || !report_dispose || !cell_dispose) {
    fprintf(stderr, "SIM: incomplete official Battery Report/Cell/Dispose "
            "report=%d cell=%d reportDispose=%d cellDispose=%d\n",
            got_report, got_cell, report_dispose, cell_dispose);
    rc = 9;
    goto done;
  }
  if (report_sec != cell_sec || report_ns != cell_ns) {
    puts("INDEPENDENT_BATTERY_CHECKPOINT_REJECTED");
    rc = 10;
    goto done;
  }
  puts("SIM: PASS independent official Battery Report+Cell payload and two-topic Dispose");
  rc = 0;

done:
  if (ordered_qos) dds_delete_qos(ordered_qos);
  if (endpoint_qos) dds_delete_qos(endpoint_qos);
  dds_delete(participant);
  return rc;
}
