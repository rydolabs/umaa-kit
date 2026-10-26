#!/usr/bin/env python3
"""Extract literal UMAA 6.0 ICD operation/topic facts, never DDS role guesses.

Input is a pinned public markdown *transcription* of Distro A ICDs. AUVSI
documents remain normative: these extracted records are a review ledger, not
a completed service contract. No query RPC or QoS is inferred from names.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

DOMAINS = ("EO", "MO", "MM", "SA", "SEM", "SO")
SERVICE = re.compile(r"\*\*((?:5|6)\.1\.(\d+)) ([A-Za-z][A-Za-z0-9]*)\*\*")
OPERATION = re.compile(r"\*\*((?:5|6)\.1\.(\d+)\.(\d+)) ([A-Za-z][A-Za-z0-9]*)\*\*")
NAMESPACE = re.compile(r"\*\*Namespace:\*\*\s*(UMAA::[A-Za-z0-9_:]+)")
TOPIC = re.compile(r"\*\*Topic:\*\*\s*([A-Za-z_][A-Za-z0-9_]*)")
DATA_TYPE = re.compile(r"\*\*Data Type:\*\*\s*([A-Za-z_][A-Za-z0-9_]*)")


def extract(code: str, markdown: str):
    if code not in DOMAINS:
        raise ValueError("unsupported reviewed ICD domain " + code)
    if "Version 6.0" not in markdown[:2000]:
        raise ValueError(code + ": missing exact 6.0 document identity")
    services = list(SERVICE.finditer(markdown))
    if not services:
        raise ValueError(code + ": no service sections found")
    records = []
    for n, match in enumerate(services):
        section, ordinal, name = match.groups()
        group = markdown[match.start():services[n + 1].start() if n + 1 < len(services) else len(markdown)]
        ops = list(OPERATION.finditer(group))
        if not ops:
            raise ValueError(code + ": service has no operation declarations: " + name)
        for i, op in enumerate(ops):
            clause, op_service_num, op_num, operation = op.groups()
            if not clause.startswith(section + ".") or op_service_num != ordinal:
                raise ValueError(code + ": operation is misnested: " + clause)
            body = group[op.start():ops[i + 1].start() if i + 1 < len(ops) else len(group)]
            namespace = NAMESPACE.search(body)
            topic = TOPIC.search(body)
            dtype = DATA_TYPE.search(body)
            expected = "UMAA::" + code + "::" + name
            if not all((namespace, topic, dtype)):
                raise ValueError(code + ": missing explicit Namespace/Topic/Data Type in " + clause)
            if namespace.group(1) != expected or topic.group(1) != dtype.group(1):
                raise ValueError(code + ": mismatched namespace/topic/type in " + clause)
            relative = Path("UMAA", code, name, topic.group(1) + ".idl")
            records.append({
                "service": expected, "service_section": section,
                "operation": operation, "operation_clause": clause,
                "namespace": namespace.group(1), "topic": topic.group(1),
                "data_type": dtype.group(1), "idl_path": relative.as_posix(),
                "review_status": "EXTRACTED_UNREVIEWED",
            })
    if len({(x["service"], x["operation_clause"]) for x in records}) != len(records):
        raise ValueError(code + ": duplicate section/operation")
    return records


def audit(markdown_dir: Path, official_root: Path, catalog=None, revision=None, complete_domains=()):
    all_items = []
    sources = {}
    for domain in DOMAINS:
        path = markdown_dir / ("UMAA-SPEC-" + domain + "ICD.md")
        contents = path.read_text(encoding="utf-8")
        parsed = extract(domain, contents)
        sources[domain] = {
            "transcription_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "service_sections": len({r["service"] for r in parsed}),
            "operations": len(parsed),
        }
        all_items.extend(parsed)
    missing = []
    for r in all_items:
        if not (official_root / r["idl_path"]).is_file():
            missing.append(r["idl_path"])
        else:
            r["idl_exists"] = True
    if missing:
        raise ValueError("ICD declarations absent from official IDL: " + repr(sorted(set(missing))[:18]))
    reviewed = set()
    if catalog is not None:
        for svc in catalog["services"]:
            for topic in svc["provider_topics"] + svc["consumer_topics"]:
                reviewed.add((svc["name"], topic["name"].split("::")[-1]))
        available = {(r["service"], r["topic"]) for r in all_items}
        absent = sorted(reviewed - available)
        if absent:
            raise ValueError("reviewed contract missing from explicit ICD section: " + repr(absent))
    completeness = {}
    for domain in complete_domains:
        if domain not in DOMAINS:
            raise ValueError("unsupported ICD domain for completeness: " + domain)
        if catalog is None:
            raise ValueError("ICD domain mapping requires curated catalog")
        documented = {(r["service"], r["topic"]) for r in all_items
                      if r["service"].startswith("UMAA::" + domain + "::")}
        curated = {item for item in reviewed if item[0].startswith("UMAA::" + domain + "::")}
        missing = sorted(documented - curated)
        if missing:
            raise ValueError(domain + " ICD Topic mapping incomplete: " + repr(missing[:16]))
        if not documented:
            raise ValueError(domain + " has no explicit ICD declarations")
        completeness[domain] = {"topic_mappings": len(documented),
                                "service_sections": len({s for s, _ in documented}),
                                "all_explicit_topic_ids_reviewed": True,
                                "protocol_qos_verified": False}
    service_names = {r["service"] for r in all_items}
    distinct_topics = {(r["service"], r["topic"]) for r in all_items}
    return {
        "status": "PASS",
        "source_revision": revision,
        "scope": "six pinned public ICD Markdown transcriptions; literal operation/topic extraction only",
        "caveat": "Not the official Component Definitions, DDS role, QoS, or service conformance proof. CO and EXP are not covered.",
        "source_documents": sources,
        "icd_service_sections": len(service_names),
        "icd_operation_entries": len(all_items),
        "distinct_service_topic_types": len(distinct_topics),
        "reviewed_catalog_topics_cross_checked": len(reviewed),
        "all_explicit_icd_types_in_official_idl": True,
        "domain_topic_mapping_checks": completeness,
        "operations": all_items,
        "g6_complete": False,
    }


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--markdown-dir", required=True, type=Path)
    ap.add_argument("--official-root", required=True, type=Path)
    ap.add_argument("--catalog", type=Path)
    ap.add_argument("--source-revision", default="unspecified")
    ap.add_argument("--require-domain-topic-complete", action="append", default=[])
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--expect-services", type=int, default=53)
    ap.add_argument("--expect-operations", type=int, default=113)
    args = ap.parse_args(argv)
    try:
        reviewed = json.loads(args.catalog.read_text()) if args.catalog else None
        result = audit(args.markdown_dir, args.official_root, reviewed, args.source_revision,
                       args.require_domain_topic_complete)
        if result["icd_service_sections"] != args.expect_services or result["icd_operation_entries"] != args.expect_operations:
            raise ValueError("ICD extraction cardinality changed: "
                             f"{result['icd_service_sections']} services, {result['icd_operation_entries']} operations")
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, sort_keys=True, indent=2) + "\n", encoding="utf-8")
        print("UMAA_ICD_OPERATION_LEDGER_PASS"
              f" sections={result['icd_service_sections']}"
              f" operations={result['icd_operation_entries']}"
              f" distinct_topics={result['distinct_service_topic_types']}"
              f" reviewed_topics={result['reviewed_catalog_topics_cross_checked']}"
              f" completed_domain_topic_maps={list(result['domain_topic_mapping_checks'])}"
              " g6_complete=False")
        return 0
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        print("UMAA_ICD_OPERATION_LEDGER_FAIL: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
