"""Run a real split-topic LargeSet/List + G/S test in two processes.
Uses actual Cyclone/Fast DDS libraries and synthetic TypeSupport only.
"""
import subprocess
import sys
import time


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ("set", "list"):
        return 2
    exe, adapter, shape = sys.argv[1:]
    consumer = subprocess.Popen(
        [exe, "consumer", adapter, shape],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    try:
        time.sleep(0.85)
        producer = subprocess.run(
            [exe, "producer", adapter, shape],
            capture_output=True,
            text=True,
            timeout=28,
        )
        try:
            output, _ = consumer.communicate(timeout=23)
        except subprocess.TimeoutExpired:
            consumer.kill()
            output, _ = consumer.communicate(timeout=5)
        print("producer:", producer.returncode, producer.stdout, producer.stderr, flush=True)
        print("consumer:", consumer.returncode, output, flush=True)
        return int(
            producer.returncode != 0
            or consumer.returncode != 0
            or f"NATIVE_PRODUCER shape={shape}" not in producer.stdout
            or f"NATIVE_CONSUMER shape={shape}" not in output
            or "initial=1 pending=1 revised=1 final=1 delete_committed=1" not in output
        )
    finally:
        if consumer.poll() is None:
            consumer.kill()
            consumer.wait(timeout=5)


if __name__ == "__main__":
    sys.exit(main())
