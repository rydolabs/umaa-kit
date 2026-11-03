"""Independent-process, same-binary SDK v1 Runtime + genuine DDS adapters.
Synthetic IDL fixtures only; NOT official UMAA 6.0 wire conformance.
"""
import subprocess
import sys
import time


def main():
    if len(sys.argv) != 6:
        print("usage: runner <cli> <command-backend> <report-backend> <component> <normal|cancel|update>")
        return 2
    cli, command_backend, report_backend, component, scenario = sys.argv[1:]
    if scenario not in ("normal", "cancel", "update"):
        return 2
    args = [cli, command_backend, report_backend, component, scenario]
    server = subprocess.Popen([cli, "server", *args[1:]],
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True)
    try:
        time.sleep(0.85)
        client = subprocess.run([cli, "client", *args[1:]],
                                capture_output=True, text=True, timeout=40)
        try:
            output, _ = server.communicate(timeout=25)
        except subprocess.TimeoutExpired:
            server.kill()
            output, _ = server.communicate(timeout=5)
        print(f"client ({scenario}):", client.returncode, client.stdout, client.stderr, flush=True)
        print(f"server ({scenario}):", server.returncode, output, flush=True)
        assert_client = "SDK_V1_CLIENT scenario=" + scenario
        assert_server = "SDK_V1_SERVER scenario=" + scenario
        return int(client.returncode != 0 or server.returncode != 0 or
                   assert_client not in client.stdout or assert_server not in output or
                   "report_data=1 report_dispose=1" not in client.stdout or
                   "report_published=1" not in output)
    finally:
        if server.poll() is None:
            server.kill()
            server.wait(timeout=5)


if __name__ == "__main__":
    sys.exit(main())
