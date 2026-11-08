"""Prove a single SDK Host entrypoint runs a pinned original Battery binding.

Independent original-TypeSupport publisher and business DLL/SO are separate
native processes. A deliberately invalid second checkpoint must never reach
the user's business Handler, but actual keyed DDS Dispose must still arrive.
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
SOURCE=json.loads((ROOT/"catalog/umaa6-eo-batterystatus-source-bindings-v1.json").read_text())
IDL=SOURCE["sources"]["idlArchive"]["sha256"]
ICD=SOURCE["sources"]["icd"]["sha256"]
BINDING="UMAA::EO::BatteryStatus/cyclone/original-6.0-host-preview-v1"

def args(module: str) -> list[str]:
    return ["--native-binding",BINDING,
            "--module",module,
            "--source-idl-sha256",IDL,
            "--source-icd-sha256",ICD,
            "--until-report-disposes","2",
            "--until-element-disposes","4",
            "--timeout-ms","18000"]

def denied(host: str,module: str) -> None:
    baseline=args(module)
    invalid=[
        ["--native-binding","UMAA::EO::InventedService",
         *baseline[2:]],
        [*baseline[:5], "0"*64, *baseline[6:]],
        [*baseline[:7], "0"*64, *baseline[8:]],
        baseline[:-2]+["--timeout-ms","0"],
        [*baseline,"--until-report-disposes","2"],
        baseline[:-2]+["--timeout-ms","-1"],
        baseline[:2]+["--module","nonexistent-original-battery-business-library.so"]+baseline[4:],
        baseline[:2]+["--module",""],
        ["--native-binding",BINDING,"--module",module],
        [*baseline,"--unknown","item"],
    ]
    for i,case in enumerate(invalid):
        result=subprocess.run([host,*case],capture_output=True,text=True,
                              timeout=6)
        if result.returncode==0 or "UMAA_HOST_ERROR:" not in result.stderr:
            raise RuntimeError(f"native Host accepted invalid binding #{i}: "
                               f"{result.stdout!r} {result.stderr!r}")

def main() -> int:
    if len(sys.argv) not in (4,5) or (
        len(sys.argv)==5 and sys.argv[4]!="--bad-checkpoint"):
        print("usage: script PUBLISHER GENERIC_HOST MODULE [--bad-checkpoint]",
              file=sys.stderr)
        return 2
    publisher,host,module=sys.argv[1:4]
    bad=len(sys.argv)==5
    process=subprocess.Popen(
        [host,*args(module)],stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,text=True,env=os.environ.copy())
    try:
        time.sleep(0.6)
        sent=subprocess.run(
            [publisher,"--two-lists-bad-checkpoint" if bad else "--two-lists"],
            capture_output=True,text=True,timeout=24)
        output,_=process.communicate(timeout=24)
        print(sent.stdout,end="")
        print(sent.stderr,end="",file=sys.stderr)
        print(output,end="")
        marker=("BATTERY_TWO_LIST_BAD_CHECKPOINT_SENT" if bad
                else "BATTERY_TWO_LIST_ORIGINAL_WIRE_SENT")
        if sent.returncode!=0 or marker not in sent.stdout:
            raise RuntimeError("independent original publisher failed")
        if process.returncode!=0 or "UMAA_HOST_NATIVE_READY" not in output:
            raise RuntimeError("shared SDK Host original-source factory failed")
        match=re.search(
            r"UMAA_HOST_NATIVE_STOPPED accepted=(\d+) disposed=(\d+) "
            r"nativeReportDisposes=(\d+) nativeElementDisposes=(\d+)",output)
        n=1 if bad else 2
        if not match or tuple(map(int,match.groups()))!=(n,n,2,4):
            raise RuntimeError("original source/Dispose and business counters differ")
        denied(host,module)
        print("UMAA_COMMON_HOST_NATIVE_BATTERY_BAD_CHECKPOINT_ISOLATED" if bad
              else "UMAA_COMMON_HOST_NATIVE_BATTERY_SOURCE_PASS")
        return 0
    except (RuntimeError,subprocess.TimeoutExpired) as error:
        print("common native Host E2E failed:",error,file=sys.stderr)
        return 4
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()

if __name__=="__main__":
    raise SystemExit(main())
