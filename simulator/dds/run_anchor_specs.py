"""Supervise independent official AnchorSpecs value/update/dispose oracle."""
import subprocess
import sys
import time


def run(host, oracle):
    sim = subprocess.Popen([oracle], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        time.sleep(.4)
        result = subprocess.run([host], text=True, capture_output=True, timeout=18)
        output, _ = sim.communicate(timeout=18)
        print(result.stdout, result.stderr, output, flush=True)
        return (result.returncode == 0 and sim.returncode == 0 and
                'ANCHOR_SPECS_HOST_PASS meaningful_fields=2 dispose=1' in result.stdout and
                'ANCHOR_SPECS_ORACLE_PASS validated_anchor_size_holding_power_rode_length_description update=1 dispose=1' in output)
    finally:
        if sim.poll() is None:
            sim.kill()
            sim.communicate()


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(2)
    sys.exit(0 if run(sys.argv[1],sys.argv[2]) else 1)
