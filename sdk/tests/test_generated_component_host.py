#!/usr/bin/env python3
"""End-to-end generated business Component CMake DLL/SO -> SDK Host process."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools import umaa_component_cli as cli  # noqa: E402


def run(command, **kwargs):
    result = subprocess.run(
        list(map(str, command)), capture_output=True, text=True,
        timeout=180, **kwargs,
    )
    if result.returncode != 0:
        raise AssertionError(
            f"command failed {command}:\n{result.stdout}\n{result.stderr}")
    return result


def locate_module(project, name):
    targets = [
        path for path in (project / "build").rglob("*")
        if path.is_file() and path.suffix.lower() in (".so", ".dll", ".dylib")
        and name in path.stem.lower()
    ]
    if len(targets) != 1:
        raise AssertionError(f"expected one built Component module, got {targets}")
    return targets[0]


def main():
    if len(sys.argv) != 5:
        raise RuntimeError("expected Host, backend, SDK build dir and CTest build configuration")
    host, backend, sdk_build = map(lambda x: Path(x).resolve(), sys.argv[1:4])
    configuration = sys.argv[4]
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        prefix = root / "sdk-installed"
        # CTest can build MSVC in Debug while the generated project is
        # separately compiled in Release. Install the *already built*
        # SDK artifacts, not an imaginary hard-coded Release executable.
        install = ["cmake", "--install", sdk_build]
        if configuration:
            install.extend(["--config", configuration])
        install.extend(["--prefix", prefix])
        run(install)
        project = root / "generated"
        cli.new("hostgenerated", project)
        cli.add(project, "Demo::EO::BatteryStatus", "provider")
        cli.generate(project)

        # Default generated Handler is deliberately unimplemented.
        cli.run_cmake(project, prefix)
        module = locate_module(project, "hostgenerated")
        config = root / "host.yml"
        cli.generate_host_config(
            project, backend, module, config, 91, "vehicle-a", 25)
        denied = subprocess.run(
            [str(host), "--config", str(config), "--duration-ms", "150"],
            capture_output=True, text=True, timeout=30,
        )
        if denied.returncode == 0 or "UMAA_HOST_ERROR:" not in denied.stderr:
            raise AssertionError(
                "unimplemented Handler unexpectedly succeeded: "
                + repr((denied.returncode, denied.stdout, denied.stderr)))

        # The only edit is business logic. No DDS/IDL/CMake/ABI changes.
        handler = project / "src" / "handlers" / "battery_status.hpp"
        body = handler.read_text(encoding="utf-8")
        sentinel = 'throw std::runtime_error("report handler not implemented");'
        if body.count(sentinel) != 1:
            raise AssertionError("generated business Handler contract changed")
        handler.write_text(
            body.replace(sentinel, "return generated::BatteryStatus::Report{73};"),
            encoding="utf-8",
        )
        cli.generate(project)
        if handler.read_text(encoding="utf-8").count(
                "return generated::BatteryStatus::Report{73};") != 1:
            raise AssertionError("regeneration overwrote user business logic")
        cli.run_cmake(project, prefix)
        cli.run_test(project)
        module = locate_module(project, "hostgenerated")
        succeeded = run([host, "--config", config, "--duration-ms", "150"])
        if "UMAA_HOST_READY" not in succeeded.stdout or (
                "UMAA_HOST_STOPPED" not in succeeded.stdout):
            raise AssertionError("Host did not run generated Component: "
                                 + succeeded.stdout)
    print("PASS generated CMake business DLL/SO -> executable Host process "
          "with only read() edited; mock DDS boundary")


if __name__ == "__main__":
    main()
