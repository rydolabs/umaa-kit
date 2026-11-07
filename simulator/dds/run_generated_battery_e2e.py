#!/usr/bin/env python3
"""Source-gated generated BatteryStatus DLL/SO -> native Cyclone DDS E2E.

Builds only in the caller's temporary workspace. The exact official EO ICD
PDF and original UMAA IDL ZIP are mandatory inputs and are never copied into
the repository. A distinct business-only Handler is installed only into the
generated user's file. This is a scoped host-preview acceptance test, NOT
a general service runtime or complete normative DDS QoS approval.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
COMPONENT = "batterywireclient"
BUSINESS = ROOT / "tests/fixtures/generated_battery_consumer_handler.hpp"


def checked(*args: str, timeout: int = 180) -> None:
    subprocess.run([str(arg) for arg in args], cwd=ROOT, check=True,
                   timeout=timeout)


def build_module(project: Path, prefix: Path, *, clean: bool = False) -> Path:
    build = project / "build"
    if not clean:
        checked("cmake", "-S", str(project), "-B", str(build),
                "-DCMAKE_BUILD_TYPE=Release",
                "-DCMAKE_PREFIX_PATH=" + str(prefix))
    command = ["cmake", "--build", str(build), "--parallel", "2"]
    if clean:
        command.append("--clean-first")
    checked(*command)
    checked("ctest", "--test-dir", str(build), "-R",
            "^component_abi_smoke$", "--output-on-failure")
    module = build / ("lib" + COMPONENT + ".so")
    if not module.is_file():
        raise RuntimeError("generated native MODULE not produced: " + str(module))
    return module


def rejected_default_handler(publisher: Path, host: Path, module: Path) -> None:
    """An unchanged, rejecting generated Handler must NEVER turn wire green."""
    process = subprocess.Popen([
        str(host), "--module", str(module),
        "--until-report-disposes", "2", "--until-element-disposes", "4",
        "--timeout-ms", "18000",
    ], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=ROOT)
    try:
        time.sleep(0.6)
        # The publisher can report a failed ACK if the rejecting Host has
        # already closed its Reader; the Host's exact business refusal is
        # the required negative evidence, never a publisher timeout alone.
        try:
            sent = subprocess.run([str(publisher), "--two-lists"],
                                  capture_output=True, text=True,
                                  cwd=ROOT, timeout=22)
            print(sent.stdout, end="")
            print(sent.stderr, end="", file=sys.stderr)
        except subprocess.TimeoutExpired:
            print("Negative publisher timed out after Host refusal attempt")
        output, _ = process.communicate(timeout=23)
        print(output, end="")
        if (process.returncode == 0 or
                "UMAA_BATTERY_HOST_READY" not in output or
                "business Battery ABI module refused Host event" not in output or
                "UMAA_BATTERY_HOST_STOPPED" in output):
            raise RuntimeError("default user Handler was not rejected by native Host")
        print("UMAA_GENERATED_BATTERY_DEFAULT_HANDLER_REJECTED")
    finally:
        if process.poll() is None:
            process.kill()
            process.communicate()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--idl-zip", type=Path, required=True)
    parser.add_argument("--eo-icd", type=Path, required=True)
    parser.add_argument("--publisher", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args(argv)

    raw_inputs = (args.idl_zip, args.eo_icd, args.publisher, args.host)
    for path in raw_inputs:
        if path.is_symlink() or not path.is_file():
            raise RuntimeError("missing or symlinked original input/binary: " + str(path))
    sources = (args.idl_zip.resolve(), args.eo_icd.resolve())
    publisher, host = args.publisher.resolve(), args.host.resolve()
    work = args.work.absolute()
    if work.exists() or work.is_symlink():
        raise RuntimeError("generated E2E workspace must be new")
    work.mkdir(parents=True)
    project = work / COMPONENT

    def cli(*args_: str) -> None:
        checked(sys.executable, "-m", "tools.umaa_component_cli", *args_)

    cli("new", "component", COMPONENT, "--output", str(project))
    cli("service", "add", "UMAA::EO::BatteryStatus", "--role",
        "consumer", "--project", str(project))
    def source_gate_generate() -> None:
        cli("generate", "--project", str(project), "--battery-host-preview",
            "--idl-archive", str(sources[0]), "--original-icd", str(sources[1]))
    source_gate_generate()

    prefix = work / "sdk-install"
    checked("cmake", "-S", str(ROOT / "sdk"), "-B", str(work / "sdk-build"),
            "-DUMAA_BUILD_TESTS=OFF", "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_INSTALL_PREFIX=" + str(prefix))
    checked("cmake", "--build", str(work / "sdk-build"), "--parallel", "2")
    checked("cmake", "--install", str(work / "sdk-build"))

    # Prove source-generated business entrypoint is fail-closed BEFORE any
    # test fixture overwrites the user's original rejected methods.
    module = build_module(project, prefix)
    rejected_default_handler(publisher, host, module)

    handler = project / "src/handlers/battery_status_consumer.hpp"
    if not handler.is_file() or not BUSINESS.is_file():
        raise RuntimeError("missing user-owned Handler or reviewed test fixture")
    shutil.copyfile(BUSINESS, handler)
    business_bytes = handler.read_bytes()
    # Regeneration must preserve the actual user-owned business implementation.
    source_gate_generate()
    if handler.read_bytes() != business_bytes:
        raise RuntimeError("generator overwrote user-owned Battery Handler")
    module = build_module(project, prefix, clean=True)

    runner = ROOT / "simulator/dds/run_battery_consumer_host.py"
    for flag, marker in (
        ([], "UMAA_BATTERY_HOST_NATIVE_WIRE_PASS"),
        (["--bad-checkpoint"], "UMAA_BATTERY_HOST_BAD_CHECKPOINT_ISOLATED"),
    ):
        result = subprocess.run([
            sys.executable, str(runner), str(publisher), str(host),
            str(module), *flag,
        ], cwd=ROOT, capture_output=True, text=True, timeout=90)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        if result.returncode or marker not in result.stdout:
            raise RuntimeError("generated business Component original DDS " +
                               ("negative" if flag else "positive") + " failed")
    print("UMAA_GENERATED_BATTERY_SOURCE_GATED_NATIVE_E2E_PASS")
    print("NOT_APPROVED: generic official Runtime, full Component QoS, conformance")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, subprocess.CalledProcessError,
            subprocess.TimeoutExpired) as exc:
        print("GENERATED_BATTERY_E2E_ERROR:", exc, file=sys.stderr)
        raise SystemExit(3)
