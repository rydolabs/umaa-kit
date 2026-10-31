#!/usr/bin/env python3
"""Strict six-ICD YAML schema, provenance and source-consistency validator.

Dependencies (explicit for the dedicated CI workflow):
  PyYAML==6.0.2, jsonschema==4.23.0

These generated mappings are NOT normative ICD prose or approved runtime DDS
TypeBindings. The validator cannot manufacture official key/QoS evidence.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from tools.generate_icd_yaml import DEFAULT_OUT, REPO, expected_files
from tools.umaa_service_contracts import CATALOG, _read_catalog


class ICDYamlError(ValueError):
    pass


def require_dependencies():
    try:
        import yaml
        import jsonschema
    except ImportError as exc:
        raise ICDYamlError("install PyYAML==6.0.2 and jsonschema==4.23.0") from exc
    return yaml, jsonschema


def strict_load(path: Path, yaml):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > 1_000_000:
        raise ICDYamlError("missing, symlinked or oversized YAML: " + str(path))

    class Loader(yaml.SafeLoader):
        def compose_node(self, parent, index):
            if self.check_event(yaml.AliasEvent):
                raise ICDYamlError("YAML aliases are forbidden: " + str(path))
            return super().compose_node(parent, index)

        def construct_mapping(self, node, deep=False):
            self.flatten_mapping(node)
            keys = set()
            for key_node, _ in node.value:
                key = self.construct_object(key_node, deep=deep)
                if not isinstance(key, str) or key in keys:
                    raise ICDYamlError("duplicate/non-string YAML key: " + str(path))
                keys.add(key)
            return super().construct_mapping(node, deep=deep)

    try:
        with path.open(encoding="utf-8") as stream:
            doc = yaml.load(stream, Loader=Loader)
    except (UnicodeError, yaml.YAMLError) as exc:
        raise ICDYamlError("unsafe or malformed YAML: " + str(path)) from exc
    if not isinstance(doc, dict):
        raise ICDYamlError("YAML root must be a mapping: " + str(path))
    return doc


def validate(root: Path = DEFAULT_OUT, catalog_path: Path = CATALOG,
             schema_path: Path | None = None) -> dict:
    yaml, jsonschema = require_dependencies()
    if root.is_symlink() or not root.is_dir():
        raise ICDYamlError("reviewed YAML output directory missing or symlinked")
    schema_path = schema_path or REPO / "schemas/umaa-icd-v1alpha1.schema.json"
    if schema_path.is_symlink() or not schema_path.is_file():
        raise ICDYamlError("canonical YAML contract schema missing")
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    jsonschema.Draft202012Validator.check_schema(schema)
    validator = jsonschema.Draft202012Validator(schema)
    catalog, digest = _read_catalog(catalog_path)
    expected = expected_files(catalog)
    actual = {p.relative_to(root).as_posix() for p in root.rglob("*.yaml")}
    if actual != set(expected):
        raise ICDYamlError("missing/extra mapped ICD YAML files: " +
                            repr(sorted(set(expected) ^ actual)[:16]))
    docs = {}
    for rel, original in expected.items():
        path = root / rel
        doc = strict_load(path, yaml)
        errors = sorted(validator.iter_errors(doc), key=lambda e: str(e.path))
        if errors:
            raise ICDYamlError(rel + ": schema error: " + str(errors[0]))
        # Compare the semantic object AND serialized text, so no undocumented
        # alias, source upgrade, inferred type/QoS, or hand-edit can be accepted.
        original_doc = yaml.safe_load(original)
        if doc != original_doc:
            raise ICDYamlError("reviewed catalog/YAML semantic drift: " + rel)
        if path.read_text(encoding="utf-8") != original:
            raise ICDYamlError("noncanonical generated YAML: " + rel)
        docs[rel] = doc

    by_domain = {d: set() for d in ("EO", "MO", "MM", "SA", "SEM", "SO")}
    for svc in catalog["services"]:
        _, domain, name = svc["name"].split("::")
        rel = f"services/{domain}/{name}.yaml"
        data = docs[rel]
        by_domain[domain].add(svc["name"])
        if data["metadata"]["id"] != svc["name"] or data["sources"]["icd"]["clauses"] != svc["clauses"]:
            raise ICDYamlError("service identity/clause drift: " + rel)
        assert data["sources"]["icd"]["originalSha256"] is None
        assert data["sources"]["idl"]["bundleSha256"] is None
        if data["verification"]["ddsQosVerified"] or data["verification"]["ddsBehaviorVerified"]:
            raise ICDYamlError("fabricated normative DDS evidence: " + rel)
        if any(x["ddsTopicName"] is not None or x["keyFields"] is not None or
               x["qosProfileRef"] is not None for x in data["spec"]["topics"].values()):
            raise ICDYamlError("fabricated DDS Topic/Key/QoS mapping: " + rel)
        if data["spec"]["collections"] or data["spec"]["specializations"]:
            raise ICDYamlError("unreviewed collections or specialization created: " + rel)
        if set(data["spec"]["operations"]) != set(svc["operations"]):
            raise ICDYamlError("operation omission: " + rel)
        if any(x["kind"] != "unresolved" for x in data["spec"]["operations"].values()):
            raise ICDYamlError("unreviewed operation binding was invented: " + rel)

    for domain, names in by_domain.items():
        doc = docs[f"icd/{domain}.yaml"]
        if set(doc["spec"]["services"]) != names:
            raise ICDYamlError("ICD service index mismatch: " + domain)

    count_topics = sum(len(s["provider_topics"]) + len(s["consumer_topics"])
                       for s in catalog["services"])
    count_ops = sum(len(s["operations"]) for s in catalog["services"])
    return {"icds": len(by_domain), "services": len(catalog["services"]),
            "topics": count_topics, "operations": count_ops,
            "catalogSha256": digest, "status": "REVIEWED_MAPPING_ONLY"}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--catalog", type=Path, default=CATALOG)
    ap.add_argument("--schema", type=Path)
    args = ap.parse_args()
    print("ICD_YAML_VALIDATED: " + json.dumps(
        validate(args.root, args.catalog, args.schema), sort_keys=True))


if __name__ == "__main__":
    main()
