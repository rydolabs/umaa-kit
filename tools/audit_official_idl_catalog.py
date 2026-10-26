#!/usr/bin/env python3
"""Auditable full IDL inventory, NOT an authoritative UMAA service catalog.

The official (hash-pinned) UMAA 6.0 IDL ZIP is provided outside the repo by CI.
Every .idl is inventoried, grouped by directory as a *candidate* (not a
service). Only explicitly reviewed catalog topic mappings count as verified.
No heuristics are permitted to assign DDS topic names, roles, QoS or flow.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path, PurePosixPath
import sys

from verify_official_service_types import inspect, type_idl_path

KNOWN_ICDS = frozenset({"EO", "MO", "MM", "SA", "SEM", "SO", "CO", "EXP"})


def collect(root: Path, catalog: dict, expected_count: int = 352) -> dict:
    root = root.resolve(strict=True)
    if not root.is_dir():
        raise ValueError("official IDL root must be a directory")
    paths = sorted(root.rglob("*.idl"))
    if len(paths) != expected_count:
        raise ValueError(f"official IDL input cardinality drift: {len(paths)} != {expected_count}")
    path_set = set()
    directory_groups = defaultdict(list)
    domains = Counter()
    entries = []
    for path in paths:
        # Follow neither symlinks nor paths outside the independently supplied root.
        if path.is_symlink() or root not in path.resolve().parents:
            raise ValueError("IDL symlink or path escaping input root: " + str(path))
        rel = path.relative_to(root).as_posix()
        parts = PurePosixPath(rel).parts
        if len(parts) < 3 or parts[0] != "UMAA":
            raise ValueError("unexpected UMAA IDL path layout: " + rel)
        domain = parts[1]
        if domain != "Common" and domain not in KNOWN_ICDS:
            raise ValueError("unrecognized UMAA domain: " + domain)
        domains[domain] += 1
        if domain != "Common":
            if len(parts) < 4:
                raise ValueError("missing service-candidate path: " + rel)
            candidate = "::".join(parts[:3])
            directory_groups[candidate].append(rel)
        data = path.read_bytes()
        entries.append({"path": rel, "sha256": hashlib.sha256(data).hexdigest(),
                        "domain": domain})
        path_set.add(rel)

    check = inspect(catalog, root)
    if check["status"] != "PASS":
        raise ValueError("reviewed catalog fails official type check: " + repr(check["errors"]))
    covered_types = sorted({row["idl_path"] for row in check["verified"]})
    reviewed_services = sorted({svc["name"] for svc in catalog["services"]})
    candidate_groups = sorted(directory_groups)
    # A reviewed service must have a domain/service directory, not merely a
    # matching topic type name from an unrelated service.
    missing_reviewed = sorted(set(reviewed_services) - set(candidate_groups))
    if missing_reviewed:
        raise ValueError("reviewed service directory absent: " + repr(missing_reviewed))
    candidate_unreviewed = sorted(set(candidate_groups) - set(reviewed_services))

    # This is an IDL file inventory. Many files are nested type dependencies
    # rather than DDS message topics. Never interpret the ratio as service
    # completeness or conformance.
    result = {
        "status": "PASS",
        "scope": "official UMAA 6.0 IDL file enumeration and reviewed type existence only",
        "warning": "Directory group candidates are NOT verified ICD services, DDS topics, roles or QoS",
        "input_idl_count": len(paths),
        "domain_file_counts": dict(sorted(domains.items())),
        "directory_candidate_count": len(candidate_groups),
        "reviewed_service_directory_count": len(reviewed_services),
        "unreviewed_directory_candidate_count": len(candidate_unreviewed),
        "reviewed_topic_type_count": len(covered_types),
        "idl_files_not_explicit_reviewed_topic_types": len(path_set - set(covered_types)),
        "reviewed_service_names": reviewed_services,
        "unreviewed_directory_candidates": candidate_unreviewed,
        "candidates": [{"name": group, "idl_files": len(directory_groups[group]),
                        "files": sorted(directory_groups[group])} for group in candidate_groups],
        "idl_files": entries,
        "release_g6_complete": False,
    }
    return result


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--official-root", required=True, type=Path)
    ap.add_argument("--catalog", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--expected-count", type=int, default=352)
    ap.add_argument("--strict-g6", action="store_true",
                    help="Fail until all service contracts, QoS and behavior are reviewed")
    args = ap.parse_args(argv)
    try:
        catalog = json.loads(args.catalog.read_text(encoding="utf-8"))
        result = collect(args.official_root, catalog, args.expected_count)
        result["release_g6_complete"] = (
            catalog.get("complete") is True
            and not result["unreviewed_directory_candidates"]
            and all(s.get("official_idl_verified") and s.get("dds_qos_verified")
                    and s.get("dds_behavior_verified") for s in catalog["services"])
        )
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print("UMAA_IDL_CATALOG_AUDIT_PASS"
              f" input={result['input_idl_count']}"
              f" domains={result['domain_file_counts']}"
              f" candidate_dirs={result['directory_candidate_count']}"
              f" reviewed_dirs={result['reviewed_service_directory_count']}"
              f" unreviewed_dirs={result['unreviewed_directory_candidate_count']}"
              f" reviewed_topic_types={result['reviewed_topic_type_count']}"
              f" g6_complete={result['release_g6_complete']}")
        if args.strict_g6 and not result["release_g6_complete"]:
            print("UMAA_G6_INCOMPLETE: missing full ICD/Component/Topic/QoS/service audit", file=sys.stderr)
            return 3
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print("UMAA_IDL_CATALOG_AUDIT_FAIL: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
