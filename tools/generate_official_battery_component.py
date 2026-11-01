#!/usr/bin/env python3
"""Generate a source-checked, *chargeRemaining-only* BatteryStatus C++ V2 module.

Deliberately NOT a full UMAA BatteryStatus implementation: no source GUID
encoding, List element protocol, mandatory Topic-name compatibility or
reviewed QoS. Native DDS binding remains in the external test host/backend.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys
from tools.official_icd_evidence import audit, inspected_zip, SourceError
from tools.umaa_contract_ir import load_service

SERVICE = "UMAA::EO::BatteryStatus"
TYPE = SERVICE + "::BatteryReportType"
COLLECTION = SERVICE + "::BatteryReportTypeCellsListElement"
SCHEMA = "umaa-6.0-batterystatus-chargeRemaining-f64-prototype-v1"

def verify(yaml_path: Path, archive: Path, generated_root: Path, digest: str) -> dict:
    if not re.fullmatch(r"[a-f0-9]{64}", digest):
        raise SourceError("pinned official ZIP SHA-256 required")
    info = audit(archive, [yaml_path])
    if info["archiveSha256"] != digest or info["issues"]:
        raise SourceError("source YAML and original official IDL disagree")
    doc, yaml_digest = load_service(yaml_path)
    if doc["metadata"]["id"] != SERVICE or doc["sources"]["idl"]["bundleSha256"] != digest:
        raise SourceError("BatteryStatus Service or IDL source pin mismatch")
    topics = doc["spec"]["topics"]
    if (set(topics) != {"report01"} or
        topics["report01"]["idlType"] != TYPE or
        topics["report01"]["ddsTopicName"] != TYPE or
        topics["report01"]["keyFields"] != ["source"] or
        topics["report01"]["role"] != "report" or
        topics["report01"]["direction"] != "provider-to-consumer"):
        raise SourceError("BatteryStatus Report Topic/Key or direction unreviewed")
    protocols=doc["spec"]["protocols"]
    if (len(protocols)!=1 or protocols[0]["kind"]!="request-reply" or
        protocols[0].get("mode")!="without-query-data" or
        protocols[0]["bindings"]!={"reports":["report01"]}):
        raise SourceError("BatteryStatus selected Report protocol mapping invalid")
    if doc["spec"]["unresolvedCollectionTypes"] != [COLLECTION]:
        raise SourceError("BatteryStatus Collection must remain explicitly unresolved")
    if doc["verification"]["ddsBehaviorVerified"] or doc["verification"]["ddsQosVerified"]:
        raise SourceError("selected-field prototype cannot approve normative behavior/QoS")
    descriptor=generated_root/"UMAA/EO/BatteryStatus/BatteryReportType.h"
    if descriptor.is_symlink() or not descriptor.is_file():
        raise SourceError("original Cyclone idlc Battery TypeSupport header not supplied")
    header=descriptor.read_text(encoding="utf-8")
    if (not re.search(r"typedef\s+struct\s+UMAA_EO_BatteryStatus_BatteryReportType\s*\{",header)
        or not re.search(r"\bdouble\s*\*\s*chargeRemaining\s*;",header)):
        raise SourceError("Battery chargeRemaining generated C type changed")
    with inspected_zip(archive) as zipped:
        text=zipped.read("UMAA/Common/Measurement/Measurements.idl").decode("utf-8-sig")
        checks=[
            r"\btypedef\s+double\s+EnergyPercent\s*;",
            r"\bconst\s+double\s+EnergyPercent_MIN\s*=\s*0(?:\.0+)?\s*;",
            r"\bconst\s+double\s+EnergyPercent_MAX\s*=\s*1000(?:\.0+)?\s*;",
        ]
        if any(not re.search(pattern,text) for pattern in checks):
            raise SourceError("official EnergyPercent type or bound changed")
    return {
        "scope":"BATTERYSTATUS_CHARGE_REMAINING_ONLY",
        "officialArchiveSha256":digest,
        "authoredYamlSha256":yaml_digest,
        "verifiedIdlType":TYPE,
        "sourceKeyFields":["source"],
        "verifiedField":"chargeRemaining",
        "verifiedFieldType":"optional double (EnergyPercent 0..1000)",
        "nativeDdsCodecGenerated":False,
        "completeServiceGenerated":False,
        "fullQosReviewed":False,
        "largeCollectionImplemented":False,
        "originalWireTopicNameVerified":False,
    }

HEADER="// GENERATED from checked original UMAA 6.0 Battery IDL + YAML.\n// NOT full BatteryStatus or a DDS wire codec. Only optional EnergyPercent field.\n#pragma once\n#include <cstdint>\n#include <cstring>\n#include <cmath>\n#include <limits>\n#include <stdexcept>\n#include <vector>\n#include <cstddef>\nnamespace umaa::generated {\nstruct BatteryChargeSubset {\n  static constexpr const char* id = \"UMAA::EO::BatteryStatus\";\n  static constexpr const char* schema_id =\n      \"umaa-6.0-batterystatus-chargeRemaining-f64-prototype-v1\";\n  static constexpr const char* declared_idl_topic_constant =\n      \"UMAA::EO::BatteryStatus::BatteryReportType\";\n  static constexpr bool complete_service = false;\n  static constexpr bool native_dds_codec = false;\n  struct Report { double charge_remaining{0}; };\n  static std::vector<std::uint8_t> encode_report(const Report& data) {\n    static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559,\n                  \"requires IEEE754 binary64 double\");\n    if(!std::isfinite(data.charge_remaining) ||\n       data.charge_remaining<0 || data.charge_remaining>1000)\n      throw std::invalid_argument(\"official EnergyPercent bounds 0..1000\");\n    std::uint64_t bits=0;\n    std::memcpy(&bits,&data.charge_remaining,8);\n    std::vector<std::uint8_t> result(8);\n    for(unsigned i=0;i<8;i++)result[i]=static_cast<std::uint8_t>(bits>>(8*i));\n    return result;\n  }\n  static bool decode_report(const std::uint8_t* bytes,std::size_t size,Report& output) {\n    if(size!=8 || !bytes)return false;\n    std::uint64_t bits=0;\n    for(unsigned i=0;i<8;i++)bits|=std::uint64_t(bytes[i])<<(8*i);\n    Report candidate{};\n    std::memcpy(&candidate.charge_remaining,&bits,8);\n    if(!std::isfinite(candidate.charge_remaining) ||\n       candidate.charge_remaining<0 || candidate.charge_remaining>1000)return false;\n    output=candidate;\n    return true;\n  }\n};\n} // namespace umaa::generated\n"
MODULE="// GENERATED: Component V2 provider. User Handler is linked separately.\n#include \"battery_charge_subset.hpp\"\n#include \"umaa/component.hpp\"\nnamespace umaa::generated {\numaa::ReportSource<BatteryChargeSubset>& external_battery_charge_source();\n}\nnamespace {\nvoid register_battery(umaa::Component& component) {\n  component.provide<umaa::generated::BatteryChargeSubset>(\n     umaa::generated::external_battery_charge_source());\n}\n}\nUMAA_EXPORT_COMPONENT_V2(\"UMAA-EO-BatteryStatus-charge-only-prototype\", register_battery)\n"

def generate(yaml_path:Path, archive:Path, generated_root:Path, output:Path,digest:str) -> dict:
    info=verify(yaml_path,archive,generated_root,digest)
    if output.exists() or output.is_symlink() or any(p.is_symlink() for p in output.parents):
        raise SourceError("output must be a new, nonsymlink directory")
    output.mkdir(parents=True)
    (output/"battery_charge_subset.hpp").write_text(HEADER,encoding="utf-8")
    (output/"battery_component_v2.cpp").write_text(MODULE,encoding="utf-8")
    (output/"battery_charge_source_evidence.json").write_text(
        json.dumps(info,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print("OFFICIAL_BATTERY_CHARGE_V2_GENERATED", info["verifiedField"])
    return info

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--yaml",required=True,type=Path)
    p.add_argument("--zip",required=True,type=Path)
    p.add_argument("--type-root",required=True,type=Path)
    p.add_argument("--expect-sha256",required=True)
    p.add_argument("--output",required=True,type=Path)
    a=p.parse_args()
    try:
        generate(a.yaml,a.zip,a.type_root,a.output,a.expect_sha256)
        return 0
    except (OSError,ValueError,KeyError) as e:
        print("OFFICIAL_BATTERY_CHARGE_ERROR: "+str(e),file=sys.stderr)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
