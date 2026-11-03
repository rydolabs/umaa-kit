"""Run official Battery Report+Cell proof over two genuine Cyclone DDS processes.

One process publishes official TypeSupport samples using a host-side codec,
the other is a standalone C oracle with no SDK linkage. A bad checkpoint must
be REJECTED by the oracle (exit code 10), not merely cause a timeout.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) not in (3, 4):
        print("usage: run_battery_payload.py <publisher> <C-oracle> [--bad-checkpoint]",
              file=sys.stderr)
        return 2
    publisher, oracle = sys.argv[1:3]
    bad = len(sys.argv) == 4 and sys.argv[3] == "--bad-checkpoint"
    if len(sys.argv) == 4 and not bad:
        return 2
    env = dict(os.environ)
    # A single named test gets an isolated discovery domain. Other existing
    # official UMAA test jobs and regression samples remain unaffected.
    env["CYCLONEDDS_URI"] = env.get("CYCLONEDDS_URI", "")
    consumer = subprocess.Popen(
        [oracle], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        time.sleep(0.5)
        process = subprocess.run(
            [publisher, "unused-demo-module", *(["--bad-checkpoint"] if bad else [])],
            env=env, capture_output=True, text=True, timeout=18,
        )
        output, _ = consumer.communicate(timeout=13)
        print(process.stdout, end="")
        print(process.stderr, end="", file=sys.stderr)
        print(output, end="")
        if process.returncode != 0:
            print("publisher failed rc", process.returncode, file=sys.stderr)
            return 3
        if bad:
            return 0 if (consumer.returncode == 10 and
                         "INDEPENDENT_BATTERY_CHECKPOINT_REJECTED" in output) else 4
        return 0 if (
            consumer.returncode == 0 and
            "SIM: PASS independent official Battery Report+Cell" in output
        ) else 5
    except subprocess.TimeoutExpired:
        print("Battery native TypeSupport process timeout", file=sys.stderr)
        return 6
    finally:
        if consumer.poll() is None:
            consumer.kill()
            consumer.communicate()


if __name__ == "__main__":
    raise SystemExit(main())
