#!/usr/bin/env python3
"""Evidence-only reconciliation of pinned EO BatteryStatus PDF, IDL and RTI example.

Never copies original sources to Git; never generates approved DDS/QoS Runtime
bindings. A PDF digest authenticates bytes, NOT semantic clause interpretation.
The existing official IDL declaration auditor is the only IDL parsing surface.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

from tools.official_icd_evidence import (
    SourceError, check_idl_imports, inspected_zip, sha, source_type,
)
from tools.validate_icd_yaml import require_dependencies, strict_load

PIN_MANIFEST = (
    Path(__file__).resolve().parents[1] /
    "catalog/official-evidence/eo-battery-status-6.0.json"
)
MAX_PDF = 64 * 1024 * 1024
MAX_XML = 1024 * 1024
MAX_MANIFEST = 16 * 1024
REVIEWED_QOS = (
    Path(__file__).resolve().parents[1] / "contracts/qos/umaa6/eo-large-collections.yaml"
)


def _read_regular(path: Path, maximum: int, what: str) -> bytes:
    if (not path.is_file() or path.is_symlink() or
            path.stat().st_size <= 0 or path.stat().st_size > maximum):
        raise SourceError(what + " absent, symlinked or oversized")
    return path.read_bytes()


def _reviewed_qos_minimum(path: Path, service: str, element: str, minimum: dict) -> dict:
    """Cross-check the existing scoped EO normative-minimum ledger.

    This is NOT a full QoS policy, nor a normative PDF content parser.
    """
    yaml, _ = require_dependencies()
    record = strict_load(path, yaml)
    if (record.get("kind") != "QosRequirements" or
            record.get("schemaVersion") != "1" or
            record.get("source") != {
                "document": "UMAA-SPEC-EOICD-v6.0", "section": "3.8.1", "page": 14
            }):
        raise SourceError("existing scoped QoS source locator mismatch")
    rows = [x for x in record.get("entries", []) if x.get("serviceId") == service]
    if (len(rows) != 1 or rows[0].get("idlType") != element or
            rows[0].get("required") != {
                "presentationScope": "topic", "orderedAccess": True
            } or minimum.get("appliesTo") != "collectionElement"):
        raise SourceError("existing scoped collection-element QoS minimum drift")
    return {"reviewedScopedQosFileMatches": True,
            "requiredScope": "collection-element only",
            "fullServiceQosApproved": False}


def _reference_xml(xml_path: Path, digest: str) -> dict:
    raw = _read_regular(xml_path, MAX_XML, "RTI reference XML")
    if sha(raw) != digest:
        raise SourceError("RTI example XML source digest mismatch")
    if b"<!DOCTYPE" in raw.upper() or b"<!ENTITY" in raw.upper():
        raise SourceError("external XML entities/DOCTYPE are prohibited")
    try:
        root = ET.fromstring(raw)
    except ET.ParseError as exc:
        raise SourceError("invalid RTI example XML") from exc
    if root.tag != "dds":
        raise SourceError("unexpected RTI example XML root")
    library = root.find("./qos_library[@name='UMAAQoSLib']")
    if library is None:
        raise SourceError("RTI example QoS library identity missing")
    profiles = library.findall("./qos_profile")
    names = [p.get("name") for p in profiles]
    if len(names) != len(set(names)) or not {"DefaultUMAAParticipant", "ElementQoS"}.issubset(names):
        raise SourceError("RTI example missing/duplicate expected QoS profiles")
    participant = next(p for p in profiles if p.get("name") == "DefaultUMAAParticipant")
    for role in ("publisher_qos", "subscriber_qos"):
        prefix = "./" + role + "/presentation/"
        if (participant.findtext(prefix + "access_scope") != "TOPIC_PRESENTATION_QOS" or
                participant.findtext(prefix + "ordered_access") != "true"):
            raise SourceError("RTI example reference lacks topic/ordered Presentation in " + role)
    return {
        "sha256": digest,
        "library": "UMAAQoSLib",
        "profilesObserved": ["DefaultUMAAParticipant", "ElementQoS"],
        "publisherSubscriberTopicOrderingExamplePresent": True,
        "referenceOnly": True,
        "rtiConnextOnlyLicense": True,
        "officialQosApproval": False,
    }


def inspect(archive: Path, original_icd: Path, *,
            manifest: Path = PIN_MANIFEST,
            rti_reference_xml: Path | None = None,
            scoped_qos: Path = REVIEWED_QOS) -> dict:
    """Authenticate source bytes and existing Topic/Key IDL declarations.

    This intentionally does NOT interpret PDF semantics, infer a complete DDS
    QoS profile, validate arbitrary payload fields, or authorize Runtime.
    """
    manifest_raw = _read_regular(manifest, MAX_MANIFEST, "evidence manifest")
    try:
        declared = json.loads(manifest_raw)
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise SourceError("invalid evidence manifest JSON") from exc
    if (declared.get("schemaVersion") != 1 or
            declared.get("kind") != "SOURCE_PINNED_UNAPPROVED_BATTERY_STATUS_REFERENCE" or
            declared.get("serviceId") != "UMAA::EO::BatteryStatus" or
            set(declared.get("topics", {})) != {"report", "collectionElement"} or
            declared.get("qualification", {}).get("officialConformanceClaim") is not False or
            declared.get("qualification", {}).get("nativeRuntimeApproved") is not False):
        raise SourceError("untrusted/incomplete source evidence reference")
    pins = declared["sourcePins"]
    icd_raw = _read_regular(original_icd, MAX_PDF, "original EO ICD")
    if not icd_raw.startswith(b"%PDF-") or sha(icd_raw) != pins["originalEoIcdSha256"]:
        raise SourceError("official EO ICD PDF source digest mismatch")
    archive_raw = _read_regular(archive, 32 * 1024 * 1024, "official IDL ZIP")
    if sha(archive_raw) != pins["officialIdlArchiveSha256"]:
        raise SourceError("official IDL ZIP digest mismatch")

    with inspected_zip(archive) as opened:
        closure = check_idl_imports(opened)
        observed = {}
        for role in ("report", "collectionElement"):
            required = declared["topics"][role]
            actual = source_type(opened, required["idlType"])
            for actual_key, expected_key in (
                ("idlPath", "idlPath"), ("idlSha256", "idlSha256"),
                ("idlType", "idlType"), ("ddsTopicName", "ddsTopicName"),
                ("keyFields", "keyFields"), ("keyFieldTypes", "keyFieldTypes"),
            ):
                if actual[actual_key] != required[expected_key]:
                    raise SourceError("official IDL declaration mismatch: " +
                                      role + "/" + actual_key)
            observed[role] = actual

    if observed["report"]["ddsTopicName"] == observed["collectionElement"]["ddsTopicName"]:
        raise SourceError("Report and Collection must have separate DDS Topics")
    qos = declared["collectionElementQosMinimum"]
    if (qos.get("icdClause") != "3.8.1" or qos.get("printedPage") != 14 or
            qos.get("appliesTo") != "collectionElement" or
            qos.get("presentationAccessScope") != "TOPIC_PRESENTATION_QOS" or
            qos.get("orderedAccess") is not True or
            qos.get("publisherAndSubscriber") is not True or
            qos.get("crossTopicOrderingGuaranteed") is not False):
        raise SourceError("unsupported or broadened pinned collection QoS minimum")

    reviewed_qos = _reviewed_qos_minimum(
        scoped_qos, declared["serviceId"], observed["collectionElement"]["idlType"], qos
    )
    sample = (
        _reference_xml(rti_reference_xml, pins["rtiExampleQosXmlSha256"])
        if rti_reference_xml is not None else None
    )
    return {
        "schemaVersion": 1,
        "kind": "SOURCE_CHECKED_UNAPPROVED_BATTERY_STATUS_EVIDENCE",
        "serviceId": declared["serviceId"],
        "sourcePins": {
            "originalEoIcdSha256": pins["originalEoIcdSha256"],
            "officialIdlArchiveSha256": pins["officialIdlArchiveSha256"],
        },
        "sourceIntegrity": {
            "idlFiles": closure["idlFiles"],
            "idlImportReferences": closure["idlImportReferences"],
            "allIdlImportsPresent": closure["allIdlImportsPresent"],
            "originalIcdPdfBytesMatchPin": True,
            "originalIcdClauseTextMachineReviewed": False,
            "idlTopicAndKeyDeclarationsMatchPin": True,
        },
        "icdEvidenceLocators": declared["icdEvidence"],
        "topicDeclarations": observed,
        "collectionElementQosMinimum": qos,
        "existingReviewedQosMinimum": reviewed_qos,
        "rtiExampleQosReference": sample,
        "readiness": {
            "officialQosComplete": False,
            "officialPayloadCodecReviewed": False,
            "componentProviderRoleReviewed": False,
            "collectionRuntimeReviewed": False,
            "nativeRuntimeAuthorized": False,
            "officialConformanceClaim": False,
        },
        "blockers": [
            "The pinned PDF digest does not machine-verify ICD clause semantics",
            "Only the Large Collection element topic Presentation minimum is source-located",
            "No complete Report or Element DDS QoS profile or component-level QoS is approved",
            "Actual official nested payload codec and optional/union mapping unverified",
            "Collection metadata/element sequencing, recovery, Dispose and negative E2E unverified",
            "RTI example QoS is reference-only and is not portable to Cyclone/Fast DDS",
        ],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Check EO BatteryStatus source pins (never runtime approval)")
    parser.add_argument("--idl-zip", type=Path, required=True)
    parser.add_argument("--original-icd", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=PIN_MANIFEST)
    parser.add_argument("--rti-reference-xml", type=Path)
    parser.add_argument("--reviewed-qos", type=Path, default=REVIEWED_QOS)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)
    try:
        result = inspect(args.idl_zip, args.original_icd, manifest=args.manifest,
                         rti_reference_xml=args.rti_reference_xml,
                         scoped_qos=args.reviewed_qos)
        output = json.dumps(result, sort_keys=True, indent=2) + "\n"
        if args.out is not None:
            if (args.out.is_symlink() or args.out.exists() or
                    any(part.is_symlink() for part in args.out.parents)):
                raise SourceError("refuse overwriting or symlinked source evidence output")
            args.out.parent.mkdir(parents=True, exist_ok=True)
            with args.out.open("x", encoding="utf-8") as fd:
                fd.write(output)
        else:
            print(output, end="")
        print("UMAA_BATTERY_SOURCE_EVIDENCE_ONLY_NOT_RUNTIME_APPROVED", file=sys.stderr)
        return 0
    except (SourceError, OSError, ValueError, KeyError, TypeError) as exc:
        print("UMAA_BATTERY_SOURCE_EVIDENCE_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
