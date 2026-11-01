#!/usr/bin/env python3
"""Audit external UMAA IDL ZIP against source-aware Service YAMLs.

Only source-declared DDS Topic names, struct names, @key order, and hashes
are extracted. This never approves QoS, behavior, compliance or TypeSupport.
Original AUVSI IDL is never written to the repository or report.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import sys
import zipfile

from tools.umaa_contract_ir import load_service

TYPE_RE = re.compile(r"^[A-Za-z_]\w*(?:::[A-Za-z_]\w*){2,7}$")
MODULE_RE = re.compile(r"\bmodule\s+([A-Za-z_]\w*)\s*\{")
CONSTANT_RE = re.compile(r'\bconst\s+string\s+([A-Za-z_]\w*Topic)\s*=\s*"([^"\r\n]+)"\s*;')
KEY_RE = re.compile(r'@key\s+([\w:]+(?:<\d+>)?)\s+([A-Za-z_]\w*)\s*;')
IDL_FILE_LIMIT = 4 * 1024 * 1024
ZIP_TOTAL_LIMIT = 96 * 1024 * 1024
ZIP_MEMBER_LIMIT = 4096


class SourceError(ValueError):
    pass


def sha(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def inspected_zip(path: Path):
    """Reject unsafe ZIP entries before parsing any publisher source."""
    if not path.is_file() or path.is_symlink():
        raise SourceError("IDL archive must be a regular file")
    try:
        archive = zipfile.ZipFile(path)
    except (zipfile.BadZipFile, OSError) as exc:
        raise SourceError("invalid IDL ZIP") from exc
    infos = archive.infolist()
    if len(infos) > ZIP_MEMBER_LIMIT:
        archive.close()
        raise SourceError("too many archive members")
    seen = set()
    total = 0
    for entry in infos:
        name = entry.orig_filename  # unnormalized header, including on Windows
        parts = PurePosixPath(name).parts
        if (name != entry.filename or not parts or name.startswith("/") or "\\" in name
                or any(part in ( ".", "..") for part in parts)
                or (len(parts) > 1 and parts[0] not in ("UMAA", "LICENSE"))
                or name in seen or entry.flag_bits & 1
                or stat.S_ISLNK(entry.external_attr >> 16)):
            archive.close()
            raise SourceError("unsafe, duplicate or encrypted ZIP entry: " + name)
        seen.add(name)
        total += entry.file_size
        if entry.file_size > IDL_FILE_LIMIT or total > ZIP_TOTAL_LIMIT:
            archive.close()
            raise SourceError("ZIP uncompressed size limit exceeded")
    if "LICENSE" not in seen:
        archive.close()
        raise SourceError("source ZIP missing LICENSE")
    return archive


INCLUDE_RE = re.compile(r'^\s*#\s*include\s*"([^"\r\n]+)"', re.M)


def check_idl_imports(archive: zipfile.ZipFile) -> dict:
    """Verify every local IDL dependency resolves within the supplied archive."""
    names = set(archive.namelist())
    inputs = sorted(n for n in names if n.endswith(".idl"))
    edges = 0
    for path in inputs:
        try:
            src = archive.read(path).decode("utf-8-sig")
        except (UnicodeError, RuntimeError, zipfile.BadZipFile) as exc:
            raise SourceError("invalid IDL source/import file: " + path) from exc
        for rel in INCLUDE_RE.findall(src):
            if (not rel.startswith("UMAA/") or "\\" in rel or
                    any(part == ".." for part in PurePosixPath(rel).parts) or
                    not rel.endswith(".idl") or rel not in names):
                raise SourceError("missing or unsafe transitive IDL import: " + path + " => " + rel)
            edges += 1
    return {"idlFiles": len(inputs), "idlImportReferences": edges,
            "allIdlImportsPresent": True}


def source_type(archive: zipfile.ZipFile, fqtype: str) -> dict:
    if not TYPE_RE.fullmatch(fqtype) or not fqtype.startswith("UMAA::"):
        raise SourceError("unsupported source Type ID: " + str(fqtype))
    parts = fqtype.split("::")
    rel = "/".join(parts) + ".idl"
    if rel not in archive.namelist():
        # Large-collection element structs may be declared alongside their
        # parent Report in the same .idl. Resolve by an exact declaration,
        # never by a guessed filename or merely a matching namespace.
        folder = "/".join(parts[:-1]) + "/"
        sought = re.compile(r"\bconst\s+string\s+" + re.escape(parts[-1]) +
                            r'Topic\s*=\s*"' + re.escape(fqtype) + r'"\s*;')
        matches = []
        for candidate in archive.namelist():
            if candidate.startswith(folder) and candidate.endswith(".idl"):
                blob = archive.read(candidate).decode("utf-8-sig")
                if sought.search(re.sub(r"/\*.*?\*/|//[^\n]*", "", blob, flags=re.S)):
                    matches.append(candidate)
        if len(matches) != 1:
            raise SourceError("IDL type has no unique owning source: " + fqtype)
        rel = matches[0]
    try:
        data = archive.read(rel)
    except (RuntimeError, zipfile.BadZipFile) as exc:
        raise SourceError("IDL ZIP integrity error: " + rel) from exc
    if len(data) > IDL_FILE_LIMIT:
        raise SourceError("IDL member too large: " + rel)
    try:
        raw = data.decode("utf-8-sig")
    except UnicodeDecodeError as exc:
        raise SourceError("IDL source is not UTF-8: " + rel) from exc
    raw = re.sub(r"/\*.*?\*/|//[^\n]*", "", raw, flags=re.S)
    raw = re.sub(r"^\s*#.*$", "", raw, flags=re.M)
    modules = MODULE_RE.findall(raw)
    if modules[:len(parts)-1] != parts[:-1]:
        raise SourceError("IDL namespace does not match source path: " + rel)
    constants = CONSTANT_RE.findall(raw)
    expected_const = parts[-1] + "Topic"
    matches = [value for name, value in constants if name == expected_const]
    if len(matches) != 1 or matches[0] != fqtype:
        raise SourceError("DDS Topic constant missing, duplicate or mismatched: " + rel)
    pattern = re.compile(r"\bstruct\s+" + re.escape(parts[-1]) + r"\s*\{(.*?)\}\s*;", re.S)
    structs = pattern.findall(raw)
    if len(structs) != 1:
        raise SourceError("type struct missing or duplicate: " + rel)
    fields = KEY_RE.findall(structs[0])
    key_names = [name for _type, name in fields]
    if len(set(key_names)) != len(key_names):
        raise SourceError("duplicate IDL key: " + rel)
    return {"idlPath": rel, "idlSha256": sha(data), "idlType": fqtype,
            "ddsTopicName": matches[0], "keyFields": key_names,
            "keyFieldTypes": [type_name for type_name, _ in fields]}


def audit(archive_path: Path, service_paths: list[Path]) -> dict:
    if not service_paths:
        raise SourceError("at least one Service YAML required")
    results = []
    issues = []
    names = set()
    with inspected_zip(archive_path) as archive:
        imports = check_idl_imports(archive)
        for service_path in service_paths:
            doc, yaml_digest = load_service(service_path)
            service = doc["metadata"]["id"]
            if service in names:
                raise SourceError("duplicate Service YAML: " + service)
            names.add(service)
            actual = []
            for alias, declared in sorted(doc["spec"]["topics"].items()):
                dtype = declared["idlType"]
                try:
                    observed = source_type(archive, dtype)
                except SourceError as exc:
                    issues.append(service + "/" + alias + ": " + str(exc))
                    continue
                for field in ("ddsTopicName", "keyFields"):
                    expected = declared[field]
                    if expected is not None and expected != observed[field]:
                        issues.append(service + "/" + alias + ": reviewed " + field + " disagrees with IDL")
                actual.append({"alias": alias, "role": declared["role"],
                               "source": observed,
                               "unresolvedInYaml": [f for f in ("ddsTopicName", "keyFields", "qosProfileRef")
                                                    if declared[f] is None]})
            collections = []
            for dtype in doc["spec"].get("unresolvedCollectionTypes", []):
                try:
                    collections.append(source_type(archive, dtype))
                except SourceError as exc:
                    issues.append(service + "/unresolvedCollectionType: " + str(exc))
            results.append({"serviceId": service, "yamlSha256": yaml_digest,
                            "topics": actual, "unresolvedCollectionTypes": collections})
    return {"reportVersion": 1, "status": "PASS" if not issues else "CONFLICT",
            "scope": "IDL Topic constant / @key declaration evidence only; NOT official runtime or compliance",
            "archiveSha256": sha(archive_path.read_bytes()),
            "archiveIntegrity": imports,
            "servicesAudited": len(results), "topicsAudited": sum(len(s["topics"]) for s in results),
            "unresolvedCollectionTypesAudited": sum(len(s["unresolvedCollectionTypes"]) for s in results),
            "issues": sorted(issues), "services": results,
            "approvedDdsQos": False, "approvedOperationSemantics": False,
            "nativeDdsTypeSupportGenerated": False}


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description="Audit external UMAA IDL ZIP against reviewed Service YAML")
    p.add_argument("--zip", required=True, type=Path)
    p.add_argument("--yaml", action="append", type=Path)
    p.add_argument("--yaml-root", type=Path)
    p.add_argument("--expect-sha256")
    p.add_argument("--out", type=Path)
    args = p.parse_args(argv)
    try:
        paths = list(args.yaml or [])
        if args.yaml_root:
            paths += sorted((args.yaml_root / "services").glob("*/*.yaml"))
        result = audit(args.zip, paths)
        if args.expect_sha256 and args.expect_sha256 != result["archiveSha256"]:
            raise SourceError("IDL archive SHA-256 does not match pinned source")
        serialized = json.dumps(result, indent=2, sort_keys=True) + "\n"
        if args.out:
            if args.out.is_symlink():
                raise SourceError("refusing output symlink")
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(serialized, encoding="utf-8")
        else:
            print(serialized, end="")
        return 0 if not result["issues"] else 3
    except (SourceError, OSError, ValueError, zipfile.BadZipFile) as exc:
        print("UMAA_OFFICIAL_SOURCE_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
