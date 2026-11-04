#!/usr/bin/env python3
"""Produce standalone author-reviewable YAML candidates from externally verified IDL.

Never alters the canonical catalog-generated contracts, QoS, Operations, or
UMAA compliance flags. Original licensed source stays outside the repository.
"""
from __future__ import annotations
import argparse
import copy
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile

from tools.official_icd_evidence import audit, SourceError
from tools.umaa_contract_ir import load_service
from tools.validate_icd_yaml import require_dependencies

ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "contracts/umaa6"


def produce(zip_path: Path, service_paths: list[Path], output_root: Path,
            expected_archive_sha256: str) -> dict:
    if not re.fullmatch(r"[0-9a-f]{64}", expected_archive_sha256):
        raise SourceError("explicit pinned 64-hex archive SHA-256 required")
    if output_root.is_symlink() or output_root.exists():
        raise SourceError("output root must be a new, nonexistent directory")
    if CANONICAL.resolve() in output_root.resolve().parents or output_root.resolve() == CANONICAL.resolve():
        raise SourceError("canonical generated YAML is immutable; use a separate output directory")
    reviewed = audit(zip_path, service_paths)
    if reviewed["archiveSha256"] != expected_archive_sha256:
        raise SourceError("official archive SHA-256 differs from expected pin")
    if reviewed["status"] != "PASS":
        raise SourceError("source/YAML disagreement; refusing authored overlay: " + "; ".join(reviewed["issues"]))
    yaml, _ = require_dependencies()
    class Dumper(yaml.SafeDumper):
        def ignore_aliases(self, data):
            return True
    patches = []
    for path, evidence in zip(service_paths, reviewed["services"]):
        doc, _ = load_service(path)
        doc = copy.deepcopy(doc)
        name = doc["metadata"]["id"]
        pieces = name.split("::")
        if len(pieces) != 3 or pieces[0] != "UMAA" or not all(re.fullmatch(r"[A-Za-z_]\w*", x) for x in pieces):
            raise SourceError("invalid official Service path: " + name)
        if name != evidence["serviceId"]:
            raise SourceError("Service ordering mismatch")
        for row in evidence["topics"]:
            target = doc["spec"]["topics"][row["alias"]]
            target["ddsTopicName"] = row["source"]["ddsTopicName"]
            target["keyFields"] = row["source"]["keyFields"]
        doc["sources"]["idl"]["bundleSha256"] = expected_archive_sha256
        # Preserve original PDF provenance and explicit review flags as-is.
        if doc["verification"] != load_service(path)[0]["verification"]:
            raise SourceError("verification flags must not change")
        rel = Path("services") / pieces[1] / (pieces[2] + ".yaml")
        body = ("# SOURCE-DERIVED CANDIDATE: IDL Topic/Key declarations ONLY.\n"
                "# QoS, Operation, Collection, native DDS and compliance are NOT approved.\n"
                + yaml.dump(doc, Dumper=Dumper, sort_keys=False, allow_unicode=True))
        patches.append((rel, body))
    parent = output_root.parent
    parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=".umaa-overlay-", dir=parent))
    try:
        for rel, body in patches:
            dest = stage / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_text(body, encoding="utf-8")
            # Must conform to the exact same strict v1alpha1 schema.
            load_service(dest)
        manifest = {"kind": "UNAPPROVED_SOURCE_CANDIDATE", "archiveSha256": expected_archive_sha256,
                    "services": len(patches), "directTopicTypes": reviewed["topicsAudited"],
                    "unresolvedCollectionTypes": reviewed["unresolvedCollectionTypesAudited"],
                    "approvedDdsQos": False, "approvedOperations": False,
                    "nativeTypeSupport": False}
        (stage / "source-evidence-summary.json").write_text(json.dumps(manifest, sort_keys=True, indent=2) + "\n")
        stage.rename(output_root)
        return manifest
    finally:
        if stage.exists():
            shutil.rmtree(stage)


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description="Create immutable, source-backed YAML candidates")
    p.add_argument("--zip", type=Path, required=True)
    p.add_argument("--yaml-root", type=Path, required=True)
    p.add_argument("--out-root", type=Path, required=True)
    p.add_argument("--expect-sha256", required=True)
    args = p.parse_args(argv)
    try:
        source = sorted((args.yaml_root / "services").glob("*/*.yaml"))
        report = produce(args.zip, source, args.out_root, args.expect_sha256)
        print("UMAA_OFFICIAL_YAML_CANDIDATES: " + json.dumps(report, sort_keys=True))
        return 0
    except (SourceError, OSError, ValueError) as exc:
        print("UMAA_OFFICIAL_OVERLAY_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
