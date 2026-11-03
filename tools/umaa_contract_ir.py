#!/usr/bin/env python3
"""Source-aware, fail-closed YAML ICD + restricted IDL -> vendor-neutral Contract IR.

This tool deliberately does NOT validate arbitrary official OMG IDL or DDS
TypeSupport. Its IDL parser supports the conservative SDK fixture subset.
It never infers a DDS Topic name, Key, QoS or runtime conformance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

from tools.validate_icd_yaml import (
    ICDYamlError, require_dependencies, strict_load,
    validate_document_elements, validate_topic_references,
)

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "schemas/umaa-icd-v1alpha1.schema.json"
SERVICE_ID = re.compile(r"^[A-Za-z_]\w*(?:::[A-Za-z_]\w*){2,5}$")
SUPPORTED_ROLES = {"report", "command", "ack", "status"}
SUPPORTED_KINDS = {"request-reply", "command-response"}


class ContractIRError(ValueError):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_service(path: Path) -> tuple[dict, str]:
    """Read only an existing regular YAML file; validate shape and references.

    Unlike the canonical reviewed-catalog validator, this accepts separately
    authored service documents. It never mutates generated catalog YAML.
    """
    yaml, jsonschema = require_dependencies()
    if not path.is_file() or path.is_symlink() or path.stat().st_size > 2_000_000:
        raise ContractIRError("service YAML missing, symlinked, or oversized")
    doc = strict_load(path, yaml)
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    jsonschema.Draft202012Validator.check_schema(schema)
    errors = list(jsonschema.Draft202012Validator(schema).iter_errors(doc))
    if errors:
        raise ContractIRError("ICD schema invalid: " + str(errors[0]))
    if doc.get("kind") != "Service":
        raise ContractIRError("expected a Service YAML resource")
    validate_topic_references(doc)
    validate_document_elements(doc)
    name = doc["metadata"]["id"]
    if not SERVICE_ID.fullmatch(name):
        raise ContractIRError("unsafe or malformed Service id")
    return doc, sha256(path.read_bytes())


def resolve(yaml_path: Path, idl_path: Path | None = None,
            *, fixture: bool = False, original_icd: Path | None = None) -> dict:
    """Create deterministic IR and explicit blockers for executable codegen.

    The ready flag means *only* vendor-neutral component scaffold readiness.
    Native official TypeSupport/Key/QoS and physical DDS interoperability
    require separate backend/official-source verification.
    """
    doc, yaml_digest = load_service(yaml_path)
    name = doc["metadata"]["id"]
    topics = doc["spec"]["topics"]
    protocols = doc["spec"]["protocols"]
    blockers: list[str] = []
    is_fixture = fixture and name.startswith("Demo::")
    if fixture and not is_fixture:
        blockers.append("fixture mode is restricted to Demo:: service IDs")
    if name.startswith("Demo::") and not fixture:
        blockers.append("Demo:: input requires explicit fixture mode")

    idl_types = {}
    idl_digest = None
    if idl_path is None:
        blockers.append("IDL source was not supplied")
    elif idl_path.is_symlink() or not idl_path.is_file() or idl_path.stat().st_size > 4_000_000:
        blockers.append("IDL source missing, symlinked, or oversized")
    else:
        data = idl_path.read_bytes()
        idl_digest = sha256(data)
        if fixture:
            # Compatibility only: the legacy host-only Demo:: scaffold.
            # Official/native ICD paths NEVER interpret IDL with regex.
            from sdk.tools.umaa_codegen import SchemaError, parse_idl
            try:
                idl_types = parse_idl(data.decode("utf-8"))
            except (UnicodeError, SchemaError) as exc:
                blockers.append("IDL not supported by restricted fixture parser: " + str(exc))
        else:
            blockers.append("official ICD must use native DDS compiler plugin path")

    pinned_idl = doc["sources"]["idl"]["bundleSha256"]
    if pinned_idl is None:
        blockers.append("IDL SHA-256 source pin is absent")
    elif idl_digest is not None and pinned_idl != idl_digest:
        blockers.append("IDL SHA-256 does not match input (single-file subset mode)")

    source = doc["sources"]["icd"]
    if not is_fixture:
        if source["originalSha256"] is None:
            blockers.append("authoritative ICD SHA-256 source pin is absent")
        elif original_icd is None:
            blockers.append("pinned original ICD file was not supplied for digest verification")
        elif (original_icd.is_symlink() or not original_icd.is_file()
              or sha256(original_icd.read_bytes()) != source["originalSha256"]):
            blockers.append("original ICD SHA-256 does not match input")
        if source["reference"]["kind"] != "official":
            blockers.append("ICD source is a secondary transcription, not official")
        checks = doc["verification"]
        if not checks["officialIdlVerified"]:
            blockers.append("official IDL binding not reviewed")
        if not checks["ddsQosVerified"]:
            blockers.append("mandatory DDS QoS not reviewed")

    selected = {}
    seen_topic_names: set[str] = set()
    for alias, topic in sorted(topics.items()):
        role = topic["role"]
        if role not in SUPPORTED_ROLES:
            blockers.append(alias + ": unsupported DDS role " + role)
        dtype = topic["idlType"]
        fields = idl_types.get(dtype)
        if idl_types and fields is None:
            blockers.append(alias + ": IDL type not found: " + dtype)
        if fields is not None and any(f.type_name not in {"long", "string"} for f in fields):
            blockers.append(alias + ": restricted generator supports IDL long/string only")
        keys = topic["keyFields"]
        if not isinstance(keys, list):
            blockers.append(alias + ": Key fields unresolved")
        elif fields is not None:
            declared = [f.name for f in fields if f.is_key]
            if keys != declared:
                blockers.append(alias + ": Key fields differ from IDL @key declarations")
        if topic["ddsTopicName"] is None:
            blockers.append(alias + ": DDS Topic name unresolved")
        elif topic["ddsTopicName"] in seen_topic_names:
            blockers.append(alias + ": duplicate DDS Topic name in service")
        else:
            seen_topic_names.add(topic["ddsTopicName"])
        if not topic["qosProfileRef"]:
            blockers.append(alias + ": QoS profile unresolved")
        selected[alias] = {
            "idlType": dtype,
            "role": role,
            "direction": topic["direction"],
            "ddsTopicName": topic["ddsTopicName"],
            "keyFields": keys,
            "qosProfileRef": topic["qosProfileRef"],
            "fields": ([{"name": f.name, "type": f.type_name, "key": f.is_key}
                        for f in fields] if fields is not None else None),
        }

    # The first working generator slice handles one Report or Command protocol.
    if len(protocols) != 1:
        blockers.append("generator requires exactly one protocol")
    elif protocols[0]["kind"] not in SUPPORTED_KINDS:
        blockers.append("protocol kind not supported")
    else:
        protocol = protocols[0]
        binds = protocol["bindings"]
        if protocol["kind"] == "request-reply":
            if protocol.get("mode") != "without-query-data":
                blockers.append("request/reply with query data is not implemented")
            report_aliases = binds.get("reports")
            if (not isinstance(report_aliases, list) or len(report_aliases) != 1
                or selected.get(report_aliases[0], {}).get("role") != "report"
                or selected.get(report_aliases[0], {}).get("direction") != "provider-to-consumer"):
                blockers.append("expected one provider-to-consumer Report binding")
            if set(binds) != {"reports"}:
                blockers.append("unknown request/reply protocol bindings")
        else:
            if set(binds) != {"command", "ack", "status"}:
                blockers.append("Command requires exactly command/ack/status")
            for key, role, direction in (
                ("command", "command", "consumer-to-provider"),
                ("ack", "ack", "provider-to-consumer"),
                ("status", "status", "provider-to-consumer"),
            ):
                entry = selected.get(binds.get(key), {}) if isinstance(binds.get(key), str) else {}
                if entry.get("role") != role or entry.get("direction") != direction:
                    blockers.append("invalid Command " + key + " binding/direction")

    for operation, record in sorted(doc["spec"]["operations"].items()):
        if (record["kind"] == "unresolved" or record["topicRef"] is None
                or record["sourceClause"] is None):
            blockers.append("Operation " + operation + " is unresolved")
        elif record["sourceClause"] not in source["clauses"]:
            blockers.append("Operation " + operation + " lacks reviewed ICD clause")
    if doc["spec"]["collections"] or doc["spec"]["specializations"] or doc["spec"]["unresolvedCollectionTypes"]:
        blockers.append("collection/specialization semantics not implemented in generator")

    blockers = sorted(set(blockers))
    return {
        "irVersion": 1,
        "serviceId": name,
        "scope": "synthetic-fixture" if is_fixture else "official-source-required",
        "inputs": {"yamlSha256": yaml_digest, "idlSha256": idl_digest,
                   "pinnedIdlSha256": pinned_idl,
                   "originalIcdSha256": source["originalSha256"]},
        "topics": selected,
        "operations": doc["spec"]["operations"],
        "protocols": protocols,
        "readiness": {"componentScaffold": not blockers,
                      "nativeDdsBinding": False,
                      "blockers": blockers},
    }


def resolve_native(yaml_path: Path, *, type_sources: dict[str, str],
                   generated_sources: dict[str, dict],
                   fixture: bool = False) -> dict:
    """Vendor-neutral ICD Contract IR from ACTUAL native TypeSupport evidence.

    No Python IDL parser is imported or run here. This IR is suitable for
    source-pinned build/codegen metadata but not an approved service runtime
    binding: DDS compiler success cannot prove ICD normative QoS, @key
    semantics or host payload conversions.
    """
    doc, yaml_digest = load_service(yaml_path)
    service = doc["metadata"]["id"]
    if fixture != service.startswith("Demo::"):
        raise ContractIRError("invalid fixture mode for native ICD Contract IR")
    topics = {}
    for alias, binding in sorted(doc["spec"]["topics"].items()):
        name = binding["idlType"]
        source = type_sources.get(name)
        if not source or source not in generated_sources:
            raise ContractIRError("native TypeSupport source not compiled for " + name)
        artifact = generated_sources[source]
        if (artifact.get("format") != "UMAA-native-IDL-generator-v2" or
            not artifact.get("nativeTypeSupportGenerated") or
            artifact.get("source") != source or not artifact.get("backends") or
            not artifact.get("sourceDependencies")):
            raise ContractIRError("invalid native compiler evidence for " + name)
        if not binding["ddsTopicName"] or not binding["qosProfileRef"]:
            raise ContractIRError("unresolved native DDS Topic or QoS")
        if not isinstance(binding["keyFields"], list):
            raise ContractIRError("unresolved native Key fields")
        topics[alias] = {
            "idlType": name, "sourceFile": source,
            "ddsTopicName": binding["ddsTopicName"], "role": binding["role"],
            "direction": binding["direction"],
            "qosProfileRef": binding["qosProfileRef"],
            "declaredKeyFields": binding["keyFields"],
            "nativeCompilerBackends": artifact["backends"],
            "nativeTypeSourceSha256": artifact["inputSha256"],
            "typeExistenceProbeGenerated": True,
            "keyMemberProbeGenerated": True,
            "keyAnnotationVerified": False,
            "normativeQosVerifiedByCompiler": False,
        }
    return {
        "irVersion": 2,
        "mode": "native-compiler-evidence",
        "serviceId": service,
        "scope": "synthetic-fixture" if fixture else "official-source-reviewed",
        "yamlSha256": yaml_digest,
        "declaredIdlBundleSha256": doc["sources"]["idl"]["bundleSha256"],
        "topics": topics,
        "protocols": doc["spec"]["protocols"],
        "collections": doc["spec"]["collections"],
        "specializations": doc["spec"]["specializations"],
        "operations": doc["spec"]["operations"],
        "readiness": {
            "nativeTypeSupportGenerated": True,
            "nativeTypeProbesCompiled": False,
            "nativeDdsBinding": False,
            "componentRuntimeBound": False,
            "officialConformanceClaim": False,
            "blockers": [
                "Key annotation and mandatory DDS QoS are not verified by compiler success",
                "Vendor TypeSupport to SDK wire codec and native DDS adapter not generated",
                "Runtime Provider/Consumer registration requires reviewed protocol mapping",
            ],
        },
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Inspect source-aware YAML+IDL Contract IR")
    parser.add_argument("--yaml", type=Path, required=True)
    parser.add_argument("--idl", type=Path)
    parser.add_argument("--original-icd", type=Path)
    parser.add_argument("--fixture", action="store_true",
                        help="synthetic Demo:: inputs only; never official UMAA runtime proof")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)
    try:
        ir = resolve(args.yaml, args.idl, fixture=args.fixture, original_icd=args.original_icd)
        result = json.dumps(ir, indent=2, sort_keys=True) + "\n"
        if args.out:
            if args.out.is_symlink():
                raise ContractIRError("refusing symlink output")
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(result, encoding="utf-8")
        else:
            print(result, end="")
        return 0 if ir["readiness"]["componentScaffold"] else 3
    except (OSError, ICDYamlError, ContractIRError, ValueError) as exc:
        print("CONTRACT_IR_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
