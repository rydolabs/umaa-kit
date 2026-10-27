#!/usr/bin/env python3
"""Fail-closed static audit of the evidence trace schema (not a compliance verdict)."""
import argparse
import json
from pathlib import Path

def validate(root):
    trace=json.loads((root/"catalog/umaa6-compliance-trace.json").read_text(encoding="utf-8"))
    catalog=json.loads((root/"catalog/umaa6-six-icd-reviewed.json").read_text(encoding="utf-8"))
    assert trace["schema"] == "umaa-kit-compliance-trace-v1"
    sources={x["id"] for x in trace["sources"]}
    requirements=trace["requirements"]
    assert len(requirements)>=20
    ids=[x["id"] for x in requirements]
    assert len(ids)==len(set(ids))
    assert {"1-1(1)","1-1(6)","1-3.2(a)","1-3.2(b)(2)","1-3.2(c)"} <= {x["clause"] for x in requirements}
    allowed={"OPEN","PARTIAL","PASS","FAIL","SCOPE_UNSET","NOT_APPLICABLE"}
    for requirement in requirements:
        assert requirement["source_id"] in sources
        assert requirement["clause"] and requirement["summary"]
        assert requirement["status"] in allowed
        assert requirement["applicability"] in {"UNRESOLVED","APPLICABLE","NOT_APPLICABLE"}
        assert isinstance(requirement["evidence_refs"],list)
        if requirement["status"]=="PASS":
            assert requirement["applicability"]=="APPLICABLE" and requirement["evidence_refs"], requirement["id"]
        if requirement["status"]=="NOT_APPLICABLE":
            assert requirement["applicability"]=="NOT_APPLICABLE" and requirement.get("not_applicable_rationale"), requirement["id"]
    assert trace["scope"]["status"].startswith("BLOCKED_")
    assert trace["scope"]["claim"]=="UNRESOLVED"
    assert trace["scope"]["component_id"] is None
    assert catalog["complete"] is False
    assert len(catalog["services"]) == 53
    topics=sum(len(s.get(key,[])) for s in catalog["services"] for key in ("provider_topics","consumer_topics","collection_topics"))
    print(f"TRACE_SCHEMA_PASS requirements={len(requirements)} reviewed_service_sections={len(catalog['services'])} total_catalog_entries={topics} component_scope=BLOCKED")
    print("COMPLIANCE_STATUS=NOT_DEMONSTRATED; schema validation is not official conformity assessment")

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("--root",type=Path,default=Path(__file__).resolve().parents[1])
    args=parser.parse_args()
    validate(args.root)
