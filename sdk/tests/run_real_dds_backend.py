"""Independent-process Cyclone DDS plugin smoke; NOT UMAA conformance."""
import subprocess
import sys
import time

def main():
    if len(sys.argv) != 4:
        return 2
    exe, backend, component = sys.argv[1:]
    subscriber = subprocess.Popen([exe, "subscriber", backend, component],
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  text=True)
    try:
        time.sleep(0.75)
        publisher = subprocess.run([exe, "publisher", backend, component],
                                   capture_output=True, text=True, timeout=17)
        try:
            output, _ = subscriber.communicate(timeout=13)
        except subprocess.TimeoutExpired:
            subscriber.kill()
            output, _ = subscriber.communicate()
        print("publisher:", publisher.returncode, publisher.stdout, publisher.stderr)
        print("subscriber:", subscriber.returncode, output)
        return int(publisher.returncode != 0 or subscriber.returncode != 0)
    finally:
        if subscriber.poll() is None:
            subscriber.kill()
            subscriber.wait()

if __name__ == "__main__":
    sys.exit(main())
