"""Original UMAA Battery three-instance Cyclone DDS E2E supervisor.

The independent SDK joiner must observe all three real keyed Cell data
samples, official Report metadata, and four DDS Instance Dispose events.
Only an explicit inconsistent-checkpoint rejection counts as negative PASS.
"""
from __future__ import annotations
import os
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("Usage: run_battery_multicell.py <publisher> <SDK oracle> "
              "[--bad-checkpoint]", file=sys.stderr)
        return 2
    publisher, receiver = sys.argv[1:3]
    invalid = len(sys.argv) == 4 and sys.argv[3] == "--bad-checkpoint"
    if len(sys.argv) == 4 and not invalid:
        return 2
    env = dict(os.environ)
    oracle = subprocess.Popen(
        [receiver, *(["--bad-checkpoint"] if invalid else [])],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    )
    try:
        time.sleep(0.6)
        writer = subprocess.run(
            [publisher, *(["--bad-checkpoint"] if invalid else [])],
            env=env, capture_output=True, text=True, timeout=22,
        )
        received, _ = oracle.communicate(timeout=15)
        print(writer.stdout, end="")
        print(writer.stderr, file=sys.stderr, end="")
        print(received, end="")
        if writer.returncode:
            print("official three-Cell publisher failed", writer.returncode,
                  file=sys.stderr)
            return 3
        marker = ("BATTERY_THREE_CELL_BAD_CHECKPOINT_REJECTED"
                  if invalid else "BATTERY_THREE_CELL_NATIVE_JOINER_PASS")
        expect = 10 if invalid else 0
        return 0 if oracle.returncode == expect and marker in received else 4
    except subprocess.TimeoutExpired:
        print("original three-Cell native DDS process timed out", file=sys.stderr)
        return 5
    finally:
        if oracle.poll() is None:
            oracle.kill()
            oracle.communicate()


if __name__ == "__main__":
    raise SystemExit(main())
