"""Separate-process four-topic synthetic DDS Collection+Specialization.
Every run checks that the exact same CLI binary works on each vendor plugin.
"""
import subprocess
import sys
import time


def main():
    if len(sys.argv) != 4 or sys.argv[3] not in ("set", "list"):
        return 2
    exe, plugin, kind = sys.argv[1:4]
    sub = subprocess.Popen([exe, "subscriber", plugin, kind],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True)
    try:
        time.sleep(1.0)
        pub = subprocess.run([exe, "publisher", plugin, kind],
                             text=True, capture_output=True, timeout=40)
        try:
            output, _ = sub.communicate(timeout=22)
        except subprocess.TimeoutExpired:
            sub.kill()
            output, _ = sub.communicate(timeout=5)
        print("publisher:", pub.returncode, pub.stdout, pub.stderr, flush=True)
        print("subscriber:", sub.returncode, output, flush=True)
        required = ("SPECIALIZATION_SUB full=1 skew_hidden=1 updated=1 "
                    "tombstone=1 disposed=1")
        return int(pub.returncode != 0 or sub.returncode != 0 or
                   required not in output or
                   "SPECIALIZATION_PUB sequence=2 tombstone=1 disposed=1"
                   not in pub.stdout)
    finally:
        if sub.poll() is None:
            sub.kill()
            sub.wait(timeout=5)


if __name__ == "__main__":
    sys.exit(main())
