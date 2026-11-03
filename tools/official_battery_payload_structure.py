#!/usr/bin/env python3
"""IDL-pinned host-side structure validation for UMAA EO BatteryStatus.

This is a restricted source-audit and host JSON shape checker, NOT a DDS CDR
codec or a generated vendor TypeSupport binding. Never infer conformance from it.
"""
from __future__ import annotations

import argparse
from decimal import Decimal, InvalidOperation
import json
import math
from pathlib import Path
import re
import sys

from tools.official_icd_evidence import (
    SourceError, check_idl_imports, inspected_zip, sha, source_type,
)

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MANIFEST = ROOT / "catalog/official-evidence/eo-battery-payload-v1.json"
DEFAULT_SOURCE_PINS = ROOT / "catalog/official-evidence/eo-battery-status-6.0.json"
MAX_MANIFEST_BYTES = 64 * 1024
_FQN = re.compile(r"^[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*$")
_HEX_GUID = re.compile(r"^[0-9a-f]{32}$")
_FIELD = re.compile(
    r"(?:(@key|@optional)\s+)?"
    r"([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s+([A-Za-z_]\w*)"
)


def _load_json(path: Path) -> dict:
    if not path.is_file() or path.is_symlink() or path.stat().st_size > MAX_MANIFEST_BYTES:
        raise SourceError("payload evidence manifest absent, symlinked or oversized")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise SourceError("invalid JSON source evidence manifest") from exc
    if not isinstance(data, dict):
        raise SourceError("source evidence manifest must be an object")
    return data


def _without_comments(text: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)


def _fields(idl: str, name: str) -> list[dict]:
    if not _FQN.fullmatch(name):
        raise SourceError("invalid type in payload manifest")
    short = name.split("::")[-1]
    source = _without_comments(idl)
    matches = list(re.finditer(
        r"\bstruct\s+" + re.escape(short) + r"\s*\{([^{}]*)\}\s*;",
        source, flags=re.DOTALL,
    ))
    if len(matches) != 1:
        raise SourceError("missing or ambiguous struct " + name)
    found = []
    for statement in matches[0].group(1).split(";"):
        statement = statement.strip()
        if not statement:
            continue
        m = _FIELD.fullmatch(statement)
        if not m:
            raise SourceError("unsupported field syntax in " + name)
        annotation = (m.group(1) or "").lstrip("@")
        found.append({"name": m.group(3), "type": m.group(2),
                      "annotation": annotation})
    if len({x["name"] for x in found}) != len(found):
        raise SourceError("duplicate members in " + name)
    return found


def _typedef(idl: str, name: str) -> dict:
    if not re.fullmatch(r"[A-Za-z_]\w*", name):
        raise SourceError("unsafe typedef name")
    matches = list(re.finditer(
        r"\btypedef\s+(long long|double|long|octet)\s+" +
        re.escape(name) + r"(?:\[(\d+)\])?\s*;\s*(?://([^\r\n]*))?",
        idl,
    ))
    if len(matches) != 1:
        raise SourceError("missing or ambiguous scalar typedef " + name)
    m = matches[0]
    result = {"base": m.group(1)}
    if m.group(2) is not None:
        result["arrayLength"] = int(m.group(2))
    if m.group(3):
        for bound in ("max", "min"):
            b = re.search(
                bound + r"Inclusive=(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)",
                m.group(3),
            )
            if b:
                result[bound] = Decimal(b.group(1))
    return result


def _enum_values(idl: str, name: str) -> list[str]:
    if not re.fullmatch(r"[A-Za-z_]\w*", name):
        raise SourceError("unsafe enum name")
    source = _without_comments(idl)
    matches = list(re.finditer(
        r"\benum\s+" + re.escape(name) + r"\s*\{([^{}]*)\}\s*;",
        source, flags=re.DOTALL,
    ))
    if len(matches) != 1:
        raise SourceError("missing or ambiguous enum " + name)
    values = [x.strip() for x in matches[0].group(1).split(",") if x.strip()]
    if not values or any(not re.fullmatch(r"[A-Za-z_]\w*", x) for x in values):
        raise SourceError("unsupported enum members " + name)
    return values


def inspect(archive: Path, manifest: Path = DEFAULT_MANIFEST,
            source_pins: Path = DEFAULT_SOURCE_PINS) -> dict:
    """Bind nested payload field/typedef shape to exactly pinned original IDL.

    Only the selected reviewed substructures are checked. The vendor DDS IDL
    compiler remains authoritative for native language type and wire layout.
    """
    spec = _load_json(manifest)
    pins = _load_json(source_pins)
    if (spec.get("schemaVersion") != 1 or
            spec.get("kind") != "UNAPPROVED_OFFICIAL_BATTERY_PAYLOAD_STRUCTURE" or
            spec.get("serviceId") != "UMAA::EO::BatteryStatus" or
            spec.get("readiness") != {
                "officialRuntimeBinding": False,
                "nativeDdsCodec": False,
                "collectionLifecycle": False,
                "formalConformance": False,
            } or
            spec.get("idlArchiveSha256") != pins.get("sourcePins", {}).get(
                "officialIdlArchiveSha256"
            )):
        raise SourceError("payload manifest/source pins not compatible or fail-closed")
    raw = archive.read_bytes() if archive.is_file() and not archive.is_symlink() else b""
    if sha(raw) != spec["idlArchiveSha256"]:
        raise SourceError("IDL archive SHA-256 mismatch for payload structure")
    source_hashes = spec.get("sourceSha256")
    structures = spec.get("structures")
    typedefs = spec.get("scalarTypedefs")
    enumeration = spec.get("enumDefinition")
    if (not isinstance(source_hashes, dict) or not isinstance(structures, dict) or
            not isinstance(typedefs, dict) or not isinstance(enumeration, dict)):
        raise SourceError("incomplete payload source mappings")
    report_fqn = "UMAA::EO::BatteryStatus::BatteryReportType"
    element_fqn = "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement"
    if (report_fqn not in structures or element_fqn not in structures or
            len(structures) != 6 or len(typedefs) != 9 or
            len(source_hashes) != 6):
        raise SourceError("reviewed Battery payload closure is incomplete")

    with inspected_zip(archive) as z:
        closure = check_idl_imports(z)
        if not closure["allIdlImportsPresent"]:
            raise SourceError("IDL import closure is incomplete")
        texts = {}
        for path, expected in source_hashes.items():
            if not isinstance(path, str) or not path.startswith("UMAA/") or ".." in Path(path).parts:
                raise SourceError("unsafe IDL member reference")
            data = z.read(path)
            if sha(data) != expected:
                raise SourceError("reviewed IDL member hash mismatch: " + path)
            texts[path] = data.decode("utf-8")

        for fqn, entry in structures.items():
            if (not isinstance(entry, dict) or
                    entry.get("source") not in texts or
                    not isinstance(entry.get("fields"), list) or
                    entry["fields"] != _fields(texts[entry["source"]], fqn)):
                raise SourceError("payload nested field/optional/Key drift: " + fqn)
        for name, entry in typedefs.items():
            if not isinstance(entry, dict) or entry.get("source") not in texts:
                raise SourceError("missing scalar source: " + name)
            parsed = _typedef(texts[entry["source"]], name)
            if parsed["base"] != entry["base"] or parsed.get("arrayLength") != entry.get("arrayLength"):
                raise SourceError("scalar IDL type/array drift: " + name)
            for bound in ("min", "max"):
                if bound in entry:
                    if parsed.get(bound) is None or parsed[bound] != Decimal(str(entry[bound])):
                        raise SourceError("scalar IDL range drift: " + name + "/" + bound)
                elif bound in parsed:
                    raise SourceError("unreviewed scalar bound: " + name)
        if enumeration.get("source") not in texts or _enum_values(
            texts[enumeration["source"]], enumeration["name"]
        ) != enumeration.get("values"):
            raise SourceError("Battery state enum drift")

        for fqn, role in ((report_fqn, "report"), (element_fqn, "collectionElement")):
            actual = source_type(z, fqn)
            baseline = pins["topics"][role]
            for field in ("idlPath", "idlSha256", "ddsTopicName", "keyFields", "keyFieldTypes"):
                if actual[field] != baseline[field]:
                    raise SourceError("Battery topic/Key mismatch vs parent evidence " + role)
            described_keys = [x["name"] for x in structures[fqn]["fields"] if x["annotation"] == "key"]
            if described_keys != actual["keyFields"]:
                raise SourceError("Battery topic Key field order mismatch: " + role)
    return {
        "kind": "SOURCE_CHECKED_UNAPPROVED_BATTERY_PAYLOAD_STRUCTURE",
        "schemaVersion": 1,
        "serviceId": spec["serviceId"],
        "idlArchiveSha256": spec["idlArchiveSha256"],
        "membersVerified": sorted(source_hashes),
        "verifiedStructs": structures,
        "verifiedScalarTypedefs": typedefs,
        "verifiedStateEnum": enumeration,
        "allIdlImportsPresent": True,
        "hostValueProjectionOnly": True,
        "nativeDdsCodec": False,
        "officialRuntimeBinding": False,
        "collectionLifecycle": False,
        "formalConformance": False,
    }


class HostProjection:
    """Strict, non-wire Python host JSON projection of source-checked members."""

    def __init__(self, verified: dict):
        if verified.get("kind") != "SOURCE_CHECKED_UNAPPROVED_BATTERY_PAYLOAD_STRUCTURE":
            raise SourceError("host projection requires source-checked payload metadata")
        self.structures = verified["verifiedStructs"]
        self.typedefs = verified["verifiedScalarTypedefs"]
        self.enums = verified["verifiedStateEnum"]["values"]
        self.fqn_prefix = "UMAA::Common::Measurement::"

    def validate(self, type_name: str, value: object) -> dict:
        if type_name not in self.structures:
            raise SourceError("unreviewed host payload type")
        return self._struct(type_name, value, 0)

    def _struct(self, type_name: str, value: object, depth: int) -> dict:
        if depth > 12 or not isinstance(value, dict):
            raise SourceError("invalid nested host payload structure")
        fields = self.structures[type_name]["fields"]
        valid = {f["name"] for f in fields}
        if set(value) - valid:
            raise SourceError("unknown host payload fields")
        result = {}
        for f in fields:
            name, item_type, annotation = f["name"], f["type"], f["annotation"]
            if name not in value:
                if annotation == "optional":
                    continue
                raise SourceError("missing required host payload field: " + name)
            item = value[name]
            if item is None:
                raise SourceError("JSON null is not a DDS @optional absent marker: " + name)
            if item_type in self.structures:
                result[name] = self._struct(item_type, item, depth + 1)
            elif item_type == "long":
                if type(item) is not int or not 0 <= item <= 2147483647:
                    raise SourceError("out-of-range LargeListMetadata.size")
                result[name] = item
            elif item_type.endswith("::PowerPlantStateEnumType"):
                if not isinstance(item, str) or item not in self.enums:
                    raise SourceError("invalid Battery state enum")
                result[name] = item
            elif item_type.startswith(self.fqn_prefix):
                result[name] = self._scalar(item_type[len(self.fqn_prefix):], item)
            elif type_name.endswith("::DateTime") and item_type in self.typedefs:
                result[name] = self._scalar(item_type, item)
            else:
                raise SourceError("unsupported source-checked payload field type: " + item_type)
        return result

    def _scalar(self, name: str, value: object) -> object:
        info = self.typedefs.get(name)
        if not info:
            raise SourceError("unreviewed scalar host type: " + name)
        if name == "NumericGUID":
            if not isinstance(value, str) or not _HEX_GUID.fullmatch(value):
                raise SourceError("NumericGUID must be 16 octets in lower-case hex")
            return value
        kind = info["base"]
        if kind in ("long", "long long"):
            if type(value) is not int:
                raise SourceError("integer expected for " + name)
            if not int(info["min"]) <= value <= int(info["max"]):
                raise SourceError("integer outside original IDL bound for " + name)
            return value
        if kind == "double":
            if type(value) not in (int, float) or not math.isfinite(value):
                raise SourceError("finite number expected for " + name)
            try:
                number = Decimal(str(value))
                if not Decimal(str(info["min"])) <= number <= Decimal(str(info["max"])):
                    raise SourceError("scalar outside original IDL bound for " + name)
            except InvalidOperation as exc:
                raise SourceError("invalid decimal scalar for " + name) from exc
            return value
        raise SourceError("unsupported scalar host base type")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Source-checked Battery host structure; NEVER official DDS wire codec")
    parser.add_argument("--idl-zip", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--source-pins", type=Path, default=DEFAULT_SOURCE_PINS)
    parser.add_argument("--json", type=Path, help="optional host-value JSON, structurally validate only")
    parser.add_argument("--type", choices=("report", "element"), default="report")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)
    try:
        inspection = inspect(args.idl_zip, args.manifest, args.source_pins)
        if args.json:
            if args.json.is_symlink() or not args.json.is_file() or args.json.stat().st_size > 1_000_000:
                raise SourceError("invalid host JSON payload source")
            input_value = json.loads(args.json.read_text(encoding="utf-8"))
            target = ("UMAA::EO::BatteryStatus::BatteryReportType" if args.type == "report"
                      else "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement")
            normalized = HostProjection(inspection).validate(target, input_value)
            inspection = {**inspection, "hostProjection": normalized}
        body = json.dumps(inspection, indent=2, sort_keys=True) + "\n"
        if args.out:
            if args.out.exists() or args.out.is_symlink() or any(p.is_symlink() for p in args.out.parents):
                raise SourceError("refuse overwriting host projection output")
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(body, encoding="utf-8")
        else:
            print(body, end="")
        print("SOURCE_VERIFIED_HOST_STRUCTURE_ONLY_NOT_DDS_CODEC", file=sys.stderr)
        return 0
    except (SourceError, ValueError, KeyError, TypeError, OSError, UnicodeError) as exc:
        print("SOURCE_VERIFIED_HOST_STRUCTURE_ERROR: " + str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
