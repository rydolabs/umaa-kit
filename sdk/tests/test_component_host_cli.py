#!/usr/bin/env python3
"""Native process Host v1: actual ABI v2 module + in-process mock DDS backend."""
import pathlib
import subprocess
import sys
import tempfile


def invoke(host, config):
    return subprocess.run(
        [host, "--config", str(config), "--duration-ms", "150"],
        capture_output=True, text=True, timeout=25,
    )


def check():
    if len(sys.argv) != 4:
        raise RuntimeError("expected Host, Component DLL/SO, mock DDS backend")
    host, component, backend = [
        str(pathlib.Path(x).resolve()) for x in sys.argv[1:]
    ]
    with tempfile.TemporaryDirectory() as directory:
        config = pathlib.Path(directory) / "host.yml"
        fixture = (
            "middleware:\n"
            f'  backend: "{backend}"\n'
            "  domain_id: 91\n"
            "modules:\n"
            f'  - "{component}"\n'
            "report_publishers:\n"
            "  - service: Demo::EO::BatteryStatus\n"
            "    topic: demo/BatteryStatus\n"
            "    schema: demo:battery-v1\n"
            "    qos: demo-reliable\n"
            "    key: vehicle-a\n"
            "    interval_ms: 25\n"
        )
        config.write_text(fixture, encoding="utf-8")
        result = invoke(host, config)
        if result.returncode or "UMAA_HOST_READY" not in result.stdout or (
                "UMAA_HOST_STOPPED" not in result.stdout):
            raise AssertionError(
                "Host cannot load/run/stop real ABI module: "
                + repr((result.returncode, result.stdout, result.stderr)))
        # Reject fake official contracts and invalid config before any
        # code generation or DDS runtime access is treated as approved.
        rejected = [
            fixture.replace("Demo::EO::BatteryStatus", "UMAA::EO::BatteryStatus"),
            fixture.replace("demo/BatteryStatus", "UMAA::fake/topic"),
            fixture.replace("demo:battery-v1", "fake:official"),
            fixture.replace("    interval_ms: 25", "    interval_ms: 0"),
            fixture.replace("    interval_ms: 25", "    interval_ms: 25\n"
                            "    interval_ms: 50"),
            fixture.replace("    qos: demo-reliable\n", ""),
            fixture + "report_publishers:\n",
            fixture.replace(f'  - "{component}"\n', ""),
            fixture.replace(f'  backend: "{backend}"', "  backend: missing-backend"),
            fixture.replace("    topic: demo/BatteryStatus",
                            "     topic: demo/BatteryStatus"),
        ]
        for index, bad in enumerate(rejected):
            config.write_text(bad, encoding="utf-8")
            result = invoke(host, config)
            if result.returncode == 0 or "UMAA_HOST_ERROR:" not in result.stderr:
                raise AssertionError(
                    f"Host accepted invalid configuration {index}: "
                    + repr((result.returncode, result.stdout, result.stderr)))
        config.write_text(fixture, encoding="utf-8")
        result = subprocess.run(
            [host, "--config", str(config), "--duration-ms", "0"],
            capture_output=True, text=True, timeout=5,
        )
        if result.returncode == 0:
            raise AssertionError("Host accepted invalid test duration")
        # Standard SDK binary has no official generated TypeSupport, and
        # must refuse native factory selection even with plausible source ID.
        denied_native = subprocess.run(
            [host, "--native-binding",
             "UMAA::EO::BatteryStatus/cyclone/original-6.0-host-preview-v1",
             "--module", component],
            capture_output=True, text=True, timeout=5,
        )
        if denied_native.returncode == 0 or (
                "native source binding not compiled" not in denied_native.stderr):
            raise AssertionError("default SDK Host exposed unauthorized native factory")
    print("PASS process Host starts/runs/stops ABI-v2 Report Component "
          "with DDS backend and rejects unapproved source")


if __name__ == "__main__":
    check()
