#!/usr/bin/env python3
"""Host-OS portable verifier for both GitHub-hosted Windows and Ubuntu runners.

Only verifies the synthetic IDL subset, C++ prototype and subprocess fixture.
It never asserts actual UMAA DDS conformance or support of RTI/Cyclone.
"""
import argparse
import datetime
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "evidence" / "ci" / platform.system().lower()


def run_step(name: str, args: list[str]) -> None:
    print(f"[CI] {name}: {args}", flush=True)
    try:
        p = subprocess.run(args, cwd=ROOT, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, errors="replace", timeout=240)
    except (OSError, subprocess.TimeoutExpired) as exc:
        (EVIDENCE / (name + ".log")).write_text(str(exc), encoding="utf-8")
        raise RuntimeError(f"{name}: {exc}") from exc
    (EVIDENCE / (name + ".log")).write_text(p.stdout, encoding="utf-8")
    print(p.stdout[-3500:], flush=True)
    if p.returncode != 0:
        raise RuntimeError(f"{name} failed with exit code {p.returncode}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", choices=["default", "clang"], default="default")
    args = parser.parse_args()
    global EVIDENCE
    EVIDENCE = EVIDENCE / args.compiler
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    stages = []
    build = ROOT / "build" / ("ci-" + platform.system().lower() + "-" + args.compiler)
    prefix = ROOT / "build" / ("ci-install-" + platform.system().lower() + "-" + args.compiler)
    try:
        tasks = [
            ("reviewed_runtime_registry", [sys.executable, "tools/generate_reviewed_runtime_registry.py", "--check"]),
            ("sdk_generator_unittests", [sys.executable, "-m", "unittest", "discover", "-s", "sdk/tests", "-p", "test_*.py", "-v"]),
            ("simulator_oracle_unittests", [sys.executable, "-m", "unittest", "discover", "-s", "simulator/tests", "-p", "test_*.py", "-v"]),
        ]
        for name, command in tasks:
            run_step(name, command); stages.append(name)
        cmake = ["cmake", "-S", "sdk", "-B", str(build),
                 "-DUMAA_BUILD_TESTS=ON", "-DUMAA_BUILD_MODULE_DEMO=ON"]
        if platform.system() == "Windows":
            cmake += ["-G", "Visual Studio 17 2022", "-A", "x64"]
        else:
            cmake += ["-DCMAKE_BUILD_TYPE=Debug"]
            if args.compiler == "clang":
                cmake += ["-DCMAKE_CXX_COMPILER=clang++"]
        run_step("cmake_configure", cmake); stages.append("cmake_configure")
        run_step("cmake_build", ["cmake", "--build", str(build), "--config", "Debug", "--parallel", "2"]); stages.append("cmake_build")
        run_step("ctest", ["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure"]); stages.append("ctest")
        run_step("cmake_install", ["cmake", "--install", str(build), "--config", "Debug", "--prefix", str(prefix)]); stages.append("cmake_install")
        consumer = ROOT / "build" / ("ci-consumer-" + platform.system().lower() + "-" + args.compiler)
        cfg = ["cmake", "-S", "sdk/tests/consumer", "-B", str(consumer), "-DCMAKE_PREFIX_PATH=" + str(prefix)]
        if platform.system() == "Windows":
            cfg += ["-G", "Visual Studio 17 2022", "-A", "x64"]
        elif args.compiler == "clang":
            cfg += ["-DCMAKE_CXX_COMPILER=clang++"]
        run_step("consumer_configure", cfg); stages.append("consumer_configure")
        run_step("consumer_build", ["cmake", "--build", str(consumer), "--config", "Debug"]); stages.append("consumer_build")
        exe_ext = ".exe" if platform.system() == "Windows" else ""
        consumer_bin = consumer / ("Debug" if platform.system() == "Windows" else "") / ("consumer" + exe_ext)
        run_step("external_consumer_smoke", [str(consumer_bin)]); stages.append("external_consumer_smoke")
        # Optional CycloneDDS CMake export contract. The configuration uses
        # a test-only no-wire CycloneDDSConfig; this does NOT exercise DDS.
        cyclone_fixture = ROOT / "sdk/tests/fake_cyclonedds"
        cyclone_build = ROOT / "build" / ("ci-cyclone-export-" + platform.system().lower() + "-" + args.compiler)
        cyclone_prefix = ROOT / "build" / ("ci-cyclone-install-" + platform.system().lower() + "-" + args.compiler)
        cyclone_cmake = ["cmake", "-S", "sdk", "-B", str(cyclone_build),
                         "-DUMAA_BUILD_TESTS=OFF", "-DUMAA_ENABLE_CYCLONE_ADAPTER=ON",
                         "-DCycloneDDS_DIR=" + str(cyclone_fixture)]
        if platform.system() == "Windows":
            cyclone_cmake += ["-G", "Visual Studio 17 2022", "-A", "x64"]
        elif args.compiler == "clang":
            cyclone_cmake += ["-DCMAKE_CXX_COMPILER=clang++"]
        run_step("cyclone_mock_export_configure", cyclone_cmake); stages.append("cyclone_mock_export_configure")
        run_step("cyclone_mock_export_build", ["cmake", "--build", str(cyclone_build), "--config", "Debug"]); stages.append("cyclone_mock_export_build")
        run_step("cyclone_mock_export_install", ["cmake", "--install", str(cyclone_build),
                                                  "--config", "Debug", "--prefix", str(cyclone_prefix)])
        stages.append("cyclone_mock_export_install")
        cyclone_consumer = ROOT / "build" / ("ci-cyclone-consumer-" + platform.system().lower() + "-" + args.compiler)
        cyclone_external = ["cmake", "-S", "sdk/tests/consumer_cyclone", "-B", str(cyclone_consumer),
                            "-DUMAAPrototype_DIR=" + str(cyclone_prefix / "lib/cmake/UMAAPrototype"),
                            "-DCycloneDDS_DIR=" + str(cyclone_fixture)]
        if platform.system() == "Windows":
            cyclone_external += ["-G", "Visual Studio 17 2022", "-A", "x64"]
        elif args.compiler == "clang":
            cyclone_external += ["-DCMAKE_CXX_COMPILER=clang++"]
        run_step("cyclone_mock_external_configure", cyclone_external); stages.append("cyclone_mock_external_configure")
        run_step("cyclone_mock_external_build", ["cmake", "--build", str(cyclone_consumer),
                                                  "--config", "Debug"]); stages.append("cyclone_mock_external_build")
        cyclone_mock_bin = cyclone_consumer / ("Debug" if platform.system() == "Windows" else "") / ("umaa_external_cyclone_mock" + exe_ext)
        run_step("cyclone_mock_external_smoke", [str(cyclone_mock_bin)]); stages.append("cyclone_mock_external_smoke")
        host = build / ("Debug" if platform.system() == "Windows" else "") / ("umaa_mock_engine_host" + exe_ext)
        scenario_paths = sorted((ROOT / "simulator/scenarios").glob("*.json"))
        for path in scenario_paths:
            command = [sys.executable, "simulator/src/run.py", "--sut", str(host),
                       "--scenario", str(path), "--report", str(EVIDENCE / path.name)]
            run_step("scenario_" + path.stem, command)
            stages.append("scenario_" + path.stem)
        summary = {"status": "PASS", "platform": platform.platform(), "compiler": args.compiler,
                   "timestamp_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
                   "stages": stages, "scenarios": len(scenario_paths),
                   "scope": "SYNTHETIC IDL + C++ PROTOTYPE + STDIO + FAKE DDS CMAKE EXPORT ONLY",
                   "actual_dds_tested": False, "k_mosa_certified": False,
                   "official_umaa_conformance": False}
        (EVIDENCE / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        print("CI PASS: " + str(EVIDENCE / "summary.json"))
        return 0
    except Exception as exc:
        (EVIDENCE / "summary.json").write_text(json.dumps({
            "status": "FAIL", "platform": platform.platform(), "completed_stages": stages,
            "error": str(exc), "actual_dds_tested": False, "official_umaa_conformance": False
        }, indent=2), encoding="utf-8")
        print(f"CI FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
