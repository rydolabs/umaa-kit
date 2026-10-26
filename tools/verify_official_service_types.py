#!/usr/bin/env python3
"""Fail-closed mapping checks for a *reviewed subset* of UMAA 6.0 services.

Never auto-derive service roles or QoS from a type filename. The service
contracts remain human-reviewed ICD input. This verifies existence of the
referenced official .idl files, not runtime wire compliance.
"""
import argparse
import hashlib
import json
import pathlib
import re
import sys

FQ = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)+$")
PATTERNS = {"request_reply_without_query_data", "command_response", "publish_subscribe"}
ROLES = {"report", "command", "status", "ack", "execution_status"}


def type_idl_path(type_name):
    if not isinstance(type_name, str) or not FQ.fullmatch(type_name):
        raise ValueError("Invalid fully qualified type: " + str(type_name))
    return pathlib.Path(*type_name.split("::")).with_suffix(".idl")


def inspect(catalog, official_root=None, require_complete=False):
    errors = []
    verified = []
    basis = catalog.get("basis", {})
    if basis.get("version") != "6.0" or not re.fullmatch("[0-9a-f]{64}", str(basis.get("sha256", ""))):
        errors.append("Missing exact official ICD 6.0 provenance")
    services = catalog.get("services", [])
    if not isinstance(services, list) or not services:
        return {"status": "FAIL", "errors": errors + ["services must be nonempty"], "verified": []}
    seen = set()
    for record in services:
        name = record.get("name")
        if not isinstance(name, str) or not FQ.fullmatch(name):
            errors.append("invalid service identity"); continue
        if name in seen:
            errors.append("duplicate service identity: " + name)
        seen.add(name)
        pattern = record.get("pattern")
        if pattern not in PATTERNS or record.get("icd_reviewed") is not True:
            errors.append(name + ": unreviewed interaction contract")
        clauses = record.get("clauses", [])
        if not clauses or not all(isinstance(c, str) and re.fullmatch("[0-9]+(?:[.][0-9]+)*", c) for c in clauses):
            errors.append(name + ": missing valid ICD clause references")
        roles = []
        topic_names = set()
        for side in ("provider_topics", "consumer_topics"):
            topics = record.get(side)
            if not isinstance(topics, list):
                errors.append(name + ": " + side + " missing"); continue
            for topic in topics:
                if not isinstance(topic, dict):
                    errors.append(name + ": topic contract must be an object")
                    continue
                topic_name = topic.get("name")
                if topic_name in topic_names:
                    errors.append(name + ": duplicate topic name: " + str(topic_name))
                topic_names.add(topic_name)
                role = topic.get("role")
                if role not in ROLES:
                    errors.append(name + ": invalid role: " + str(role))
                roles.append(role)
                try:
                    rel = type_idl_path(topic.get("type"))
                    if topic.get("name") != topic.get("type"):
                        errors.append(name + ": explicit topic name differs from type; requires manual review")
                    if official_root is not None:
                        path = official_root / rel
                        if not path.is_file():
                            errors.append(name + ": official IDL not found: " + rel.as_posix())
                        else:
                            verified.append({"service": name, "role": role,
                                             "type": topic["type"], "idl_path": rel.as_posix(),
                                             "idl_sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
                except ValueError as exc:
                    errors.append(name + ": " + str(exc))
        if pattern == "command_response":
            command_types = {t["type"] for t in record.get("consumer_topics", []) if t.get("role") == "command"}
            ack_types = {t["type"] for t in record.get("provider_topics", []) if t.get("role") == "ack"}
            status_types = {t["type"] for t in record.get("provider_topics", []) if t.get("role") == "status"}
            if not command_types or not ack_types or not status_types or set(roles) - {"command", "ack", "status", "execution_status"}:
                errors.append(name + ": command response needs command, ack, status and optionally execution status")
            groups = record.get("command_groups")
            if groups is None:
                if len(command_types) != 1 or len(ack_types) != 1 or len(status_types) != 1:
                    errors.append(name + ": multiple command families require explicit command_groups")
            elif not isinstance(groups, list) or not groups:
                errors.append(name + ": command_groups must be a nonempty list")
            else:
                used = {"command": set(), "ack": set(), "status": set()}
                group_names = set()
                for group in groups:
                    if not isinstance(group, dict):
                        errors.append(name + ": invalid command group"); continue
                    label = group.get("name")
                    if not isinstance(label, str) or not re.fullmatch("[A-Za-z_][A-Za-z0-9_]*", label) or label in group_names:
                        errors.append(name + ": invalid/duplicate command group name")
                    group_names.add(label)
                    for role in ("command", "ack", "status"):
                        typename = group.get(role)
                        expected = {"command": command_types, "ack": ack_types, "status": status_types}[role]
                        if typename not in expected or typename in used[role]:
                            errors.append(name + ": missing/duplicate " + role + " contract in group " + str(label))
                        else:
                            used[role].add(typename)
                if any(used[role] != expected for role, expected in
                       (("command", command_types), ("ack", ack_types), ("status", status_types))):
                    errors.append(name + ": command_groups do not cover all command/ack/status topics")
            if roles.count("execution_status") > 1:
                errors.append(name + ": multiple execution status topics require explicit review")
        if pattern == "request_reply_without_query_data" and (not roles or set(roles) != {"report"}):
            errors.append(name + ": queryless request/reply requires one or more Report topics only")
        if pattern == "publish_subscribe" and (not roles or set(roles) != {"report"}):
            errors.append(name + ": publish-subscribe needs explicitly reviewed Report topics")
        for collection in record.get("collection_topics", []):
            try:
                type_idl_path(collection)
            except ValueError as exc:
                errors.append(name + ": " + str(exc))
    if require_complete:
        if catalog.get("complete") is not True:
            errors.append("complete=false; full UMAA service coverage unproven")
        for record in services:
            if not all(record.get(k) is True for k in
                       ("official_idl_verified", "dds_qos_verified", "dds_behavior_verified")):
                errors.append(record.get("name", "?") + ": service implementation and QoS gates not satisfied")
        if official_root is None:
            errors.append("no official IDL root for complete gate")
    return {"status": "FAIL" if errors else "PASS",
            "scope": "reviewed service mapping and referenced IDL file existence only",
            "complete": False if not require_complete else not errors,
            "reviewed_service_count": len(services),
            "verified_topic_types": len(verified),
            "verified": verified, "errors": errors}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalog", type=pathlib.Path, required=True)
    ap.add_argument("--official-root", type=pathlib.Path)
    ap.add_argument("--report", type=pathlib.Path)
    ap.add_argument("--require-complete", action="store_true")
    args = ap.parse_args()
    data = json.loads(args.catalog.read_text(encoding="utf-8"))
    result = inspect(data, args.official_root, args.require_complete)
    summary = json.dumps(result, indent=2, sort_keys=True)
    print(summary)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(summary + "\n", encoding="utf-8")
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    sys.exit(main())
