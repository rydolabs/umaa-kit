#!/usr/bin/env python3
"""Compile real IDL with Cyclone idlc and/or Fast DDS-Gen; NEVER parse IDL.

External official IDL input remains outside git. Native generated TypeSupport
is vendor-specific and must never be mistaken for UMAA ICD QoS conformance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

from tools.official_idl_compile import collect


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def native_generate(root: Path, source: str, output: Path, *,
                    vendor: str, idlc: str = "idlc",
                    fastddsgen: str = "fastddsgen",
                    source_sha256: str | None = None) -> dict:
    root = root.resolve(strict=True)
    if not root.is_dir() or output.is_symlink():
        raise ValueError("input root must be a directory and output cannot be a symlink")
    name = Path(source)
    if (name.is_absolute() or name.suffix.lower() != ".idl"
            or not name.parts or any(p in ("", ".", "..") for p in name.parts)):
        raise ValueError("IDL source must be a relative .idl path within its root")
    target = root / name
    if not target.is_file() or target.is_symlink():
        raise ValueError("IDL source is missing or a symlink")
    if output.resolve() == root or root in output.resolve().parents:
        raise ValueError("output must be outside original IDL tree")
    closure = collect(root, target)
    if not closure:
        raise ValueError("empty IDL dependency closure")
    if any(p.is_symlink() for p in closure):
        raise ValueError("IDL include symlinks not allowed")
    digest = sha256(target)
    if source_sha256 is not None and digest != source_sha256:
        raise ValueError("source SHA-256 mismatch")
    if vendor not in ("cyclone", "fastdds", "both"):
        raise ValueError("unsupported DDS vendor")
    vendors = ["cyclone", "fastdds"] if vendor == "both" else [vendor]
    for tool in [idlc if v == "cyclone" else fastddsgen for v in vendors]:
        if shutil.which(tool) is None:
            raise FileNotFoundError("native IDL compiler not found: " + tool)
    # Fast DDS-Gen v2 produces flat, basename-named outputs. Reject a
    # dependency collision rather than silently replacing another type.
    if "fastdds" in vendors:
        stems = [p.stem for p in closure]
        if len(stems) != len(set(stems)):
            raise ValueError("Fast DDS-Gen flattened filename collision")
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    generated: dict[str, list[str]] = {}
    for v in vendors:
        dest = output / v
        dest.mkdir(parents=True, exist_ok=True)
        if v == "cyclone":
            # idlc compiles each transitive dependency through the official
            # compiler, retaining relative include paths.
            for dep in closure:
                rel = dep.relative_to(root)
                parent = dest / rel.parent
                parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(
                    [idlc, "-I", str(root), "-o", str(parent), str(dep)],
                    check=True, timeout=120,
                )
            expected = dest / name.with_suffix(".c")
            if not expected.is_file() or not expected.with_suffix(".h").is_file():
                raise ValueError("Cyclone compiler did not emit expected TypeSupport")
            native = sorted(dest.rglob("*.c"))
        else:
            # Compile imports first. These are actual vendor compiler calls:
            # no use of the prototype regex parser or generated host codec.
            for dep in closure:
                subprocess.run(
                    [fastddsgen, "-replace", "-I", str(root), "-d", str(dest),
                     str(dep)],
                    check=True, timeout=180,
                )
            # Fast DDS-Gen writes flat filenames but preserves original
            # include locations in generated C++ headers. Recreate just
            # the derived header layout, without copying source IDLs or
            # compiling duplicate .cxx translation units.
            for dep in closure:
                mirrored = dest / dep.relative_to(root).parent
                mirrored.mkdir(parents=True, exist_ok=True)
                for header in dest.iterdir():
                    if (header.is_file() and header.name.startswith(dep.stem)
                            and header.suffix in (".h", ".hpp", ".ipp")):
                        shutil.copy2(header, mirrored / header.name)
            native = sorted(dest.rglob("*.cxx"))
            if not any(p.name.startswith(name.stem) and
                       "PubSubType" in p.name for p in native):
                raise ValueError("Fast DDS-Gen emitted no target PubSubType .cxx")
        if not native:
            raise ValueError(v + " compiler generated no native sources")
        generated[v] = [str(p.relative_to(output)) for p in native]
    result = {
        "format": "UMAA-native-IDL-generator-v1",
        "vendor": vendor,
        "source": name.as_posix(),
        "inputSha256": digest,
        "transitiveIdls": len(closure),
        "sourceDependencies": [
            {"path": p.relative_to(root).as_posix(), "sha256": sha256(p)}
            for p in closure
        ],
        "nativeSources": generated,
        "nativeTypeSupportGenerated": True,
        "serviceQosVerified": False,
        "officialConformanceClaim": False,
    }
    (output / "native-generation.json").write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return result


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="real native vendor IDL code generation")
    ap.add_argument("--root", type=Path, required=True)
    ap.add_argument("--source", required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--vendor", choices=["cyclone", "fastdds", "both"], default="both")
    ap.add_argument("--idlc", default="idlc")
    ap.add_argument("--fastddsgen", default="fastddsgen")
    ap.add_argument("--source-sha256")
    args = ap.parse_args(argv)
    try:
        result = native_generate(args.root, args.source, args.out,
                                 vendor=args.vendor, idlc=args.idlc,
                                 fastddsgen=args.fastddsgen,
                                 source_sha256=args.source_sha256)
        print("UMAA_NATIVE_IDL_GENERATION_PASS", result["vendor"],
              "dependencies=", result["transitiveIdls"],
              "native_sources=", sum(map(len, result["nativeSources"].values())))
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as exc:
        print("UMAA_NATIVE_IDL_GENERATION_FAIL: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
