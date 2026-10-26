"""Independent real-Cyclone-DDS process supervisor for the simulator executable.

No SDK-internal import, and no synthetic stdio events: subprocesses use DDS directly.
"""
import os
import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) not in (4, 5):
        print('Usage: run.py <publisher> <simulator> <service-module> [simulator-scenario-flag]', file=sys.stderr)
        return 2
    publisher, simulator, module_path = sys.argv[1:4]
    sim_options = sys.argv[4:]
    env = dict(os.environ)
    # Use Cyclone DDS default discovery: multicast disabled without peers prevents discovery.
    sim = subprocess.Popen([simulator, *sim_options], env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        time.sleep(0.5)
        pub = subprocess.run([publisher, module_path], env=env, capture_output=True, text=True, timeout=20)
        if pub.returncode != 0:
            print('PUBLISHER FAILED rc=', pub.returncode, pub.stdout, pub.stderr, flush=True)
            if sim.poll() is not None:
                print('SIMULATOR EARLY EXIT:', sim.communicate()[0], flush=True)
            return pub.returncode
        out, _ = sim.communicate(timeout=12)
        print(pub.stdout)
        print(out)
        return 0 if sim.returncode == 0 and 'SIM: PASS' in out else 3
    except subprocess.TimeoutExpired:
        print('DDS integration timeout', file=sys.stderr)
        return 4
    finally:
        if sim.poll() is None:
            sim.kill()
            sim.communicate()


if __name__ == '__main__':
    sys.exit(main())
