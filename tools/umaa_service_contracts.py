#!/usr/bin/env python3
"""Deterministic, provenance-pinned projection of a REVIEWED UMAA ICD catalog.

This is a machine-readable *reviewed mapping*, not authoritative UMAA IDL,
native DDS QoS, Component Definition roles, or runtime conformance.
The official original ICD/IDL are separate acceptance evidence inputs.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "catalog/umaa6-six-icd-reviewed.json"
SNAPSHOT_KIND = "umaa-reviewed-service-contract-snapshot"
SNAPSHOT_VERSION = "1"
CATALOG_PATH = "catalog/umaa6-six-icd-reviewed.json"


class ContractError(ValueError):
    pass


def _read_catalog(path: Path = CATALOG) -> tuple[dict, str]:
    from tools.verify_official_service_types import inspect

    if path.is_symlink() or not path.is_file() or path.stat().st_size > 4_000_000:
        raise ContractError("missing, symlinked or oversized reviewed catalog")
    data = path.read_bytes()
    try:
        catalog = json.loads(data)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ContractError("invalid reviewed catalog JSON") from exc
    result = inspect(catalog)
    if result["status"] != "PASS":
        raise ContractError("reviewed catalog failed ICD mapping validator: " +
                            repr(result["errors"][:4]))
    if catalog.get("complete") is not False:
        raise ContractError("partial catalog must not claim full coverage")
    return catalog, hashlib.sha256(data).hexdigest()


def catalog_names(path: Path = CATALOG) -> tuple[set[str], str]:
    """All names have passed the same source validator as SDK registry/codegen."""
    catalog, digest = _read_catalog(path)
    return {item["name"] for item in catalog["services"]}, digest


def create_snapshot(path: Path = CATALOG) -> dict:
    catalog, digest = _read_catalog(path)
    services = []
    for record in catalog["services"]:
        topics = []
        for side, direction in (("provider_topics", "provider"),
                                ("consumer_topics", "consumer")):
            for item in record[side]:
                topics.append({"id": item["name"], "type": item["type"],
                               "direction": direction, "role": item["role"]})
        services.append({
            "id": record["name"],
            "pattern": record["pattern"],
            "operations": list(record["operations"]),
            "topics": topics,
            "collectionTypes": list(record.get("collection_topics", [])),
            "commandGroups": list(record.get("command_groups", [])),
            "source": {
                "clauses": list(record["clauses"]),
                "reference": record.get("source"),  # None means provenance not recorded; never fabricate.
            },
            "evidence": {
                "icdMappingReviewed": record["icd_reviewed"] is True,
                "officialIdlVerified": record["official_idl_verified"] is True,
                "ddsQosVerified": record["dds_qos_verified"] is True,
                "ddsBehaviorVerified": record["dds_behavior_verified"] is True,
            },
        })
    return {
        "kind": SNAPSHOT_KIND,
        "schemaVersion": SNAPSHOT_VERSION,
        "source": {
            "catalogPath": CATALOG_PATH,
            "catalogSha256": digest,
            "catalogVersion": catalog["catalog_version"],
            "scope": "partial-reviewed-mapping-not-normative-runtime",
            "basis": catalog["basis"],
            "ledgerSourceRevision": catalog["ledger_source_revision"],
        },
        "services": services,
    }


def serialized(snapshot: dict) -> str:
    return json.dumps(snapshot, sort_keys=True, indent=2, ensure_ascii=False) + "\n"


def validate_snapshot(snapshot: object, catalog_path: Path = CATALOG) -> dict:
    """Exact trusted projection check: rejects invented QoS, keys and source claims.

    The full input must match the projection of the currently reviewed, pinned
    catalog. No remote URL or attached model can silently supply authority.
    """
    if not isinstance(snapshot, dict):
        raise ContractError("snapshot must be a JSON object")
    if snapshot.get("kind") != SNAPSHOT_KIND or snapshot.get("schemaVersion") != SNAPSHOT_VERSION:
        raise ContractError("unknown Service Contract snapshot kind/version")
    expected = create_snapshot(catalog_path)
    if snapshot != expected:
        raise ContractError("Service Contract/source digest mismatch or unreviewed fields")
    return {
        "status": "REVIEWED_MAPPING_ONLY",
        "services": len(expected["services"]),
        "topics": sum(len(s["topics"]) for s in expected["services"]),
        "catalogSha256": expected["source"]["catalogSha256"],
        "normativeQosClaim": False,
        "officialServiceRuntimeClaim": False,
    }


def read_snapshot(path: Path, catalog_path: Path = CATALOG) -> dict:
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 4_000_000:
        raise ContractError("snapshot missing, symlinked or oversized")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise ContractError("invalid snapshot JSON") from exc
    validate_snapshot(data, catalog_path)
    return data


def write_snapshot(path: Path, snapshot: dict, catalog_path: Path = CATALOG) -> None:
    validate_snapshot(snapshot, catalog_path)
    if path.is_symlink() or any(parent.is_symlink() for parent in path.parents):
        raise ContractError("refusing symlink output")
    if path.exists():
        # A non-generated user file must never be replaced.
        try:
            existing = json.loads(path.read_text(encoding="utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise ContractError("refusing to replace non-Contract JSON file") from exc
        if not isinstance(existing, dict) or existing.get("kind") != SNAPSHOT_KIND:
            raise ContractError("refusing to replace non-generated Service Contract file")
    path.parent.mkdir(parents=True, exist_ok=True)
    # An on-disk snapshot is a review/export artifact. It is not the official
    # source of truth and must be regenerated after a catalog revision.
    import os
    import tempfile
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent,
                                     prefix=".umaa-contract-", delete=False) as stream:
        temp = Path(stream.name)
        stream.write(serialized(snapshot))
    try:
        os.replace(temp, path)
    finally:
        temp.unlink(missing_ok=True)
