"""Independent-process synthetic DDS Command request/response round-trip."""
import subprocess
import sys
import time

def main():
    if len(sys.argv) not in (4, 5):
        return 2
    exe, backend, component = sys.argv[1:4]
    client_exe = sys.argv[4] if len(sys.argv) == 5 else exe
    server = subprocess.Popen([exe, "server", backend, component],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True)
    try:
        time.sleep(0.75)
        client = subprocess.run([client_exe, "client", backend, component],
                                capture_output=True, text=True, timeout=32)
        try:
            output, _ = server.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            server.kill()
            output, _ = server.communicate()
        print("client:", client.returncode, client.stdout, client.stderr)
        print("server:", server.returncode, output)
        return int(client.returncode != 0 or server.returncode != 0)
    finally:
        if server.poll() is None:
            server.kill()
            server.wait()

if __name__ == "__main__":
    sys.exit(main())
