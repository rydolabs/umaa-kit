#!/usr/bin/env python3
"""Fail-closed DDS Deployment admission report for System Composition Intent v1.

This is deliberately not a native DDS generator: the current reviewed catalog
does not prove original Topic names, instance Keys, mandatory QoS, per-operation
flow bindings or vendor runtime behavior. Never upgrade a design preview to a
runnable deployment based on user-supplied evidence booleans.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

from tools import umaa_service_contracts as contracts
from tools import umaa_system_model as common
from tools import umaa_system_planner as planner

KIND = "umaa-dds-deployment-admission"
VERSION = "1"
BLOCKED = "BLOCKED_UNAPPROVED_NATIVE_ENDPOINTS"


def _missing(code: str, subject: str, detail: str) -> dict:
    return {"code": code, "subject": subject, "status": "MISSING",
            "detail": detail}


def assess(model: dict, root: Path) -> dict:
    """Validate pinned inputs, then return deterministic, non-executable gaps.

    No files, native DDS entities or executable configuration are generated.
    Call the existing Planner as the sole topology/role/digest validator.
    """
    preview = planner.plan(model, root)
    catalog, catalog_sha = contracts._read_catalog()
    if (preview["source"]["catalogPinned"] and
            preview["source"]["catalogSha256"] != catalog_sha):
        raise common.SystemModelError("reviewed Catalog changed during admission")
    records = {row["name"]: row for row in catalog["services"]}
    checks = []
    for link in sorted(preview["connections"],
                       key=lambda x: (x["service"], x["consumer"], x["provider"])):
        service = link["service"]
        pair = {link["consumer"], link["provider"]}
        endpoints = sorted(
            (e for e in preview["endpoints"]
             if e["service"] == service and
             {e["writerComponent"], e["readerComponent"]} == pair),
            key=lambda e: e["topicId"])
        row = records.get(service)
        if row is None:
            # Only synthetic Demo:: roles can reach this path: planner.plan
            # rejects unrecognized official services and invalid roles.
            checks.append({
                "service": service, "consumer": link["consumer"],
                "provider": link["provider"], "operations": [],
                "logicalTopics": [], "catalogClaims": None,
                "missingEvidence": [_missing(
                    "SYNTHETIC_NATIVE_BINDING_UNSUPPORTED", service,
                    "Demo roles do not authorize official native DDS deployment")],
            })
            continue
        operations = [
            {"name": name, "topicRef": None, "flow": None,
             "status": "UNRESOLVED_ORIGINAL_OPERATION_TOPIC_FLOW"}
            for name in sorted(row["operations"])
        ]
        missing = [
            _missing("OPERATION_TOPIC_FLOW", service,
                     "Original ICD Operation-to-Topic/Flow binding is not approved"),
            _missing("ORIGINAL_TYPES_AND_INSTANCE_KEYS", service,
                     "Exact native IDL TypeSupport and original instance Key annotations required"),
            _missing("OFFICIAL_DDS_TOPIC_NAMES", service,
                     "Reviewed type identity does not establish the DDS Topic name"),
            _missing("MANDATORY_WRITER_READER_QOS", service,
                     "Source-approved requested/offered QoS and vendor effective QoS required"),
            _missing("PROTOCOL_LIFECYCLE", service,
                     "Official Command/Report/Dispose/Collection behavior not independently proved"),
            _missing("INDEPENDENT_VENDOR_WIRE_EVIDENCE", service,
                     "Exact-build native Writer/Reader tests required before emitting endpoints"),
        ]
        if row.get("collection_topics"):
            missing.append(_missing(
                "COLLECTION_ENDPOINT_OWNERSHIP", service,
                "Collection Element/Metadata directions and coherence must be reviewed"))
        checks.append({
            "service": service, "consumer": link["consumer"],
            "provider": link["provider"],
            "operations": operations,
            # Values come only from planner; unresolved fields must stay null.
            "logicalTopics": [{
                "topicId": e["topicId"], "idlType": e["idlType"],
                "writerComponent": e["writerComponent"],
                "readerComponent": e["readerComponent"],
                "ddsTopicName": None, "keyFields": None,
                "writerQosProfile": None, "readerQosProfile": None,
                "status": "SOURCE_BINDING_REQUIRED"
            } for e in endpoints],
            "catalogClaims": {
                "officialIdlVerified": row["official_idl_verified"],
                "ddsQosVerified": row["dds_qos_verified"],
                "ddsBehaviorVerified": row["dds_behavior_verified"],
            },
            "missingEvidence": missing,
        })

    blockers = list(preview["blockers"])
    if not preview["connections"]:
        blockers.append({
            "code": "NO_EXPLICIT_SERVICE_CONNECTIONS",
            "service": None,
            "reason": "No connected Service can authorize any native endpoint",
        })
    # The catalog cannot currently authorize a deployable Service. Preserve a
    # hard deny even if a caller forges flags or all logical links are valid.
    digest = hashlib.sha256(json.dumps(
        model, sort_keys=True, ensure_ascii=False, separators=(",", ":"),
        allow_nan=False).encode("utf-8")).hexdigest()
    return {
        "kind": KIND, "schemaVersion": VERSION, "status": BLOCKED,
        "source": {"intentCanonicalSha256": digest,
                   "catalogSha256": catalog_sha,
                   "catalogVersion": catalog["catalog_version"]},
        "deploymentSelection": preview["deploymentSelection"],
        "componentCount": preview["componentCount"],
        "connectionCount": preview["connectionCount"],
        "logicalEndpointCount": preview["endpointCount"],
        "serviceChecks": checks,
        "blockers": blockers,
        "missingEvidenceCount": sum(len(x["missingEvidence"]) for x in checks),
        "topologyReferencesValid": True,
        "nativeParticipantSpec": None,
        "nativeEndpointSpecs": [],
        "ddsDeploymentReady": False,
        "canEmitNativeEndpoints": False,
        "officialRuntimeClaim": False,
        "officialConformanceClaim": False,
    }


def assess_file(path: Path) -> dict:
    doc, _ = common._json_file(path, limit=512 * 1024)
    return assess(doc, path.resolve().parent)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Check missing original DDS evidence; never emit native endpoints")
    parser.add_argument("command", choices=["check"])
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        report = assess_file(args.input)
    except (common.SystemModelError, contracts.ContractError, OSError,
            ValueError) as exc:
        print("UMAA DEPLOYMENT ADMISSION ERROR: " + str(exc), file=sys.stderr)
        return 2
    print(json.dumps(report, sort_keys=True, indent=2))
    # A readable JSON denial is still a nonzero shell outcome for deployment CI.
    return 3


if __name__ == "__main__":
    sys.exit(main())
