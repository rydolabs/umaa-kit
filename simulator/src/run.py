#!/usr/bin/env python3
"""External process-boundary test runner. Intentionally no SDK imports."""
import argparse
import json
import pathlib
import subprocess
import sys
from verify import Oracle, VerificationError


def run_scenario(sut: str, scenario_file: pathlib.Path):
    scenario = json.loads(scenario_file.read_text(encoding="utf-8"))
    actions = scenario["actions"]
    commands = "\n".join(actions + ["EXIT", ""])
    proc = subprocess.run([sut], input=commands, text=True, capture_output=True, timeout=15)
    if proc.returncode != 0:
        raise VerificationError(f"SUT crashed/returned {proc.returncode}: {proc.stderr.strip()}")
    observed = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
    oracle = Oracle()
    oracle.feed(observed)
    oracle.assert_complete()
    expected = scenario["expected_events"]
    if observed != expected:
        mismatches = [f"index {i}: expected={expected[i] if i<len(expected) else '<none>'}, observed={observed[i] if i<len(observed) else '<none>'}" for i in range(max(len(expected),len(observed))) if i>=len(expected) or i>=len(observed) or expected[i]!=observed[i]]
        raise VerificationError("Trace mismatch: " + "; ".join(mismatches[:6]))
    return {"scenario": scenario["name"], "result": "PASS", "events": len(observed), "transport": "stdio fixture only (NOT DDS)", "trace": observed}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--sut", required=True)
    p.add_argument("--scenario", required=True, type=pathlib.Path)
    p.add_argument("--report", type=pathlib.Path)
    a = p.parse_args()
    try:
        result = run_scenario(a.sut, a.scenario)
        if a.report:
            a.report.parent.mkdir(parents=True, exist_ok=True)
            a.report.write_text(json.dumps(result, indent=2) + "\n")
        print(f"PASS {result['scenario']} ({result['events']} observed events)")
    except (OSError, ValueError, subprocess.TimeoutExpired, VerificationError) as exc:
        print(f"FAIL {a.scenario}: {exc}", file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__": sys.exit(main())
