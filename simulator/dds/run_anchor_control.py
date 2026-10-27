"""Independently supervise real AnchorControl Command/Ack/Status/Dispose DDS.

Does not import the SDK or share any oracle implementation with the host.
"""
import os
import subprocess
import sys
import time


def main():
    if len(sys.argv) not in (3, 4):
        print("Usage: run_anchor_control.py <host> <independent-oracle> [--update|--stop|--bad-destination]",file=sys.stderr)
        return 2
    host, oracle = sys.argv[1:3]
    options = sys.argv[3:]
    proc = subprocess.Popen([oracle, *options], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, env=os.environ.copy())
    try:
        time.sleep(0.5)
        app = subprocess.run([host], capture_output=True, text=True,
                             timeout=26, env=os.environ.copy())
        wire_output, _ = proc.communicate(timeout=15)
        print(app.stdout, app.stderr, wire_output, flush=True)
        if options == ['--bad-destination']:
            return 0 if (app.returncode==3 and proc.returncode==0 and
               'command destination did not match configured provider' in app.stderr and
               'ANCHOR_SIM: PASS invalid destination with no Status/Ack' in wire_output) else 3
        return 0 if (app.returncode==0 and proc.returncode==0 and
                   'ANCHOR_HOST: PASS' in app.stdout and
                   'ANCHOR_SIM: PASS' in wire_output) else 3
    except subprocess.TimeoutExpired:
        print('AnchorControl DDS timed out',file=sys.stderr)
        return 4
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.communicate()


if __name__=='__main__':
    sys.exit(main())
