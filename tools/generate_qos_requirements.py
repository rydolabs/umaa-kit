#!/usr/bin/env python3
"""Source-bounded IDL Topic QoS requirements -> generated SDK-only C++ header.

This is a minimum reviewed rule ledger, not a global/default QoS profile.
All other policies remain unreviewed and must not be invented.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "contracts/umaa6/qos/eo-large-collections.yaml"
OUTPUT = ROOT / "sdk/include/umaa/generated/umaa6_qos_requirements.hpp"
IDL = re.compile(r"UMAA(?:::[A-Za-z_]\w*){2,6}$")


def parse(path: Path) -> list[dict]:
    import yaml
    class Strict(yaml.SafeLoader):
        pass
    def mapping(loader, node):
        loader.flatten_mapping(node)
        out = {}
        for k, v in node.value:
            key=loader.construct_object(k)
            if not isinstance(key,str) or key in out:
                raise ValueError("duplicate/non-string QoS YAML mapping key")
            out[key]=loader.construct_object(v)
        return out
    Strict.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)
    if path.is_symlink() or not path.is_file() or path.stat().st_size>100_000:
        raise ValueError("missing/unsafe QoS source")
    doc=yaml.load(path.read_text(encoding="utf-8"), Loader=Strict)
    if (not isinstance(doc,dict) or set(doc)!={"schemaVersion","kind","source","entries"}
        or doc["schemaVersion"]!="1" or doc["kind"]!="QosRequirements"):
        raise ValueError("invalid QoS contract root")
    src=doc["source"]
    if (not isinstance(src,dict) or src!={"document":"UMAA-SPEC-EOICD-v6.0",
        "section":"3.8.1","page":14}):
        raise ValueError("unexpected/unreviewed official QoS source reference")
    if not isinstance(doc["entries"],list) or len(doc["entries"])!=1:
        raise ValueError("expected one reviewed Large Collection rule")
    entries=[]
    for e in doc["entries"]:
        if not isinstance(e,dict) or set(e)!={"serviceId","idlType","required"}:
            raise ValueError("invalid QoS entry")
        if (not isinstance(e["serviceId"],str) or not isinstance(e["idlType"],str)
            or not IDL.fullmatch(e["serviceId"]) or not IDL.fullmatch(e["idlType"])):
            raise ValueError("invalid QoS service/type")
        if (e["serviceId"]!="UMAA::EO::BatteryStatus" or
            e["idlType"]!="UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement" or
            e["required"]!={"presentationScope":"topic","orderedAccess":True}):
            raise ValueError("unreviewed QoS policy must not be silently added")
        entries.append(e)
    return entries


def render(entries: list[dict]) -> str:
    lines=[
        "// GENERATED from contracts/umaa6/qos/eo-large-collections.yaml; DO NOT EDIT",
        "// Verified EO ICD 6.0 §3.8.1 minimum; not an entire Service QoS profile.",
        "#pragma once",
        '#include "umaa/qos_policy.hpp"',
        "#include <optional>",
        "#include <string>",
        "namespace umaa::generated {",
        "inline std::optional<umaa::qos::Requirements> reviewed_qos_requirements(",
        "    const std::string& service_id, const std::string& topic_idl_type) {",
    ]
    for e in entries:
        lines.extend([
            '  if(service_id=="'+e["serviceId"]+'" &&',
            '     topic_idl_type=="'+e["idlType"]+'") {',
            "    umaa::qos::Requirements out;",
            "    out.presentation=umaa::qos::Presentation::topic;",
            "    out.require_ordered_access=true;",
            "    return out;",
            "  }",
        ])
    lines.extend(["  return std::nullopt;","}","} // namespace umaa::generated",""])
    return "\n".join(lines)


def main(argv=None):
    p=argparse.ArgumentParser()
    p.add_argument("--input",type=Path,default=INPUT)
    p.add_argument("--output",type=Path,default=OUTPUT)
    p.add_argument("--check",action="store_true")
    args=p.parse_args(argv)
    try:
        generated=render(parse(args.input))
        if args.check:
            if args.output.read_text(encoding="utf-8")!=generated:
                raise ValueError("QoS generated contract is stale")
        else:
            if args.output.is_symlink():
                raise ValueError("refusing symlink output")
            args.output.parent.mkdir(parents=True,exist_ok=True)
            args.output.write_text(generated,encoding="utf-8")
        print("UMAA_QOS_REVIEWED_CONTRACT_PASS")
        return 0
    except (OSError,ValueError,ImportError) as exc:
        print("UMAA_QOS_CONTRACT_ERROR: "+str(exc),file=sys.stderr)
        return 2


if __name__=="__main__":
    raise SystemExit(main())
