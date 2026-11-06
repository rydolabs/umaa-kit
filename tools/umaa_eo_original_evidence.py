#!/usr/bin/env python3
"""Independently pinned, limited EO BatteryStatus / EngineControl source evidence.

Original AUVSI EO ICD PDF and original IDL ZIP are external, never shipped.
IDL literal Topic/Key validation does not approve native vendor DDS creation.
"""
from __future__ import annotations

from io import BytesIO
import hashlib
from pathlib import Path
import zipfile

from tools import umaa_original_evidence as anchor
from tools import umaa_service_contracts as contracts
from tools import umaa_system_model as common

ROOT = Path(__file__).resolve().parents[1]
BATTERY = "UMAA::EO::BatteryStatus"
ENGINE = "UMAA::EO::EngineControl"
SUPPORTED = frozenset({BATTERY, ENGINE})

# Independently hand-reviewed from EO ICD v6.0 §§3.8.1, 6.1.5, 6.1.6
# and exact original archive. These values must NOT be derived from JSON
# record fields, user input, Type filenames or guessed middleware defaults.
TYPE_SPECS = {
    BATTERY: {
        "BatteryReportType": (
            "BatteryReportType", "39a2c920cfaba9afc3c15c4c12f5d4470911ab8b7c197c9e27b08284e54b713c",
            ("source",), "provider", 55),
    },
    ENGINE: {
        "EngineCommandAckReportType": (
            "EngineCommandAckReportType", "6a38a6b0074f88dac5059ea1af1204366dc43b6948575bbeb0b6bdda6b7e723b",
            ("source", "sessionID"), "provider", 56),
        "EngineCommandStatusType": (
            "EngineCommandStatusType", "bd70cf804985ae172e51635722597b99217ac65eca0be36222b02707f4a5e83f",
            ("source", "sessionID"), "provider", 56),
        "EngineCommandType": (
            "EngineCommandType", "399177418274f7492f4493d6e154492bce9b4e24083ed3f19e91b79ab4d9a119",
            ("source", "sessionID", "destination"), "consumer", 56),
    },
}
COLLECTION_SPECS = {
    BATTERY: {
        "BatteryReportTypeCellsListElement": (
            "BatteryReportType", "39a2c920cfaba9afc3c15c4c12f5d4470911ab8b7c197c9e27b08284e54b713c",
            ("listID", "elementID"), "provider", 55),
    },
    ENGINE: {},
}
OPERATION_SPECS = {
    BATTERY: {
        "queryBattery": ("BatteryReportType", "subscribe", "consumer", 54),
        "reportBattery": ("BatteryReportType", "write", "provider", 55),
    },
    ENGINE: {
        "setEngine": ("EngineCommandType", "write", "consumer", 56),
        "queryEngineCommandAck": ("EngineCommandAckReportType", "subscribe", "consumer", 55),
        "cancelEngineCommand": ("EngineCommandType", "dispose", "consumer", 55),
        "reportEngineCommandStatus": ("EngineCommandStatusType", "write", "provider", 56),
        "reportEngineCommandAck": ("EngineCommandAckReportType", "write", "provider", 56),
    },
}
RECORDS = {
    BATTERY: ROOT / "catalog/umaa6-eo-batterystatus-source-bindings-v1.json",
    ENGINE: ROOT / "catalog/umaa6-eo-enginecontrol-source-bindings-v1.json",
}
ICD_PAGES = {BATTERY: [54, 55], ENGINE: [55, 56]}


def _expected_topic(service: str, name: str, spec: tuple) -> dict:
    origin, digest, keys, role, page = spec
    full = service + "::" + name
    return {
        "topicId": full, "idlType": full, "ddsTopicName": full,
        "idlPath": "UMAA/EO/" + service.split("::")[-1] + "/" + origin + ".idl",
        "idlSha256": digest, "keyFields": list(keys),
        "writerRole": role, "icdPage": page,
    }


def _strict_set(rows: list, expected: dict, where: str):
    if not isinstance(rows, list) or len(rows) != len(expected):
        raise anchor.EvidenceError(where + ": missing/extra entries")
    seen = set()
    for row in rows:
        if not isinstance(row, dict):
            raise anchor.EvidenceError(where + ": nonobject entry")
        key = row.get("topicId") if where != "operations" else row.get("name")
        if not isinstance(key, str) or key in seen or row != expected.get(key):
            raise anchor.EvidenceError(where + ": unknown, mutated or duplicate binding")
        seen.add(key)


def load_record(service: str) -> dict:
    """Validate exact reviewed source facts, Catalog identities and no injected fields."""
    if service not in SUPPORTED:
        raise anchor.EvidenceError("unreviewed EO service evidence")
    record, _ = common._json_file(RECORDS[service], limit=32_000)
    keys = {"kind", "schemaVersion", "scope", "service", "sources",
            "topics", "operations", "collectionTopics"}
    if service == BATTERY:
        keys.add("sourceConstraints")
    anchor._required(record, keys, "EO source record")
    if (record["kind"] != "umaa-source-pinned-service-bindings" or
            record["schemaVersion"] != "1" or
            record["scope"] != "EO " + service.split("::")[-1] +
            " literal source mapping; not approved native deployment" or
            record["service"] != service):
        raise anchor.EvidenceError("EO record identity changed")
    anchor._required(record["sources"], ("icd", "idlArchive"), "sources")
    if record["sources"] != {
        "icd": {"document": "UMAA-SPEC-EOICD", "version": "6.0",
                "sha256": anchor.ICD_SHA, "pages": ICD_PAGES[service]},
        "idlArchive": {"version": "6.0", "sha256": anchor.ZIP_SHA},
    }:
        raise anchor.EvidenceError("EO source file identity not pinned")

    catalog, _ = contracts._read_catalog()
    catalog_row = next((x for x in catalog["services"] if x["name"] == service), None)
    if catalog_row is None:
        raise anchor.EvidenceError("selected EO Service missing from reviewed Catalog")
    catalog_topics = {
        t["name"]: (t["type"], role)
        for side, role in (("provider_topics", "provider"),
                           ("consumer_topics", "consumer"))
        for t in catalog_row[side]
    }
    expected_topics = {
        service + "::" + name: _expected_topic(service, name, spec)
        for name, spec in TYPE_SPECS[service].items()
    }
    if catalog_topics != {
        key: (key, topic["writerRole"]) for key, topic in expected_topics.items()
    }:
        raise anchor.EvidenceError("reviewed Catalog EO Topic directions changed")
    _strict_set(record["topics"], expected_topics, "topics")
    expected_collections = {
        service + "::" + name: _expected_topic(service, name, spec)
        for name, spec in COLLECTION_SPECS[service].items()
    }
    if set(catalog_row.get("collection_topics", [])) != set(expected_collections):
        raise anchor.EvidenceError("reviewed Catalog Collection identity changed")
    _strict_set(record["collectionTopics"], expected_collections, "collection topics")

    expected_ops = {}
    for name, (topic, action, actor, page) in OPERATION_SPECS[service].items():
        expected_ops[name] = {
            "name": name, "topicId": service + "::" + topic,
            "action": action, "actor": actor, "icdPage": page,
        }
    if set(catalog_row["operations"]) != set(expected_ops):
        raise anchor.EvidenceError("reviewed Catalog Operation identity changed")
    _strict_set(record["operations"], expected_ops, "operations")

    if service == BATTERY:
        # EO §3.8.1 p14 is a *necessary* Collection element presentation
        # rule, not a complete effective DDS writer/reader QoS profile.
        rule = [{
            "topicId": service + "::BatteryReportTypeCellsListElement",
            "rule": "collection-element-presentation", "accessScope": "topic",
            "orderedAccess": True, "icdPage": 14,
            "status": "SOURCE_RULE_NOT_COMPLETE_EFFECTIVE_QOS",
        }]
        if record["sourceConstraints"] != rule:
            raise anchor.EvidenceError("unreviewed Collection QoS source rule")
    return record


def verify_originals(idl_archive: Path, eo_icd_pdf: Path,
                     services: set[str]) -> dict[str, dict]:
    """Verify originals once and only project explicitly selected reviewed Services.

    SHA checks are on full original PDF/ZIP bytes. The operation and QoS
    clause anchors are hand-reviewed, not automatically PDF-semantic parsed.
    """
    selected = sorted(set(services) & SUPPORTED)
    anchor._read_pinned_file(Path(eo_icd_pdf), 4_000_000, anchor.ICD_SHA)
    zip_bytes = anchor._read_pinned_file(Path(idl_archive), 2_000_000, anchor.ZIP_SHA)
    records = {service: load_record(service) for service in selected}
    try:
        with zipfile.ZipFile(BytesIO(zip_bytes)) as zipped:
            paths = zipped.namelist()
            for service, record in records.items():
                for topic in record["topics"] + record["collectionTopics"]:
                    path = topic["idlPath"]
                    if paths.count(path) != 1:
                        raise anchor.EvidenceError("original IDL entry missing or duplicated")
                    entry = zipped.getinfo(path)
                    if entry.file_size > 64_000:
                        raise anchor.EvidenceError("original IDL entry too large")
                    source = zipped.read(path)
                    if hashlib.sha256(source).hexdigest() != topic["idlSha256"]:
                        raise anchor.EvidenceError("original IDL entry digest mismatch")
                    try:
                        literal, keys = anchor._literal_layout(
                            source.decode("utf-8"), topic["idlType"].split("::")[-1])
                    except UnicodeDecodeError as exc:
                        raise anchor.EvidenceError("original IDL is not UTF-8") from exc
                    if (literal != topic["ddsTopicName"] or
                            keys != topic["keyFields"]):
                        raise anchor.EvidenceError("original IDL Topic/Key differs")
    except (zipfile.BadZipFile, EOFError, RuntimeError) as exc:
        raise anchor.EvidenceError("corrupt original IDL ZIP") from exc
    return records
