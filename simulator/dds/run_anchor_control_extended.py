"""AnchorControl independent DDS supervisor. Never imports SDK code."""
import os
import subprocess
import sys
import time


def run(host, oracle, option):
    sim = subprocess.Popen([oracle, *option], text=True, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT)
    try:
        time.sleep(.4)
        env=os.environ.copy()
        if option == ['--complete']:
            env['UMAA_ANCHOR_SIMULATE_COMPLETE']='1'
        if option == ['--fail']:
            env['UMAA_ANCHOR_SIMULATE_FAILURE']='1'
        result = subprocess.run([host], text=True, capture_output=True, timeout=22, env=env)
        output, _ = sim.communicate(timeout=16)
        print(result.stdout, result.stderr, output, flush=True)
        if option == ['--bad-destination']:
            return (result.returncode == 3 and sim.returncode == 0 and
                    'command destination did not match configured provider' in result.stderr and
                    'ANCHOR_CONTROL_ORACLE: PASS invalid destination with no Status/Ack' in output)
        return (result.returncode == 0 and sim.returncode == 0 and
                'ANCHOR_CONTROL_HOST_PASS' in result.stdout and
                'ANCHOR_CONTROL_ORACLE: PASS official UMAA AnchorControl' in output)
    finally:
        if sim.poll() is None:
            sim.kill()
            sim.communicate()


if __name__ == '__main__':
    if len(sys.argv) not in (3, 4) or (len(sys.argv) == 4 and sys.argv[3] not in ('--update', '--bad-destination', '--raise', '--complete', '--fail')):
        sys.exit(2)
    sys.exit(0 if run(sys.argv[1], sys.argv[2], sys.argv[3:]) else 1)
