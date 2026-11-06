#!/usr/bin/env python3
"""Studio/CLI read-only DDS endpoint planner for minimal Component composition.

The user selects Service roles, explicit links and a project deployment context.
The planner derives *logical* DDS Writer/Reader direction from the reviewed
partial UMAA Catalog, never synthesizing unverified official Topic names,
Key fields, mandatory QoS or executable vendor endpoints.

The existing System Model v1 explicit DDS/QoS validator remains unchanged.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

from tools import umaa_component_cli as component
from tools import umaa_service_contracts as contracts
from tools import umaa_system_model as common


KIND = "umaa-system-composition-intent"
VERSION = "1"


def _blocking(code: str, service: str, reason: str) -> dict:
    return {"code": code, "service": service, "reason": reason}


def plan(model: dict, root: Path) -> dict:
    """Return deterministic model/topology preview; never touch DDS or write files."""
    common._fields(
        model, ("kind", "schemaVersion", "system", "deployment",
                "components", "connections"), ("catalogPin",),
        where="System composition intent")
    if model["kind"] != KIND or model["schemaVersion"] != VERSION:
        raise common.SystemModelError("unsupported System composition kind/version")
    common._fields(model["system"], ("id",), where="System")
    system_id = common._name(model["system"]["id"], "system.id")
    common._fields(model["deployment"], ("backend", "domainId"),
                   where="deployment")
    backend = model["deployment"]["backend"]
    if not isinstance(backend, str) or backend not in common.ALLOWED_BACKENDS:
        raise common.SystemModelError("unsupported standalone DDS backend")
    domain_id = common._int(model["deployment"]["domainId"],
                            "deployment.domainId", 0, 230)

    requested = model["components"]
    if not isinstance(requested, list) or not 1 <= len(requested) <= 64:
        raise common.SystemModelError("expected 1..64 Component references")
    components: dict[str, dict] = {}
    file_paths: set[Path] = set()
    used_official: set[str] = set()
    preview_members = []
    for item in requested:
        common._fields(item, ("id", "manifest", "sha256"), where="Component")
        name = common._name(item["id"], "Component.id")
        if name in components:
            raise common.SystemModelError("duplicate Component id")
        digest = common._name(item["sha256"], "Component SHA", common.SHA)
        manifest_path = common._manifest_file(root, item["manifest"])
        # Normalize aliases without resolving a symbolic link: _manifest_file
        # already rejects symlinked ancestors and paths escaping the root.
        if manifest_path in file_paths:
            raise common.SystemModelError("duplicate Component Manifest reference")
        file_paths.add(manifest_path)
        manifest, observed = common._json_file(manifest_path, limit=256 * 1024)
        if observed != digest:
            raise common.SystemModelError("Component Manifest digest mismatch")
        try:
            component.validate(manifest, nonempty=True)
            assessment = component.check_assembly(manifest)
        except (component.ProjectError, contracts.ContractError) as exc:
            raise common.SystemModelError(
                "referenced Component violates existing assembly gate: " + str(exc)) from exc
        if manifest["component"]["id"] != name:
            raise common.SystemModelError("Component ID differs from pinned Manifest")
        roles = {(service["id"], service["role"])
                 for service in manifest["services"]}
        used_official.update(
            service for service, _ in roles if service not in component.DEMO)
        components[name] = {"roles": roles,
                            "codegenReady": assessment["codegenReady"]}
        preview_members.append({
            "id": name,
            "manifest": item["manifest"],
            "sha256": observed,
            "services": sorted(
                [{"id": service, "role": role} for service, role in roles],
                key=lambda row: (row["id"], row["role"])),
            "codegenReady": assessment["codegenReady"],
        })

    reviewed: dict[str, dict] = {}
    pin = model.get("catalogPin")
    if used_official or pin is not None:
        common._fields(pin, ("kind", "sha256"), where="catalogPin")
        if pin["kind"] != "reviewed-icd-catalog":
            raise common.SystemModelError("unknown Catalog pin kind")
        pinned_digest = common._name(pin["sha256"], "Catalog digest", common.SHA)
        catalog, actual_digest = contracts._read_catalog()
        if pinned_digest != actual_digest:
            raise common.SystemModelError("stale reviewed Catalog digest")
        reviewed = {x["name"]: x for x in catalog["services"]}
        if not used_official <= set(reviewed):
            raise common.SystemModelError("unknown official Service in Component")

    wires = model["connections"]
    if not isinstance(wires, list) or len(wires) > 256:
        raise common.SystemModelError("expected at most 256 Service links")
    seen: set[tuple[str, str, str]] = set()
    connected_consumers: set[tuple[str, str]] = set()
    endpoints = []
    preview_links = []
    blockers: list[dict] = []
    for link in wires:
        common._fields(link, ("service", "consumer", "provider"),
                       where="Service link")
        service = common._name(link["service"], "Service ID", common.SERVICE)
        consumer = common._name(link["consumer"], "Consumer ID")
        provider = common._name(link["provider"], "Provider ID")
        if consumer == provider or consumer not in components or provider not in components:
            raise common.SystemModelError("connection requires distinct known Components")
        if ((service, "consumer") not in components[consumer]["roles"] or
                (service, "provider") not in components[provider]["roles"]):
            raise common.SystemModelError("connection lacks declared Consumer/Provider role")
        key = (service, consumer, provider)
        if key in seen:
            raise common.SystemModelError("duplicate Service link")
        seen.add(key)
        connected_consumers.add((consumer, service))
        preview_links.append({"service": service,
                              "consumer": consumer, "provider": provider})
        if service in reviewed:
            record = reviewed[service]
            selected = []
            names = set()
            for side, writer_role in (
                    ("consumer_topics", "consumer"),
                    ("provider_topics", "provider")):
                for topic in record[side]:
                    topic_id = topic["name"]
                    if topic_id in names:
                        raise common.SystemModelError("ambiguous reviewed Catalog Topic")
                    names.add(topic_id)
                    writer = consumer if writer_role == "consumer" else provider
                    reader = provider if writer_role == "consumer" else consumer
                    selected.append({
                        "service": service,
                        "topicId": topic_id,
                        "idlType": topic["type"],
                        "role": topic["role"],
                        "writerComponent": writer,
                        "readerComponent": reader,
                        "domainId": domain_id,
                        "backend": backend,
                        # These are deliberately unresolved, not DDS defaults.
                        "ddsTopicName": None,
                        "keyFields": None,
                        "writerQosProfile": None,
                        "readerQosProfile": None,
                        "status": "SOURCE_BINDING_REQUIRED",
                    })
            endpoints.extend(sorted(selected, key=lambda item: item["topicId"]))
            evidence = record
            if not evidence["official_idl_verified"]:
                blockers.append(_blocking(
                    "OFFICIAL_NATIVE_BINDING_UNVERIFIED", service,
                    "Reviewed catalog does not approve original native TypeSupport/Key binding"))
            if not evidence["dds_qos_verified"]:
                blockers.append(_blocking(
                    "MANDATORY_DDS_QOS_UNVERIFIED", service,
                    "No original ICD-approved effective DDS QoS profile mapping"))
            if not evidence["dds_behavior_verified"]:
                blockers.append(_blocking(
                    "OFFICIAL_PROTOCOL_WIRE_UNVERIFIED", service,
                    "Service-specific original Command/Report DDS behavior not approved"))
            blockers.append(_blocking(
                "TOPIC_NAME_KEY_AND_OPERATION_MAPPING_UNRESOLVED", service,
                "Partial Catalog has literal type/role IDs but not authoritative DDS Topic name, "
                "key annotations or individual Operation-to-Topic/Flow bindings"))
            if record.get("collection_topics"):
                blockers.append(_blocking(
                    "COLLECTION_TOPIC_COVERAGE_INCOMPLETE", service,
                    "Partial Catalog collection types have no reviewed directional DDS endpoint ownership"))
        elif service in component.DEMO:
            # Development synthetic fixture identities are not source-backed
            # DDS channels. Do not infer an official type from a Demo name.
            blockers.append(_blocking(
                "SYNTHETIC_DDS_TOPIC_BINDING_UNREVIEWED", service,
                "Host-only Demo Component roles lack a source-reviewed native Topic binding"))
        else:
            raise common.SystemModelError("connection refers to unknown Service")

    for comp_id, entry in sorted(components.items()):
        for service, role in sorted(entry["roles"]):
            if role == "consumer" and (comp_id, service) not in connected_consumers:
                blockers.append(_blocking(
                    "UNCONNECTED_CONSUMER_SERVICE", service,
                    "Component " + comp_id + " has no explicitly connected Service Provider"))

    # Pure metadata preview: no source-approved official DDS endpoint or QoS
    # policy is invented, even if the selected vendor/domain is valid.
    return {
        "status": "DESIGN_PREVIEW_NOT_DEPLOYABLE",
        "schemaVersion": VERSION,
        "systemId": system_id,
        "source": {
            "kind": KIND,
            "catalogPinned": pin is not None,
            "catalogSha256": pin["sha256"] if pin else None,
        },
        "deploymentSelection": {"backend": backend, "domainId": domain_id},
        "components": preview_members,
        "connections": preview_links,
        "endpoints": endpoints,
        "blockers": blockers,
        "endpointCount": len(endpoints),
        "componentCount": len(components),
        "connectionCount": len(preview_links),
        "topologyReferencesValid": True,
        "ddSNamesKeysAndMandatoryQosResolved": False,
        "ddsDeploymentReady": False,
        "officialRuntimeClaim": False,
        "officialConformanceClaim": False,
    }


def plan_file(path: Path) -> dict:
    doc, _ = common._json_file(path, limit=512 * 1024)
    return plan(doc, path.resolve().parent)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Preview logical DDS endpoints from simple Service role connections")
    parser.add_argument("command", choices=["plan"])
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        report = plan_file(args.input)
        print(json.dumps(report, sort_keys=True, indent=2))
        return 0
    except (common.SystemModelError, contracts.ContractError, OSError) as exc:
        print("UMAA SYSTEM PLANNER ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
