#!/usr/bin/env python3
"""Strict draft K-UMAA ServiceICD YAML checker (not official runtime approval).

Usage: python -m contracts.validate contracts/services/SA/GlobalPoseStatus.service.yaml
Python dependencies are intentionally isolated from the core C++ SDK.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

import jsonschema
import yaml

CONTRACTS = Path(__file__).resolve().parent
ROOT = CONTRACTS.parent
SCHEMA = CONTRACTS / "schemas/service-icd-v1alpha1.schema.json"


class ContractError(ValueError):
    pass


class StrictLoader(yaml.SafeLoader):
    def compose_node(self, parent, index):
        if self.check_event(yaml.AliasEvent):
            raise ContractError("YAML aliases/anchors are not supported in canonical ICD contracts")
        return super().compose_node(parent, index)

    def construct_mapping(self, node, deep=False):
        if not isinstance(node, yaml.MappingNode):
            raise ContractError("YAML mapping expected")
        result = {}
        for key_node, value_node in node.value:
            if key_node.tag == "tag:yaml.org,2002:merge":
                raise ContractError("YAML '<<' merge is prohibited for normative contract fields")
            key = self.construct_object(key_node, deep=deep)
            if not isinstance(key, str):
                raise ContractError("ICD YAML mapping keys must be strings (quote YAML booleans)")
            if key in result:
                raise ContractError("duplicate ICD YAML mapping key: " + key)
            result[key] = self.construct_object(value_node, deep=deep)
        return result


def _validate_imports(doc: dict) -> None:
    for item in doc.get("imports", []):
        relative = Path(item["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ContractError("import escapes contracts root")
        candidate = CONTRACTS / relative
        if candidate.is_symlink():
            raise ContractError("symlink imports are not permitted")
        file = candidate.resolve()
        if not file.is_relative_to(CONTRACTS.resolve()) or not file.is_file():
            raise ContractError("contract import missing or outside contracts root")
        if file.is_symlink() or file.stat().st_size > 262144:
            raise ContractError("unsafe contract import")
        digest = hashlib.sha256(file.read_bytes()).hexdigest()
        if digest != item["sha256"]:
            raise ContractError("contract import pin does not match " + item["path"])


def _validate_references(doc: dict) -> None:
    spec = doc["spec"]
    topics = spec["topics"]
    ops = spec["operations"]
    for name, op in ops.items():
        ref = op.get("topicRef")
        if op["transport"] in {"publish", "subscribe", "command", "dds-dispose"} and ref is None:
            raise ContractError("operation requires existing Topic: " + name)
        if ref is not None and ref not in topics:
            raise ContractError("operation references unknown Topic: " + name)
        if op["transport"] == "subscribe" and op["actor"] != "consumer":
            raise ContractError("subscribe must be a Consumer operation: " + name)
        if op["transport"] == "publish" and op["actor"] != "provider":
            raise ContractError("Report publish must be a Provider operation: " + name)
        if op["transport"] in {"command", "dds-dispose"}:
            if (op["actor"] != "consumer" or topics[ref]["role"] != "command"
                    or topics[ref]["writer"] != "consumer"
                    or topics[ref]["reader"] != "provider"):
                raise ContractError("Command and Cancel must target an existing Consumer-written command Topic: " + name)
        if op["transport"] == "dds-native" and ref is not None:
            raise ContractError("Direct DDS operation cannot invent another Topic: " + name)
        if ref and op["transport"] in {"publish", "subscribe"}:
            endpoint = "writer" if op["transport"] == "publish" else "reader"
            if topics[ref][endpoint] != op["actor"]:
                raise ContractError("operation/DDS endpoint mismatch: " + name)
    for name, flow in spec["flows"].items():
        kind = flow["kind"]
        for key in ("topicRef", "commandTopicRef", "ackTopicRef", "statusTopicRef",
                    "executionStatusTopicRef", "metadataTopicRef", "elementTopicRef",
                    "generalTopicRef"):
            if key in flow and flow[key] not in topics:
                raise ContractError("flow references unknown Topic: " + name + "." + key)
        for key in ("queryOperation", "reportOperation", "cancelOperation", "updateOperation"):
            if key in flow and flow[key] not in ops:
                raise ContractError("flow references unknown Operation: " + name + "." + key)
        for ref in flow.get("specializationTopicRefs", []):
            if ref not in topics:
                raise ContractError("flow references unknown specialization Topic")
        if kind == "request-reply-without-query-data":
            needed = ("queryOperation", "reportOperation", "topicRef", "queryUsesDdsTopic")
            if not all(key in flow for key in needed) or flow["queryUsesDdsTopic"] is not False:
                raise ContractError("UMAA query-less request MUST be DDS subscription")
            query = ops[flow["queryOperation"]]
            report = ops[flow["reportOperation"]]
            if (query["transport"], report["transport"]) != ("subscribe", "publish"):
                raise ContractError("Query/Report are not subscription + publication")
            if query["topicRef"] != flow["topicRef"] or report["topicRef"] != flow["topicRef"]:
                raise ContractError("Query and Report Topic mismatch: " + name)
    for table in spec["documentation"].get("messageTables", []):
        if table["topicRef"] not in topics:
            raise ContractError("ICD table references an unknown Topic")
    for topic in topics.values():
        # Documentations are not verified IDL declarations. Do not silently
        # infer Topic names, keys or QoS, even when the IDL type name looks valid.
        if topic["ddsTopicName"] is not None and topic["ddsTopicName"].strip() == "":
            raise ContractError("empty resolved DDS Topic name")
    # A signed-off service is never assumed by a YAML boolean. These fields
    # are descriptive only; native runtime approval requires independent evidence.


def validate_file(path: Path) -> dict:
    if path.is_symlink():
        raise ContractError("symbolic link to ServiceICD YAML is not permitted")
    path = path.resolve()
    if not path.is_file() or path.stat().st_size > 262144:
        raise ContractError("missing, unsafe, or oversized ServiceICD YAML")
    try:
        data = yaml.load(path.read_text(encoding="utf-8"), Loader=StrictLoader)
    except (yaml.YAMLError, UnicodeError) as exc:
        raise ContractError("invalid ICD YAML: " + str(exc)) from exc
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    try:
        jsonschema.Draft202012Validator(schema).validate(data)
    except jsonschema.ValidationError as exc:
        raise ContractError("ICD schema validation failed: " + exc.message) from exc
    _validate_imports(data)
    _validate_references(data)
    return data


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate human-readable UMAA ICD YAML (design draft)")
    parser.add_argument("file", type=Path)
    parser.add_argument("--json", action="store_true", help="print parsed contract (not authorized for DDS generation)")
    opts = parser.parse_args()
    try:
        contract = validate_file(opts.file)
    except (ContractError, OSError) as exc:
        print("INVALID: " + str(exc), file=sys.stderr)
        return 2
    if opts.json:
        print(json.dumps(contract, indent=2, sort_keys=True, ensure_ascii=False))
    else:
        print(f"STRUCTURAL_VALID_NOT_WIRE_APPROVED: {contract['metadata']['namespace']}")
        print(f"  Operations: {len(contract['spec']['operations'])} | Topics: {len(contract['spec']['topics'])}")
        print("  Original normative IDL/Key/QoS and real DDS remain independent gates.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
