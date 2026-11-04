"""Real official Cyclone Battery source -> SDK Large List DDS subscriber proof.

Runs separate original-TypeSupport publisher and SDK host joiner process. The
negative case MUST observe two DDS Dispose events and explicitly report an
inconsistent source timestamp; timeouts/crashes are never success.
"""
from __future__ import annotations
import os
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) not in (3, 4):
        return 2
    publisher, subscriber = sys.argv[1:3]
    bad = len(sys.argv) == 4 and sys.argv[3] == "--bad-checkpoint"
    if len(sys.argv) == 4 and not bad:
        return 2
    env = dict(os.environ)
    observer = subprocess.Popen(
        [subscriber, *(["--bad-checkpoint"] if bad else [])],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    )
    try:
        time.sleep(0.5)
        sent = subprocess.run(
            [publisher, "unused-demo-module", *(["--bad-checkpoint"] if bad else [])],
            env=env, capture_output=True, text=True, timeout=18,
        )
        output, _ = observer.communicate(timeout=15)
        print(sent.stdout)
        print(sent.stderr, file=sys.stderr)
        print(output)
        if sent.returncode:
            return 3
        if bad:
            return 0 if observer.returncode == 10 and "SDK_BATTERY_LIVE_CHECKPOINT_REJECTED" in output else 4
        return 0 if observer.returncode == 0 and "SDK_BATTERY_LIVE_JOINER_PASS" in output else 5
    except subprocess.TimeoutExpired:
        return 6
    finally:
        if observer.poll() is None:
            observer.kill()
            observer.communicate()


if __name__ == "__main__":
    raise SystemExit(main())
