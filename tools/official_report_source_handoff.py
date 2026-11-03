#!/usr/bin/env python3
"""Source-pinned candidate handoff for one UMAA official Report.

This tool checks external original-byte digests and a narrow IDL Topic/@key
declaration audit. It does NOT approve normative QoS, IDL semantics, payload
codecs, Component roles, service lifecycle, or native runtime generation.
No original source bytes are included in its output.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

from tools.official_icd_evidence import SourceError, audit, sha
from tools.umaa_contract_ir import load_service

_SHA = re.compile(r"^[0-9a-f]{64}$")
_ORIGINAL_LIMIT = 64 * 1024 * 1024


def inspect(archive: Path, yaml_path: Path, original_icd: Path,
            expected_archive_sha256: str) -> dict:
    """Produce an unapproved, source-checked single-Report handoff record.

    The original ICD is authenticated by byte digest only. Its normative
    interpretation and the Component Definition remain unverified.
    """
    if not _SHA.fullmatch(expected_archive_sha256):
        raise SourceError("explicit 64-hex official IDL archive pin required")
    if (not archive.is_file() or archive.is_symlink() or
            not original_icd.is_file() or original_icd.is_symlink() or
            original_icd.stat().st_size > _ORIGINAL_LIMIT):
        raise SourceError("original archive/ICD must be regular, bounded files")
    archive_sha = sha(archive.read_bytes())
    if archive_sha != expected_archive_sha256:
        raise SourceError("official IDL archive SHA-256 mismatch")

    service, yaml_sha = load_service(yaml_path)
    service_id = service["metadata"]["id"]
    if not service_id.startswith("UMAA::"):
        raise SourceError("only UMAA official Service IDs can be reviewed")
    source = service["sources"]["icd"]
    if source["reference"]["kind"] != "official":
        raise SourceError("secondary ICD transcription is not authoritative")
    if not source["originalSha256"] or source["originalSha256"] != sha(original_icd.read_bytes()):
        raise SourceError("original ICD SHA-256 mismatch or missing source pin")
    if service["sources"]["idl"]["bundleSha256"] != archive_sha:
        raise SourceError("YAML official IDL archive pin does not match input")

    topics = service["spec"]["topics"]
    reports = [(alias, topic) for alias, topic in topics.items()
               if topic["role"] == "report"]
    protocols = service["spec"]["protocols"]
    if (len(topics) != 1 or len(reports) != 1 or
            reports[0][1]["direction"] != "provider-to-consumer" or
            len(protocols) != 1 or protocols[0]["kind"] != "request-reply" or
            protocols[0].get("mode") != "without-query-data" or
            protocols[0]["bindings"] != {"reports": [reports[0][0]]}):
        raise SourceError("first candidate supports exactly one queryless Report Topic")
    alias, topic = reports[0]
    if topic["ddsTopicName"] is None or topic["keyFields"] is None:
        raise SourceError("Report Topic/Key YAML must be explicitly source-filled")

    evidence = audit(archive, [yaml_path])
    if evidence["status"] != "PASS" or evidence["topicsAudited"] != 1:
        raise SourceError("IDL Topic/@key source disagreement: " + "; ".join(evidence["issues"]))
    audited = evidence["services"][0]["topics"][0]["source"]
    if audited["ddsTopicName"] != topic["ddsTopicName"] or audited["keyFields"] != topic["keyFields"]:
        raise SourceError("selected Report Topic/Key declaration mismatch")

    operations = service["spec"]["operations"]
    unresolved = sorted(
        name for name, item in operations.items()
        if (item["kind"] == "unresolved" or item["topicRef"] != alias or
            item["sourceClause"] not in source["clauses"])
    )
    blockers = [
        "Original ICD and Component Definition clauses and applicability require independent normative review",
        "DDS Key declaration extraction is not compiled/keyed identity semantic proof",
        "Mandatory QoS policies are not verified by IDL Topic declarations",
        "Official nested/bounded payload-to-SDK codec and round-trip tests are missing",
        "Independent vendor runtime, Dispose and negative-oracle tests are missing",
    ]
    if unresolved:
        blockers.append("Unresolved or mismatched service Operations: " + ",".join(unresolved))
    if topic["qosProfileRef"] is None:
        blockers.append("DDS QoS profile reference is unresolved in Service YAML")
    if (service["spec"]["collections"] or service["spec"]["specializations"] or
            service["spec"]["unresolvedCollectionTypes"]):
        blockers.append("Collection and specialization normative/native mappings not proved")

    return {
        "schemaVersion": 1,
        "kind": "UNAPPROVED_OFFICIAL_REPORT_SOURCE_CANDIDATE",
        "serviceId": service_id,
        "source": {
            "serviceYamlSha256": yaml_sha,
            "originalIcdSha256": source["originalSha256"],
            "idlArchiveSha256": archive_sha,
            "idlMemberSha256": audited["idlSha256"],
            "idlMemberPath": audited["idlPath"],
            "icdDocument": source["document"],
            "icdVersion": source["version"],
            "declaredIcdClauses": source["clauses"],
        },
        "report": {
            "alias": alias,
            "idlType": audited["idlType"],
            "ddsTopicName": audited["ddsTopicName"],
            "declaredKeyFields": audited["keyFields"],
            "declaredKeyTypes": audited["keyFieldTypes"],
            "qosProfileRef": topic["qosProfileRef"],
            "direction": topic["direction"],
        },
        "sourceAudit": {
            "allIdlImportsPresent": evidence["archiveIntegrity"]["allIdlImportsPresent"],
            "limitedToTopicConstantAndKeyDeclaration": True,
            "originalIcdBytesMatchDeclaredHash": True,
            "normativeIcdSemanticsReviewed": False,
        },
        "unresolvedOperations": unresolved,
        "nativeDdsBinding": False,
        "componentRuntimeBound": False,
        "officialConformanceClaim": False,
        "readyForOfficialNativeRuntime": False,
        "blockers": blockers,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Emit source-checked, NOT-approved UMAA Report binding candidate"
    )
    parser.add_argument("--zip", required=True, type=Path)
    parser.add_argument("--yaml", required=True, type=Path)
    parser.add_argument("--original-icd", required=True, type=Path)
    parser.add_argument("--expect-sha256", required=True)
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)
    try:
        record = inspect(args.zip, args.yaml, args.original_icd, args.expect_sha256)
        body = json.dumps(record, indent=2, sort_keys=True) + "\n"
        if args.out:
            if args.out.is_symlink() or args.out.exists() or any(
                p.is_symlink() for p in args.out.parents
            ):
                raise SourceError("refuse overwriting or symlinked candidate output")
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(body, encoding="utf-8")
        else:
            print(body, end="")
        print("UMAA_OFFICIAL_REPORT_CANDIDATE_NOT_APPROVED", file=sys.stderr)
        return 0
    except (SourceError, ValueError, OSError) as exc:
        print("UMAA_OFFICIAL_REPORT_CANDIDATE_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
