#!/usr/bin/env python3
"""One actual SDK Host process, separate Provider and Consumer ABI-v2 modules.

The mock DDS plugin keeps isolated Domain objects per open_domain() call:
without the shared Runtime/Domain this test could not deliver a single frame.
Synthetic fixture only, NOT source-approved native UMAA TypeSupport.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def host_run(host, config):
    return subprocess.run(
        [str(host), "--config", str(config), "--duration-ms", "180"],
        capture_output=True, text=True, timeout=30,
    )


def main():
    if len(sys.argv) != 6:
        raise RuntimeError(
            "expected Host, Provider DLL, Consumer DLL, rejecting Consumer DLL, mock backend")
    host, provider, consumer, rejecting, backend = [
        Path(s).resolve() for s in sys.argv[1:]
    ]
    for path in (host, provider, consumer, rejecting, backend):
        if not path.is_file():
            raise AssertionError(f"missing module/Host: {path}")

    with tempfile.TemporaryDirectory() as root:
        config = Path(root) / "host.yml"
        prefix = (
            "middleware:\n"
            f'  backend: "{backend}"\n'
            "  domain_id: 91\n"
        )
        publishers = (
            "report_publishers:\n"
            "  - service: Demo::EO::BatteryStatus\n"
            "    topic: demo/BatteryStatus\n"
            "    schema: demo:battery-v1\n"
            "    qos: demo-reliable\n"
            "    key: source-a\n"
            "    interval_ms: 20\n"
        )
        consumers = (
            "report_consumers:\n"
            "  - service: Demo::EO::BatteryStatus\n"
            "    topic: demo/BatteryStatus\n"
            "    schema: demo:battery-v1\n"
            "    qos: demo-reliable\n"
        )
        modules = lambda sink: (
            "modules:\n"
            f'  - "{provider}"\n'
            f'  - "{sink}"\n'
        )
        valid = prefix + modules(consumer) + publishers + consumers
        config.write_text(valid, encoding="utf-8")
        result = host_run(host, config)
        if result.returncode:
            raise AssertionError(
                f"Host failed Provider→Consumer native ABI E2E: "
                f"{result.stdout}\n{result.stderr}")
        if "UMAA_HOST_READY" not in result.stdout or "consumers=1" not in result.stdout:
            raise AssertionError("Host did not launch the Report Consumer")
        m = re.search(
            r"UMAA_HOST_STOPPED reports=(\d+) disposes=(\d+) active=(\d+)",
            result.stdout,
        )
        if not m or int(m[1]) < 1 or (int(m[2]), int(m[3])) != (1, 0):
            raise AssertionError(
                f"Host did not dispatch real Report and keyed Dispose: {result.stdout}")

        bad = [
            # Must refuse wrong role instead of running a Provider-only Host.
            prefix + modules(provider).replace(
                f'  - "{provider}"\n  - "{provider}"\n',
                f'  - "{provider}"\n') + publishers + consumers,
            prefix + "modules:\n" + f'  - "{consumer}"\n' + publishers + consumers,
            prefix + modules(rejecting) + publishers + consumers,
            valid.replace("report_consumers:", "report_consumers:\n  - service: Demo::EO::BatteryStatus"),
            valid + consumers,
            valid.replace("    qos: demo-reliable\n", "    qos: forged-profile\n"),
            valid.replace("    schema: demo:battery-v1\n",
                          "    schema: original:unreviewed\n"),
            valid.replace("Demo::EO::BatteryStatus", "UMAA::EO::BatteryStatus"),
            valid.replace("    qos: demo-reliable\n", ""),
            valid.replace(
                "report_consumers:\n", "report_consumers:\n  - topic: demo/BatteryStatus\n"),
        ]
        for i, candidate in enumerate(bad):
            config.write_text(candidate, encoding="utf-8")
            denied = host_run(host, config)
            if denied.returncode == 0 or "UMAA_HOST_ERROR:" not in denied.stderr:
                raise AssertionError(
                    f"unsafe binding or business result accepted ({i}): "
                    + repr((denied.returncode, denied.stdout, denied.stderr)))

        # Existing Provider-only configuration is still supported.
        config.write_text(prefix + "modules:\n" + f'  - "{provider}"\n'
                          + publishers, encoding="utf-8")
        provider_only = host_run(host, config)
        if provider_only.returncode or "UMAA_HOST_STOPPED" not in provider_only.stdout:
            raise AssertionError("legacy Publisher-only Host regressed: "
                                 + provider_only.stderr)
    print("PASS one Host/single DDS Domain, separate business DLLs, "
          "Report/Dispose callbacks and fail-closed bindings")


if __name__ == "__main__":
    main()
