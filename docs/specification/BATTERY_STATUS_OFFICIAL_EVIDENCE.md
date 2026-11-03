# BatteryStatus official source locations

Status: source references only; no Runtime authorization.

- Original publisher: AUVSI UMAA 6.0 Engineering Operations ICD, section 6.1.5 (BatteryStatus), section 6.1.5.1 (reportBattery), and Table 18 (queryBattery/reportBattery). Public AUVSI index: https://www.auvsi.org/advocacy/advocacy-initiatives/unmanned-maritime-autonomy-architecture/
- AUVSI Component Definitions v1.0: section 1.2.1 and section 3 identify Component provider/consumer roles and configurable QoS. The exact Component and BatteryStatus applicability have not been selected or reviewed.
- EO ICD section 3.8.1: the repository's existing eo-large-collections.yaml records only a topic-scope ordered-access minimum for BatteryReportTypeCellsListElement, not a universal DDS QoS profile.
- Secondary cross-check: RTI Community's pinned ecb9236 EO ICD transcription, section 6.1.5, identifies queryBattery and reportBattery; section 6.1.5.1 identifies BatteryReportType. It is not the authoritative original.
- Prior official IDL ZIP audit recorded SHA-256 9674ef84b92cc296a544d001d58149efbb4650f4fafe3a40819636df49c42411. Original bytes have not been reauthenticated for this change.

Open: original EO PDF byte hash and clause review; Component-specific roles; complete Topic/Key and mandatory QoS; nested payload codecs; official independent vendor wire and negative tests.

Do not fill canonical YAML verification flags or unblock official Runtime from source-location evidence alone.
