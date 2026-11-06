#!/usr/bin/env python3
"""Verify one original, SHA-pinned UMAA 6.0 Service literal binding set.

Only source-based names/keys/operation intentions are checked. No generated
Topic, approved QoS, service runtime or normative conformance is implied.
The original ICD PDF and IDL archive remain external (see their Terms of Use).
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import re
import zipfile

from tools import umaa_service_contracts as contracts
from tools import umaa_system_model as common

ROOT = Path(__file__).resolve().parents[1]
RECORD = ROOT / "catalog/umaa6-eo-anchorcontrol-source-bindings-v1.json"
SERVICE = "UMAA::EO::AnchorControl"
PREFIX = SERVICE + "::"
ICD_SHA = "47e88ecec73e37562ad04f5cf1ee5aef62b4de7f75e5ef993828f6dc8440d453"
ZIP_SHA = "15e71e24df4663e8b4891c461f0e37823bce5087f0b5d5a52667f3c25cb56d08"
EXPECTED_TYPES = {
    "AnchorCommandType": (
        "consumer", ("source", "sessionID", "destination"), 51,
        "2bb1a684c2872da59f3eb6d28dfd8e5f80f06c5a2bab50b5cd93495531d0acfc"),
    "AnchorCommandAckReportType": (
        "provider", ("source", "sessionID"), 50,
        "828ee7973f0901c37f5e3fa9953f6af26a860f72e9f67168ca9632bc2a2f39e9"),
    "AnchorCommandStatusType": (
        "provider", ("source", "sessionID"), 51,
        "55444ab08774be86169d243a76362f81833a21ec4e78352404e81ee4538d3b2a"),
}
# Explicitly reviewed in EO ICD v6.0 §6.1 and §6.1.1 pp. 50–51.
# Query subscribes; cancel disposes the original command instance.
EXPECTED_OPERATIONS = {
    "reportAnchorCommandAck": ("AnchorCommandAckReportType", "write", "provider", 50),
    "reportAnchorCommandStatus": ("AnchorCommandStatusType", "write", "provider", 51),
    "setAnchor": ("AnchorCommandType", "write", "consumer", 51),
    "queryAnchorCommandAck": ("AnchorCommandAckReportType", "subscribe", "consumer", 50),
    "cancelAnchorCommand": ("AnchorCommandType", "dispose", "consumer", 50),
}


class EvidenceError(ValueError):
    """Original source mismatch or unapproved service binding."""


def _required(row, keys, where):
    if not isinstance(row, dict) or set(row) != set(keys):
        raise EvidenceError(where + ": invalid fields")


def load_record() -> dict:
    record, _ = common._json_file(RECORD, limit=24_000)
    _required(record, ("kind", "schemaVersion", "scope", "service",
                       "sources", "topics", "operations"), "source record")
    if (record["kind"] != "umaa-source-pinned-service-bindings" or
            record["schemaVersion"] != "1" or record["service"] != SERVICE or
            record["scope"] != "EO AnchorControl literal source mapping; not approved native deployment"):
        raise EvidenceError("source record: wrong scope or version")
    _required(record["sources"], ("icd", "idlArchive"), "sources")
    icd, archive = record["sources"]["icd"], record["sources"]["idlArchive"]
    _required(icd, ("document", "version", "sha256", "pages"), "ICD")
    _required(archive, ("version", "sha256"), "IDL archive")
    if (icd != {"document": "UMAA-SPEC-EOICD", "version": "6.0",
                "sha256": ICD_SHA, "pages": [50, 51]} or
            archive != {"version": "6.0", "sha256": ZIP_SHA}):
        raise EvidenceError("unpinned original source identity")
    if not isinstance(record["topics"], list) or len(record["topics"]) != 3:
        raise EvidenceError("expected exactly three AnchorControl topics")
    catalog, _ = contracts._read_catalog()
    service = next((x for x in catalog["services"] if x["name"] == SERVICE), None)
    if service is None:
        raise EvidenceError("AnchorControl absent from verified reviewed catalog")
    expected = {}
    for side, role in (("consumer_topics", "consumer"), ("provider_topics", "provider")):
        for topic in service[side]:
            expected[topic["name"]] = (topic["type"], role)
    if set(expected) != {PREFIX + name for name in EXPECTED_TYPES}:
        raise EvidenceError("Catalog AnchorControl topic mapping changed")
    seen = set()
    for topic in record["topics"]:
        _required(topic, ("topicId", "idlType", "ddsTopicName", "idlPath",
                          "idlSha256", "keyFields", "writerRole", "icdPage"),
                  "IDL topic")
        topic_id = topic["topicId"]
        if not isinstance(topic_id, str) or topic_id not in expected or topic_id in seen:
            raise EvidenceError("unknown or duplicate AnchorControl topic")
        seen.add(topic_id)
        name = topic_id[len(PREFIX):]
        role, keys, page, digest = EXPECTED_TYPES[name]
        if topic != {
            "topicId": topic_id, "idlType": expected[topic_id][0],
            "ddsTopicName": topic_id,
            "idlPath": "UMAA/EO/AnchorControl/" + name + ".idl",
            "idlSha256": digest, "keyFields": list(keys),
            "writerRole": role, "icdPage": page,
        } or expected[topic_id][1] != role:
            raise EvidenceError("unreviewed AnchorControl Topic / Key / direction")
    if not isinstance(record["operations"], list) or len(record["operations"]) != 5:
        raise EvidenceError("expected exactly five reviewed AnchorControl operations")
    if set(service["operations"]) != set(EXPECTED_OPERATIONS):
        raise EvidenceError("Catalog AnchorControl operation identities changed")
    seen = set()
    for row in record["operations"]:
        _required(row, ("name", "topicId", "action", "actor", "icdPage"),
                  "Operation")
        name = row["name"]
        if not isinstance(name, str) or name not in EXPECTED_OPERATIONS or name in seen:
            raise EvidenceError("unknown or duplicate operation")
        seen.add(name)
        typename, action, actor, page = EXPECTED_OPERATIONS[name]
        if row != {
            "name": name, "topicId": PREFIX + typename,
            "action": action, "actor": actor, "icdPage": page,
        }:
            raise EvidenceError("unreviewed Operation/Topic/transport mapping")
    return record


def _read_pinned_file(path: Path, max_bytes: int, expected_digest: str) -> bytes:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > max_bytes:
        raise EvidenceError("missing, symlinked or oversized original source")
    content = path.read_bytes()
    if hashlib.sha256(content).hexdigest() != expected_digest:
        raise EvidenceError("original source SHA-256 mismatch")
    return content


def _literal_layout(idl: str, typename: str) -> tuple[str, list[str]]:
    if not isinstance(idl, str):
        raise EvidenceError("invalid IDL text")
    const = re.findall(
        r'\bconst\s+string\s+' + re.escape(typename) +
        r'Topic\s*=\s*"([^"]+)"\s*;', idl)
    structures = re.findall(
        r'\bstruct\s+' + re.escape(typename) +
        r'\s*\{([^}]*)\}\s*;', idl, re.S)
    if len(const) != 1 or len(structures) != 1:
        raise EvidenceError("missing or ambiguous literal Topic/struct")
    keys = re.findall(
        r'@key\s+[A-Za-z0-9_:]+\s+([A-Za-z_][A-Za-z0-9_]*)\s*;',
        structures[0])
    return const[0], keys


def verify_originals(idl_archive: Path, eo_icd_pdf: Path) -> dict:
    """Require the exact original ZIP/PDF bytes, then parse all three IDL types.

    ICD clause/page mapping is a reviewed assertion anchored to exact PDF SHA,
    not a general-purpose PDF semantic parser or QoS/conformance approval.
    """
    record = load_record()
    _read_pinned_file(Path(eo_icd_pdf), 4_000_000, ICD_SHA)
    archive_bytes = _read_pinned_file(Path(idl_archive), 2_000_000, ZIP_SHA)
    from io import BytesIO
    try:
        with zipfile.ZipFile(BytesIO(archive_bytes)) as archive:
            paths = archive.namelist()
            for topic in record["topics"]:
                path = topic["idlPath"]
                if paths.count(path) != 1:
                    raise EvidenceError("missing or duplicated original IDL file")
                if archive.getinfo(path).file_size > 64_000:
                    raise EvidenceError("oversized source IDL entry")
                contents = archive.read(path)
                if hashlib.sha256(contents).hexdigest() != topic["idlSha256"]:
                    raise EvidenceError("source IDL entry digest mismatch")
                try:
                    body = contents.decode("utf-8")
                except UnicodeDecodeError as exc:
                    raise EvidenceError("invalid UTF-8 original IDL") from exc
                actual_topic, actual_keys = _literal_layout(
                    body, topic["idlType"].split("::")[-1])
                if actual_topic != topic["ddsTopicName"] or actual_keys != topic["keyFields"]:
                    raise EvidenceError("original IDL Topic constant or @key fields differ")
    except (zipfile.BadZipFile, RuntimeError, EOFError, ValueError) as exc:
        if isinstance(exc, EvidenceError):
            raise
        raise EvidenceError("unreadable original IDL archive") from exc
    return record
