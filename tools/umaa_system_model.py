#!/usr/bin/env python3
"""Read-only System Model validator shared by CLI and future Studio.

Connects independently pinned Component Manifests to the reviewed UMAA
catalog's Service/Topic *identities* and declared DDS QoS pairs. It never
creates DDS entities, fabricates missing official Topic names/keys/QoS,
or asserts runtime deployment / normative compliance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

from tools import umaa_service_contracts as contracts

NAME = re.compile(r"[a-z][a-z0-9_]{0,62}\Z")
SERVICE = re.compile(r"[A-Za-z][A-Za-z0-9_]*(?:::[A-Za-z][A-Za-z0-9_]*){2,5}\Z")
SHA = re.compile(r"[0-9a-f]{64}\Z")
PATH = re.compile(r"[A-Za-z0-9_./-]{1,320}\Z")
ALLOWED_BACKENDS = {"cyclone", "fastdds"}
QOS_FIELDS = {"reliability", "durability", "history", "depth", "presentation",
              "orderedAccess", "coherentAccess", "partition"}


class SystemModelError(ValueError):
    """Untrusted topology input or an unproven inter-model reference."""


def _fields(obj, required, optional=(), *, where="object"):
    if not isinstance(obj, dict) or not set(required) <= set(obj) or set(obj) - set(required) - set(optional):
        raise SystemModelError(where + ": missing or unknown fields")


def _name(value, where, pattern=NAME):
    if not isinstance(value, str) or not pattern.fullmatch(value):
        raise SystemModelError(where + ": invalid identifier")
    return value


def _int(value, where, minimum, maximum):
    if type(value) is not int or not minimum <= value <= maximum:
        raise SystemModelError(where + ": invalid integer")
    return value


def _unique_object(pairs):
    result = {}
    for key, val in pairs:
        if key in result:
            raise SystemModelError("duplicate JSON field: " + key)
        result[key] = val
    return result


def _json_file(path: Path, *, limit: int):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > limit:
        raise SystemModelError("model/manifest missing, symlinked or oversized")
    data = path.read_bytes()
    try:
        doc = json.loads(data.decode("utf-8"), object_pairs_hook=_unique_object,
                         parse_constant=lambda _: (_ for _ in ()).throw(
                             SystemModelError("non-finite JSON number")))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise SystemModelError("invalid JSON") from exc
    return doc, hashlib.sha256(data).hexdigest()


def _manifest_file(root: Path, rel: str):
    if (not isinstance(rel, str) or not PATH.fullmatch(rel) or
            rel.startswith("/")):
        raise SystemModelError("unsafe Component Manifest path")
    parts = rel.split("/")
    if any(p in {"", ".", ".."} for p in parts) or parts[-1] != "umaa.component.json":
        raise SystemModelError("Component Manifest must be a relative umaa.component.json")
    result = root
    for part in parts:
        result = result / part
        if result.is_symlink():
            raise SystemModelError("symlinked Component Manifest ancestor")
    if not result.resolve().is_relative_to(root.resolve()):
        raise SystemModelError("Component Manifest escapes System Model root")
    return result


def _qos(profile, where):
    _fields(profile, QOS_FIELDS, where=where)
    for key, allowed in {
        "reliability": {"reliable", "best_effort"},
        "durability": {"volatile", "transient_local"},
        "history": {"keep_last", "keep_all"},
        "presentation": {"instance", "topic", "group"},
    }.items():
        if not isinstance(profile[key], str) or profile[key] not in allowed:
            raise SystemModelError(where + ": invalid " + key)
    depth = _int(profile["depth"], where + ".depth", 0, 4096)
    if (profile["history"] == "keep_last" and depth == 0) or (
            profile["history"] == "keep_all" and depth != 0):
        raise SystemModelError(where + ": history depth mismatch")
    for field in ("orderedAccess", "coherentAccess"):
        if type(profile[field]) is not bool:
            raise SystemModelError(where + ": invalid " + field)
    if (profile["orderedAccess"] or profile["coherentAccess"]) and profile["presentation"] == "instance":
        raise SystemModelError(where + ": ordered/coherent requires TOPIC or GROUP")
    partition = profile["partition"]
    if not isinstance(partition, str):
        raise SystemModelError(where + ": invalid partition")
    try:
        encoded = partition.encode("utf-8")
    except UnicodeError as exc:
        raise SystemModelError(where + ": invalid Unicode partition") from exc
    if len(encoded) > 255 or any(c in partition for c in ("\0", "\r", "\n")):
        raise SystemModelError(where + ": invalid partition")
    return profile


def _compatible(writer, reader):
    """Conservative subset of local SDK QoS checks, NOT vendor DDS matching."""
    if reader["reliability"] == "reliable" and writer["reliability"] != "reliable":
        raise SystemModelError("QoS incompatible: reader RELIABLE / writer BEST_EFFORT")
    if reader["durability"] == "transient_local" and writer["durability"] != "transient_local":
        raise SystemModelError("QoS incompatible: reader durability unavailable")
    if writer["partition"] != reader["partition"]:
        raise SystemModelError("QoS incompatible: partition mismatch")
    if reader["orderedAccess"] and not writer["orderedAccess"]:
        raise SystemModelError("QoS incompatible: ordered access mismatch")
    if reader["coherentAccess"] and not writer["coherentAccess"]:
        raise SystemModelError("QoS incompatible: coherent access mismatch")
    # Deliberately conservative for a DESIGN validator; DDS policy matching
    # and vendor-specific effective QoS still require independent wire tests.
    if writer["presentation"] != reader["presentation"]:
        raise SystemModelError("QoS model mismatch: presentation scope differs")


def validate(doc, model_dir: Path) -> dict:
    """Validate topology without writing or launching any Component/DDS code."""
    from tools import umaa_component_cli as component

    _fields(doc, ("schemaVersion", "system", "components", "connections", "dds"),
            ("catalogPin",), where="System Model")
    if doc["schemaVersion"] != "1":
        raise SystemModelError("unsupported System Model version")
    _fields(doc["system"], ("id",), where="system")
    system_id = _name(doc["system"]["id"], "system.id")

    dds = doc["dds"]
    _fields(dds, ("domains",), where="dds")
    if not isinstance(dds["domains"], list) or not 1 <= len(dds["domains"]) <= 16:
        raise SystemModelError("expected 1..16 DDS domains")
    domains = {}
    for domain in dds["domains"]:
        _fields(domain, ("id", "backend", "qosProfiles"), where="DDS domain")
        key = _int(domain["id"], "domain.id", 0, 230)
        if key in domains:
            raise SystemModelError("duplicate DDS domain")
        if not isinstance(domain["backend"], str) or domain["backend"] not in ALLOWED_BACKENDS:
            raise SystemModelError("unsupported DDS backend (no RTI/mixed-network proof)")
        profiles = domain["qosProfiles"]
        if not isinstance(profiles, dict) or len(profiles) > 64:
            raise SystemModelError("invalid DDS QoS profile collection")
        for name, profile in profiles.items():
            _name(name, "QoS profile name")
            _qos(profile, "QoS " + name)
        domains[key] = domain

    members = doc["components"]
    if not isinstance(members, list) or not 1 <= len(members) <= 64:
        raise SystemModelError("expected 1..64 Components")
    components = {}
    paths = set()
    official_used = set()
    for entry in members:
        _fields(entry, ("id", "manifest", "sha256", "domainId"), where="Component")
        name = _name(entry["id"], "component.id")
        if name in components:
            raise SystemModelError("duplicate Component id")
        domain_id = _int(entry["domainId"], "component.domainId", 0, 230)
        if domain_id not in domains:
            raise SystemModelError("Component refers to unknown DDS domain")
        digest = _name(entry["sha256"], "Component SHA-256", SHA)
        file = _manifest_file(model_dir, entry["manifest"])
        if entry["manifest"] in paths:
            raise SystemModelError("same Component Manifest referenced twice")
        paths.add(entry["manifest"])
        manifest, observed = _json_file(file, limit=256 * 1024)
        if observed != digest:
            raise SystemModelError("Component Manifest digest mismatch")
        try:
            component.validate(manifest, nonempty=True)
            gate = component.check_assembly(manifest)
        except (component.ProjectError, contracts.ContractError) as exc:
            raise SystemModelError("invalid referenced Component: " + str(exc)) from exc
        if manifest["component"]["id"] != name:
            raise SystemModelError("Component id disagrees with pinned Manifest")
        members_roles = {(s["id"], s["role"]) for s in manifest["services"]}
        official_used.update(s for s, _ in members_roles if s not in component.DEMO)
        components[name] = {"domainId": domain_id, "roles": members_roles,
                            "codegenReady": gate["codegenReady"]}

    catalog_pin = doc.get("catalogPin")
    catalog = {}
    if official_used or catalog_pin is not None:
        _fields(catalog_pin, ("kind", "sha256"), where="System catalogPin")
        if catalog_pin["kind"] != "reviewed-icd-catalog":
            raise SystemModelError("unknown catalog reference kind")
        _, digest = contracts.catalog_names()
        if _name(catalog_pin["sha256"], "catalog SHA-256", SHA) != digest:
            raise SystemModelError("System references stale reviewed ICD catalog")
        canonical, _ = contracts._read_catalog()
        catalog = {x["name"]: x for x in canonical["services"]}
        if not official_used <= set(catalog):
            raise SystemModelError("unknown official Service in System")
    # Preserve explicit source metadata rather than manufacturing official
    # Key/QoS/Operation references from the secondary catalog.
    links = doc["connections"]
    if not isinstance(links, list) or not 1 <= len(links) <= 256:
        raise SystemModelError("expected 1..256 Service connections")
    link_keys = set()
    channels_out = []
    for link in links:
        _fields(link, ("service", "consumer", "provider", "domainId", "channels"),
                where="Service connection")
        service = _name(link["service"], "connection.service", SERVICE)
        consumer_id = _name(link["consumer"], "consumer")
        provider_id = _name(link["provider"], "provider")
        if consumer_id == provider_id or consumer_id not in components or provider_id not in components:
            raise SystemModelError("connection must link two known distinct Components")
        domain_id = _int(link["domainId"], "connection.domainId", 0, 230)
        if domain_id not in domains or any(components[x]["domainId"] != domain_id
                                         for x in (consumer_id, provider_id)):
            raise SystemModelError("cross-domain Component connection unsupported")
        if (service, "consumer") not in components[consumer_id]["roles"] or (
                service, "provider") not in components[provider_id]["roles"]:
            raise SystemModelError("connection has no matching Provider/Consumer Service roles")
        key = (service, consumer_id, provider_id)
        if key in link_keys:
            raise SystemModelError("duplicate Service connection")
        link_keys.add(key)
        declared = link["channels"]
        if not isinstance(declared, list) or len(declared) > 128:
            raise SystemModelError("invalid Topic channel list")
        if service in catalog:
            record = catalog[service]
            topics = {}
            for side, owner in (("provider_topics", "provider"),
                                ("consumer_topics", "consumer")):
                for topic in record[side]:
                    if topic["name"] in topics:
                        raise SystemModelError("ambiguous catalog Topic identity")
                    topics[topic["name"]] = (topic, owner)
            if len(declared) != len(topics):
                raise SystemModelError("official Service requires every catalog Topic channel")
        elif service in component.DEMO:
            # Demo fixture TypeBindings are NOT the official catalog and cannot
            # be manufactured from service names by this system-model gate.
            if declared:
                raise SystemModelError("synthetic Service Topic list is not source reviewed")
            topics = {}
        else:
            raise SystemModelError("connection references unknown Service")
        seen = set()
        for ch in declared:
            _fields(ch, ("topicId", "writerProfile", "readerProfile"), where="Topic channel")
            topic_id = _name(ch["topicId"], "topic ID",
                             re.compile(r"[A-Za-z][A-Za-z0-9_]*(?:::[A-Za-z][A-Za-z0-9_]*){2,7}\Z"))
            if topic_id not in topics or topic_id in seen:
                raise SystemModelError("unknown or duplicate Topic identity in connection")
            seen.add(topic_id)
            writer_profile = _name(ch["writerProfile"], "writerProfile")
            reader_profile = _name(ch["readerProfile"], "readerProfile")
            profiles = domains[domain_id]["qosProfiles"]
            if writer_profile not in profiles or reader_profile not in profiles:
                raise SystemModelError("unknown explicit DDS QoS profile")
            _compatible(profiles[writer_profile], profiles[reader_profile])
            topic, owner = topics[topic_id]
            writer = provider_id if owner == "provider" else consumer_id
            reader = consumer_id if owner == "provider" else provider_id
            channels_out.append({
                "service": service, "topicId": topic_id,
                "idlType": topic["type"], "catalogRole": topic["role"],
                "writerComponent": writer, "readerComponent": reader,
                "domainId": domain_id, "writerProfile": writer_profile,
                "readerProfile": reader_profile,
            })
        if set(topics) != seen:
            raise SystemModelError("incomplete source-catalog Topic coverage")

    return {
        "status": "VALID_SYSTEM_DESIGN_NOT_DDS_DEPLOYMENT",
        "systemId": system_id, "components": len(components),
        "connections": len(links), "catalogTopicsMapped": len(channels_out),
        "channels": channels_out, "domains": len(domains),
        "componentCodegenReady": all(c["codegenReady"] for c in components.values()),
        "declaredQosPairChecksPassed": True,
        "officialSourceAndOperationMappingComplete": False,
        "officialQosVerified": False, "ddsDeploymentReady": False,
        "officialRuntimeClaim": False, "officialConformanceClaim": False,
    }


def validate_file(path: Path) -> dict:
    model, _ = _json_file(path, limit=512 * 1024)
    return validate(model, path.resolve().parent)


def main(argv=None):
    parser = argparse.ArgumentParser(description="Validate non-deployable UMAA System Model")
    parser.add_argument("command", choices=["validate"])
    parser.add_argument("--input", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        result = validate_file(args.input)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (SystemModelError, contracts.ContractError, OSError) as exc:
        print("UMAA SYSTEM MODEL ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
