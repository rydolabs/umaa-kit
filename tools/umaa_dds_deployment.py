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
from tools import umaa_original_evidence as original
from tools import umaa_eo_original_evidence as eo_original
from tools import umaa_system_model as common
from tools import umaa_system_planner as planner

KIND = "umaa-dds-deployment-admission"
VERSION = "1"
BLOCKED = "BLOCKED_UNAPPROVED_NATIVE_ENDPOINTS"


def _missing(code: str, subject: str, detail: str) -> dict:
    return {"code": code, "subject": subject, "status": "MISSING",
            "detail": detail}


def assess(model: dict, root: Path, *, official_idl_zip: Path | None = None,
           official_eo_icd: Path | None = None) -> dict:
    """Validate pinned inputs, then return deterministic, non-executable gaps.

    No files, native DDS entities or executable configuration are generated.
    Call the existing Planner as the sole topology/role/digest validator.
    """
    preview = planner.plan(model, root)
    if (official_idl_zip is None) != (official_eo_icd is None):
        raise common.SystemModelError("original IDL ZIP and EO ICD PDF must be supplied together")
    evidence = {}
    if official_idl_zip is not None:
        # Source bytes must match *both* pinned originals before any
        # Service receives reviewed literal Topic/Key/Operation data.
        selected = {x["service"] for x in preview["connections"]}
        evidence.update(eo_original.verify_originals(
            official_idl_zip, official_eo_icd, selected))
        if original.SERVICE in selected:
            evidence[original.SERVICE] = original.verify_originals(
                official_idl_zip, official_eo_icd)
    catalog, catalog_sha = contracts._read_catalog()
    if (preview["source"]["catalogPinned"] and
            preview["source"]["catalogSha256"] != catalog_sha):
        raise common.SystemModelError("reviewed Catalog changed during admission")
    records = {row["name"]: row for row in catalog["services"]}
    checks = []
    for link in sorted(preview["connections"],
                       key=lambda x: (x["service"], x["consumer"], x["provider"])):
        service = link["service"]
        row = records.get(service)
        # In two Components with both roles, A->B and B->A links may
        # coexist. Bind each endpoint to the exact reviewed topic *side*
        # and source link, not an unordered Component pair.
        directions = set()
        if row is not None:
            directions.update((t["name"], link["consumer"], link["provider"])
                              for t in row["consumer_topics"])
            directions.update((t["name"], link["provider"], link["consumer"])
                              for t in row["provider_topics"])
        endpoints = sorted(
            (e for e in preview["endpoints"]
             if e["service"] == service and
             (e["topicId"], e["writerComponent"],
              e["readerComponent"]) in directions),
            key=lambda e: e["topicId"])
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
        bindings = evidence.get(service)
        by_operation = ({x["name"]: x for x in bindings["operations"]}
                        if bindings else {})
        by_topic = ({x["topicId"]: x for x in bindings["topics"]}
                    if bindings else {})
        operations = [{
            "name": name,
            "topicRef": by_operation[name]["topicId"] if bindings else None,
            "flow": by_operation[name]["action"] if bindings else None,
            "actorRole": by_operation[name]["actor"] if bindings else None,
            "icdPage": by_operation[name]["icdPage"] if bindings else None,
            "status": ("SOURCE_PINNED_NOT_RUNTIME_APPROVED" if bindings else
                       "UNRESOLVED_ORIGINAL_OPERATION_TOPIC_FLOW"),
        } for name in sorted(row["operations"])]
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
        if bindings:
            missing = [x for x in missing if x["code"] not in {
                "OPERATION_TOPIC_FLOW", "ORIGINAL_TYPES_AND_INSTANCE_KEYS",
                "OFFICIAL_DDS_TOPIC_NAMES"}]
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
                "ddsTopicName": by_topic[e["topicId"]]["ddsTopicName"] if bindings else None,
                "keyFields": list(by_topic[e["topicId"]]["keyFields"]) if bindings else None,
                "writerQosProfile": None, "readerQosProfile": None,
                "status": ("PINNED_LITERAL_SOURCE_VERIFIED_NOT_DEPLOYABLE" if bindings else
                           "SOURCE_BINDING_REQUIRED")
            } for e in endpoints],
            "sourceRecord": (
                "catalog/umaa6-eo-" + service.split("::")[-1].lower()
                + "-source-bindings-v1.json" if bindings else None),
            # Collection element identity is source-verified separately
            # from logical report Topics; no runtime endpoint is inferred.
            "sourceCollectionTopics": ([{
                "topicId": t["topicId"], "idlType": t["idlType"],
                "ddsTopicName": t["ddsTopicName"],
                "keyFields": list(t["keyFields"]),
                "status": "SOURCE_PINNED_COLLECTION_NOT_DEPLOYABLE"
            } for t in bindings.get("collectionTopics", [])] if bindings else []),
            "partialSourceQosRules": (
                bindings.get("sourceConstraints", []) if bindings else []),
            "catalogClaims": {
                "officialIdlVerified": row["official_idl_verified"],
                "ddsQosVerified": row["dds_qos_verified"],
                "ddsBehaviorVerified": row["dds_behavior_verified"],
            },
            "missingEvidence": missing,
        })

    blockers = [x for x in preview["blockers"]
                if not (x["service"] in evidence and
                        x["code"] == "TOPIC_NAME_KEY_AND_OPERATION_MAPPING_UNRESOLVED")]
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
        "sourceVerification": ({
            "status": "PINNED_ORIGINAL_BYTES_VERIFIED_LIMITED_SCOPE",
            "services": sorted(evidence),
            "icdSha256": original.ICD_SHA,
            "idlArchiveSha256": original.ZIP_SHA,
            "semantics": "reviewed clause mapping; not QoS or runtime conformance",
        } if official_idl_zip is not None else {
            "status": "ORIGINAL_BYTES_NOT_PROVIDED", "service": None,
        }),
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


def assess_file(path: Path, *, official_idl_zip: Path | None = None,
                official_eo_icd: Path | None = None) -> dict:
    doc, _ = common._json_file(path, limit=512 * 1024)
    return assess(doc, path.resolve().parent,
                  official_idl_zip=official_idl_zip,
                  official_eo_icd=official_eo_icd)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Check missing original DDS evidence; never emit native endpoints")
    parser.add_argument("command", choices=["check"])
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--original-idl-zip", type=Path)
    parser.add_argument("--original-eo-icd", type=Path)
    args = parser.parse_args(argv)
    try:
        report = assess_file(args.input,
                             official_idl_zip=args.original_idl_zip,
                             official_eo_icd=args.original_eo_icd)
    except (common.SystemModelError, contracts.ContractError,
            original.EvidenceError, OSError, ValueError) as exc:
        print("UMAA DEPLOYMENT ADMISSION ERROR: " + str(exc), file=sys.stderr)
        return 2
    print(json.dumps(report, sort_keys=True, indent=2))
    # A readable JSON denial is still a nonzero shell outcome for deployment CI.
    return 3


if __name__ == "__main__":
    sys.exit(main())
