"""Run two independent actual middleware processes for atomic Collection frames."""
import subprocess
import sys
import time

def main():
    if len(sys.argv)!=4 or sys.argv[3] not in ("set","list"):
        return 2
    exe,backend,kind=sys.argv[1:]
    sub=subprocess.Popen([exe,"subscriber",backend,kind],
                         stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
    try:
        time.sleep(0.75)
        pub=subprocess.run([exe,"publisher",backend,kind],
                           capture_output=True,text=True,timeout=18)
        try:
            output,_=sub.communicate(timeout=12)
        except subprocess.TimeoutExpired:
            sub.kill()
            output,_=sub.communicate()
        print("publisher:",pub.returncode,pub.stdout,pub.stderr)
        print("subscriber:",sub.returncode,output)
        return int(pub.returncode!=0 or sub.returncode!=0)
    finally:
        if sub.poll() is None:
            sub.kill()
            sub.wait()
if __name__=="__main__":
    sys.exit(main())
