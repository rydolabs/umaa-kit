#!/usr/bin/env python3
"""Build generated Cyclone DDS C TypeSupport for audited UMAA topic messages.

Only the reviewed catalog defines which messages are needed. This script
does not interpret service behavior, QoS or DDS instances. The original AUVSI
IDL tree stays external and is not copied into the repository.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from verify_official_service_types import inspect, type_idl_path


def selected_types(catalog: dict, excluded=()):
    excluded = set(excluded)
    report = inspect(catalog)
    if report["status"] != "PASS":
        raise ValueError("reviewed catalog invalid: " + repr(report["errors"]))
    known = {item["name"] for item in catalog["services"]}
    if not excluded.issubset(known):
        raise ValueError("unknown excluded service: " + repr(sorted(excluded - known)))
    return sorted({type_idl_path(t["type"]).as_posix()
                   for svc in catalog["services"] if svc["name"] not in excluded
                   for t in svc["provider_topics"] + svc["consumer_topics"]})


def generate(catalog, original_root, output_root, idlc="idlc", excluded=()):
    inputs = selected_types(catalog, excluded)
    if not original_root.is_dir():
        raise ValueError("missing original official IDL tree")
    if not inputs:
        raise ValueError("no reviewed official topics to generate")
    for rel in inputs:
        if not (original_root / rel).is_file():
            raise FileNotFoundError("unresolved official topic: " + rel)
    for rel in inputs:
        subprocess.run([sys.executable, str(Path(__file__).parent / "official_idl_compile.py"),
                        "--root", str(original_root), "--out", str(output_root),
                        "--idlc", idlc, "--source", rel, "--generate-only"], check=True)
    for rel in inputs:
        if not (output_root / Path(rel).with_suffix(".h")).is_file():
            raise ValueError("TypeSupport header not generated: " + rel)
    return inputs


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--catalog", required=True, type=Path)
    ap.add_argument("--official-root", required=True, type=Path)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--idlc", default="idlc")
    ap.add_argument("--exclude-service", action="append", default=[])
    args = ap.parse_args(argv)
    try:
        doc = json.loads(args.catalog.read_text(encoding="utf-8"))
        files = generate(doc, args.official_root, args.out, args.idlc, args.exclude_service)
        print("OFFICIAL_REVIEWED_TYPESUPPORT_PASS count=" + str(len(files)))
        return 0
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        print("OFFICIAL_REVIEWED_TYPESUPPORT_FAIL: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
