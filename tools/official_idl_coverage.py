#!/usr/bin/env python3
"""Measure full official UMAA 6.0 IDL CycloneDDS-C compiler coverage.

Requires independently downloaded AUVSI IDL in --root, does not redistribute it.
The result is TypeSupport build coverage, *not* generated service API/conformance.
Fail closed: nonzero if any official input cannot generate/compile successfully.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--minimum-idls", type=int, default=352)
    parser.add_argument("--idlc", default="idlc")
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output.resolve()
    inputs = sorted(root.rglob("*.idl"))
    if len(inputs) < args.minimum_idls:
        raise ValueError(f"Unexpected official IDL count {len(inputs)} < {args.minimum_idls}")
    output.mkdir(parents=True, exist_ok=True)
    errors = []
    generated = 0
    for path in inputs:
        rel = path.relative_to(root)
        out = output / rel.parent
        out.mkdir(parents=True, exist_ok=True)
        call = subprocess.run([args.idlc, "-I", str(root), "-o", str(out), str(path)],
                              capture_output=True, text=True, timeout=40)
        if call.returncode:
            errors.append({"stage": "idl", "path": str(rel),
                           "returncode": call.returncode, "error": (call.stderr + call.stdout)[-1500:]})
        else:
            generated += 1
    files = sorted(output.rglob("*.c"))
    compiled = 0
    objects = output / "obj"
    objects.mkdir(exist_ok=True)
    for source in files:
        rel = source.relative_to(output)
        object_file = objects / rel.with_suffix(".o")
        object_file.parent.mkdir(parents=True, exist_ok=True)
        call = subprocess.run([args.cc, "-std=c11", "-fPIC", "-I", str(output),
                               "-I", str(root), "-c", str(source), "-o", str(object_file)],
                              capture_output=True, text=True, timeout=40)
        if call.returncode:
            errors.append({"stage": "compile", "path": str(rel),
                           "returncode": call.returncode, "error": (call.stderr + call.stdout)[-1500:]})
        else:
            compiled += 1
    summary = {
        "input_idl_files": len(inputs),
        "idl_generated": generated,
        "generated_c_files": len(files),
        "c_compiled": compiled,
        "failures": len(errors),
        "errors": errors,
        "scope": "Official UMAA 6.0 TypeSupport compile coverage only; no SDK service conformance"
    }
    report = output.parent / "official_idl_coverage.json"
    report.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print("OFFICIAL_UMAA_IDL_COVERAGE",
          f"input={len(inputs)} generated={generated} c_files={len(files)} compiled={compiled} failures={len(errors)}")
    for item in errors[:16]:
        print("OFFICIAL_IDL_FAILURE", item["stage"], item["path"], item["error"][-320:])
    print("OFFICIAL_UMAA_COVERAGE_REPORT", report)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
