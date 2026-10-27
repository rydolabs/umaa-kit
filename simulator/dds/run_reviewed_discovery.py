"""Independent process supervisor for two additional official UMAA DDS Topics.

Checks only discovery of DDS Reader/Writer instances; NEVER claims payload
semantics, ACK/Status or UMAA normative QoS are verified.
"""
import subprocess
import sys
import time


def run(host, oracle):
    simulator=subprocess.Popen([oracle], text=True, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT)
    try:
        time.sleep(0.3)
        result=subprocess.run([host], text=True, capture_output=True, timeout=15)
        output,_=simulator.communicate(timeout=15)
        print(result.stdout)
        print(result.stderr)
        print(output)
        return (result.returncode==0 and simulator.returncode==0 and
                'REVIEWED_SERVICE_DISCOVERY_HOST_PASS' in result.stdout and
                'REVIEWED_SERVICE_DISCOVERY_SIM_PASS' in output)
    finally:
        if simulator.poll() is None:
            simulator.kill()
            simulator.communicate()


if __name__=='__main__':
    if len(sys.argv)!=3:
        sys.exit(2)
    sys.exit(0 if run(sys.argv[1],sys.argv[2]) else 1)
