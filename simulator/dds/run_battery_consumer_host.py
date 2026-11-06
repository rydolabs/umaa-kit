"""Original UMAA Battery DDS -> native SDK Host -> ABI-v2 business DLL.

The independent original TypeSupport publisher emits two real sources and
four keyed Cell Dispose events. Invalid checkpoints must NOT hit business.
"""
from __future__ import annotations
import os
import re
import subprocess
import sys
import time

def invalid_cli(host, module):
    cases=[
        ["--module",module,"--domain-id","-1"],
        ["--module",module,"--domain-id","231"],
        ["--module",module,"--timeout-ms","0"],
        ["--module",module,"--until-report-disposes","2"],
        ["--module",module,"--until-element-disposes","4"],
        ["--module",module,"--until-element-disposes","0","--timeout-ms","1000"],
        ["--module",module,"--module",module],
        ["--module",module,"--unknown","value"],
        ["--module","missing-battery-business-module-xyz"],
    ]
    for args in cases:
        result=subprocess.run([host,*args],capture_output=True,text=True,timeout=5)
        if result.returncode==0 or "UMAA_BATTERY_HOST_ERROR:" not in result.stderr:
            raise RuntimeError("native Battery Host admitted invalid arguments: "+repr(args))

def main():
    if len(sys.argv) not in (4,5) or (
            len(sys.argv)==5 and sys.argv[4]!="--bad-checkpoint"):
        print("usage: supervisor PUBLISHER HOST MODULE [--bad-checkpoint]",
              file=sys.stderr)
        return 2
    publisher,host,module=sys.argv[1:4]
    bad=len(sys.argv)==5
    host_process=subprocess.Popen([
        host,"--module",module,
        "--until-report-disposes","2",
        "--until-element-disposes","4",
        "--timeout-ms","18000",
    ],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=os.environ.copy())
    try:
        time.sleep(0.6)
        sent=subprocess.run([
            publisher,"--two-lists-bad-checkpoint" if bad else "--two-lists"
        ],capture_output=True,text=True,timeout=22)
        output,_=host_process.communicate(timeout=23)
        print(sent.stdout,end="")
        print(sent.stderr,end="",file=sys.stderr)
        print(output,end="")
        sent_marker=("BATTERY_TWO_LIST_BAD_CHECKPOINT_SENT" if bad
                     else "BATTERY_TWO_LIST_ORIGINAL_WIRE_SENT")
        if sent.returncode!=0 or sent_marker not in sent.stdout:
            print("source-verified original Publisher failed",file=sys.stderr)
            return 3
        if host_process.returncode!=0 or "UMAA_BATTERY_HOST_READY" not in output:
            print("native Host failed",file=sys.stderr)
            return 4
        found=re.search(
            r"UMAA_BATTERY_HOST_STOPPED accepted=(\d+) disposed=(\d+) "
            r"nativeReportDisposes=(\d+) nativeElementDisposes=(\d+)",
            output)
        count=1 if bad else 2
        if not found or tuple(int(v) for v in found.groups())!=(count,count,2,4):
            print("business events do not match original native DDS",file=sys.stderr)
            return 5
        invalid_cli(host,module)
        print("UMAA_BATTERY_HOST_BAD_CHECKPOINT_ISOLATED" if bad
              else "UMAA_BATTERY_HOST_NATIVE_WIRE_PASS")
        return 0
    except (subprocess.TimeoutExpired,RuntimeError) as error:
        print("Battery native Host test error:",error,file=sys.stderr)
        return 6
    finally:
        if host_process.poll() is None:
            host_process.kill()
            host_process.communicate()

if __name__=="__main__":
    raise SystemExit(main())
