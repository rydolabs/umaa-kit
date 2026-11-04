"""Two independent official Battery Sources/Lists over genuine Cyclone DDS.

This supervisor requires two Report samples, four distinct keyed Cell
samples, two Report DDS Disposes and four Cell DDS Disposes; success is
granted ONLY to the independent consumer's exact exit code and marker.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("Usage: run_battery_multilist.py <publisher> <receiver>"
              " [--bad-checkpoint]", file=sys.stderr)
        return 2
    bad = len(sys.argv) == 4 and sys.argv[3] == "--bad-checkpoint"
    if len(sys.argv) == 4 and not bad:
        return 2
    publisher, receiver = sys.argv[1:3]
    env = dict(os.environ)
    reader = subprocess.Popen(
        [receiver, *(["--bad-checkpoint"] if bad else [])],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    )
    try:
        time.sleep(0.6)
        sender = subprocess.run(
            [publisher, "--two-lists-bad-checkpoint" if bad else "--two-lists"],
            env=env, capture_output=True, text=True, timeout=22,
        )
        received, _ = reader.communicate(timeout=18)
        print(sender.stdout, end="")
        print(sender.stderr, file=sys.stderr, end="")
        print(received, end="")
        if sender.returncode:
            print("official two-list sender failed", sender.returncode,
                  file=sys.stderr)
            return 3
        sender_marker = ("BATTERY_TWO_LIST_BAD_CHECKPOINT_SENT" if bad
                         else "BATTERY_TWO_LIST_ORIGINAL_WIRE_SENT")
        receiver_marker = ("BATTERY_TWO_LIST_BAD_CHECKPOINT_REJECTED" if bad
                           else "BATTERY_TWO_LIST_NATIVE_ROUTER_PASS")
        reader_code = 10 if bad else 0
        if (sender_marker not in sender.stdout or
                reader.returncode != reader_code or
                receiver_marker not in received):
            print("independent original two-list DDS evidence mismatch",
                  file=sys.stderr)
            return 4
        return 0
    except subprocess.TimeoutExpired:
        print("independent original two-list DDS process timed out",
              file=sys.stderr)
        return 5
    finally:
        if reader.poll() is None:
            reader.kill()
            reader.communicate()


if __name__ == "__main__":
    raise SystemExit(main())
