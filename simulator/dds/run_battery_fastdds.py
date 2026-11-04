"""Independent two-process Fast DDS / genuine original Battery TypeSupport test.

Negative test passes only when the remote oracle explicitly rejects an
inconsistent checkpoint with exit code 10; a crash/timeout never passes.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("Usage: run_battery_fastdds.py <publisher> <oracle> [--bad-checkpoint]",
              file=sys.stderr)
        return 2
    pub, oracle = sys.argv[1:3]
    negative = len(sys.argv) == 4 and sys.argv[3] == "--bad-checkpoint"
    if len(sys.argv) == 4 and not negative:
        return 2
    environment = dict(os.environ)
    observer = subprocess.Popen(
        [oracle], env=environment, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, text=True,
    )
    try:
        time.sleep(0.8)
        publisher = subprocess.run(
            [pub, *(["--bad-checkpoint"] if negative else [])],
            capture_output=True, text=True, env=environment, timeout=25,
        )
        output, _ = observer.communicate(timeout=23)
        print(publisher.stdout, end="")
        print(publisher.stderr, end="", file=sys.stderr)
        print(output, end="")
        if publisher.returncode != 0:
            print("Fast DDS publisher failed:", publisher.returncode, file=sys.stderr)
            return 3
        if negative:
            return 0 if (observer.returncode == 10 and
                         "FAST_BATTERY_CHECKPOINT_REJECTED" in output) else 4
        return 0 if (observer.returncode == 0 and
                     "FAST_BATTERY_INDEPENDENT_WIRE_PASS" in output) else 5
    except subprocess.TimeoutExpired:
        print("Official Fast DDS original Battery wire test timed out", file=sys.stderr)
        return 6
    finally:
        if observer.poll() is None:
            observer.kill()
            observer.communicate()


if __name__ == "__main__":
    raise SystemExit(main())
