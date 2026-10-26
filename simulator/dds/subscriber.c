// Independent Simulator: uses only official Cyclone DDS C API and generated IDL type;
// does not link any SDK protocol/state-machine implementation.
#include <dds/dds.h>
#include "DemoBattery.h"
#include <stdio.h>

int main(void) {
  dds_entity_t participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0) { fprintf(stderr, "DDS participant creation failed: %d (%s)\n", (int)participant, dds_strretcode(-participant)); return 2; }
  dds_entity_t topic = dds_create_topic(participant, &Demo_EO_BatteryReportType_desc,
                                        "Demo_EO_BatteryReportType", NULL, NULL);
  if (topic < 0) { fprintf(stderr, "DDS topic creation failed: %d (%s)\n", (int)topic, dds_strretcode(-topic)); return 2; }
  dds_qos_t *qos = dds_create_qos();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_SECS(1));
  dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 16);
  dds_entity_t reader = dds_create_reader(participant, topic, qos, NULL);
  dds_delete_qos(qos);
  if (reader < 0) { fprintf(stderr, "DDS reader creation failed: %d (%s)\n", (int)reader, dds_strretcode(-reader)); return 2; }
  Demo_EO_BatteryReportType sample = {0};
  void *samples[1] = { &sample };
  dds_sample_info_t info[1];
  int phase = 0;
  for (int tries = 0; tries < 250 && phase < 3; ++tries) {
    dds_return_t n = dds_take(reader, samples, info, 1, 1);
    if (n < 0) { fprintf(stderr, "SIM: take returned %d\n", (int)n); return 2; }
    if (n == 0) { dds_sleepfor(DDS_MSECS(25)); continue; }
    if (info[0].valid_data) {
      int expected = phase == 0 ? 20 : 80;
      if (phase >= 2 || sample.source_id != 17 || sample.level_percent != expected) {
        fprintf(stderr, "SIM: unexpected DDS valid-data phase %d id=%d level=%d\n",phase,
                (int)sample.source_id, (int)sample.level_percent);
        return 4;
      }
      printf("SIM: valid DDS sample id=%d level=%d\n",(int)sample.source_id,(int)sample.level_percent);
      ++phase;
    } else if (info[0].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) {
      if (phase != 2) { fprintf(stderr,"SIM: premature DDS Dispose\n"); return 5; }
      puts("SIM: DDS keyed instance dispose observed");
      ++phase;
    }
  }
  dds_delete(participant);
  if (phase != 3) { fprintf(stderr,"SIM: timeout, only %d of 3 DDS events observed\n",phase); return 6; }
  puts("SIM: PASS real Cyclone DDS independent process\n");
  return 0;
}
