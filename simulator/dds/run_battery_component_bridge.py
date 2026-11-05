"""Independent original DDS source -> actual native ABI-v2 Component module."""
from __future__ import annotations
import os
import subprocess
import sys
import time

def main() -> int:
    if len(sys.argv) not in (4,5):
        print("usage: runner <publisher> <consumer> <component dll/so> [--bad-checkpoint]",file=sys.stderr)
        return 2
    if len(sys.argv)==5 and sys.argv[4]!="--bad-checkpoint":return 2
    pub,consumer,module=sys.argv[1:4]
    bad=len(sys.argv)==5
    reader=subprocess.Popen([consumer,module,*(['--bad-checkpoint'] if bad else [])],
                            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,
                            env=os.environ.copy())
    try:
        time.sleep(0.6)
        sender=subprocess.run([pub,"--two-lists-bad-checkpoint" if bad else "--two-lists"],
                              capture_output=True,text=True,timeout=22)
        out,_=reader.communicate(timeout=25)
        print(sender.stdout,end="")
        print(sender.stderr,end="",file=sys.stderr)
        print(out,end="")
        if sender.returncode or ("BATTERY_TWO_LIST_BAD_CHECKPOINT_SENT" if bad
                                  else "BATTERY_TWO_LIST_ORIGINAL_WIRE_SENT") not in sender.stdout:
            print("source-pinned two-list native publisher failed",file=sys.stderr)
            return 3
        expected=10 if bad else 0
        marker="BATTERY_COMPONENT_ABI_BAD_CHECKPOINT_ISOLATED" if bad else "BATTERY_COMPONENT_ABI_TWO_SOURCE_NATIVE_WIRE_PASS"
        if reader.returncode!=expected or marker not in out:
            print("verified native ABI Component Consumer evidence mismatch",file=sys.stderr)
            return 4
        return 0
    except subprocess.TimeoutExpired:
        print("original ABI Component DDS proof timed out",file=sys.stderr)
        return 5
    finally:
        if reader.poll() is None:
            reader.kill()
            reader.communicate()

if __name__=="__main__":
    raise SystemExit(main())
