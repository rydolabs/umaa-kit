"""Fail-closed source-pinned UMAA Battery cross-vendor DDS interoperability supervisor.

Cyclone and Fast DDS exchange actual compiler-generated original IDL samples.
Independent receiving process MUST parse nested Report+Cell and both keyed
Dispose events. Bad metadata timestamp MUST be rejected for the right reason.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) not in (4, 5):
        print("Usage: run_battery_mixed.py <cyclone-to-fastdds|fastdds-to-cyclone> "
              "<publisher> <independent-oracle> [--bad-checkpoint]", file=sys.stderr)
        return 2
    direction, sender, receiver = sys.argv[1:4]
    if direction not in ("cyclone-to-fastdds", "fastdds-to-cyclone"):
        return 2
    bad = len(sys.argv) == 5 and sys.argv[4] == "--bad-checkpoint"
    if len(sys.argv) == 5 and not bad:
        return 2
    publisher_argv = [sender]
    if direction == "cyclone-to-fastdds":
        publisher_argv += ["unused-demo-module"]
    if bad:
        publisher_argv.append("--bad-checkpoint")

    env = dict(os.environ)
    # Use user-supplied DDS networking if present; otherwise normal DDSI RTPS
    # multicast/domain discovery, no hardcoded transports or synthetic bridge.
    observer = subprocess.Popen(
        [receiver], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        env=env, text=True,
    )
    try:
        time.sleep(1.0)
        produced = subprocess.run(
            publisher_argv, env=env, capture_output=True, text=True, timeout=22,
        )
        result, _ = observer.communicate(timeout=18)
        print("DIRECTION:", direction, "BAD_CHECKPOINT:", bad)
        print("PUBLISHER:", produced.stdout, produced.stderr)
        print("RECEIVER:", result)
        if produced.returncode != 0:
            print("Cross-vendor writer failed", produced.returncode, file=sys.stderr)
            return 3

        expected_positive = (
            "FAST_BATTERY_INDEPENDENT_WIRE_PASS"
            if direction == "cyclone-to-fastdds"
            else "SIM: PASS independent official Battery Report+Cell"
        )
        expected_negative = (
            "FAST_BATTERY_CHECKPOINT_REJECTED"
            if direction == "cyclone-to-fastdds"
            else "INDEPENDENT_BATTERY_CHECKPOINT_REJECTED"
        )
        if bad:
            return 0 if observer.returncode == 10 and expected_negative in result else 4
        return 0 if observer.returncode == 0 and expected_positive in result else 5
    except subprocess.TimeoutExpired:
        print("Original Battery cross-vendor discovery/data timeout", file=sys.stderr)
        return 6
    finally:
        if observer.poll() is None:
            observer.kill()
            observer.communicate()


if __name__ == "__main__":
    raise SystemExit(main())
