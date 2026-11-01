"""Separate-process explicit QoS test for both direct and runtime YAML paths."""
import subprocess
import sys
import time

def main():
    if len(sys.argv)!=5 or sys.argv[4] not in ("runtime","transient","ordered"):
        return 2
    exe, backend, component, variant=sys.argv[1:5]
    sub=subprocess.Popen([exe,"subscriber",backend,component,variant],
                         stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        time.sleep(0.75)
        pub=subprocess.run([exe,"publisher",backend,component,variant],
                           capture_output=True,text=True,timeout=20)
        try:
            output,_=sub.communicate(timeout=14)
        except subprocess.TimeoutExpired:
            sub.kill()
            output,_=sub.communicate()
        print("QoS publisher:",pub.returncode,pub.stdout,pub.stderr)
        print("QoS subscriber:",sub.returncode,output)
        return int(pub.returncode!=0 or sub.returncode!=0)
    finally:
        if sub.poll() is None:
            sub.kill()
            sub.wait()

if __name__=="__main__":
    raise SystemExit(main())
