"""Independent process supervisor for two additional official UMAA DDS Topics.

Checks only discovery of DDS Reader/Writer instances; NEVER claims payload
semantics, ACK/Status or UMAA normative QoS are verified.
"""
import subprocess
import sys
import time


def run(host, oracle, options):
    simulator=subprocess.Popen([oracle, *options], text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    try:
        time.sleep(0.3)
        result=subprocess.run([host, *options], text=True, capture_output=True, timeout=15)
        output,_=simulator.communicate(timeout=15)
        print(result.stdout)
        print(result.stderr)
        print(output)
        return (result.returncode==0 and simulator.returncode==0 and
                'REVIEWED_SERVICE_DISCOVERY_HOST_PASS' in result.stdout and
                'REVIEWED_SERVICE_DISCOVERY_SIM_PASS' in output and
                (options not in (['--wire'], ['--semantic']) or
                 ('REVIEWED_SERVICE_TYPED_WIRE_HOST_PASS' in result.stdout and
                  'REVIEWED_SERVICE_TYPED_WIRE_SIM_PASS' in output)) and
                (options != ['--semantic'] or
                 ('ANCHOR_SPECS_SEMANTIC_HOST_PASS' in result.stdout and
                  'ANCHOR_SPECS_SEMANTIC_SIM_PASS' in output)))
    finally:
        if simulator.poll() is None:
            simulator.kill()
            simulator.communicate()


if __name__=='__main__':
    if len(sys.argv) not in (3,4):
        sys.exit(2)
    sys.exit(0 if run(sys.argv[1],sys.argv[2],sys.argv[3:]) else 1)
