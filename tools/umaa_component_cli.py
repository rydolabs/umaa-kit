#!/usr/bin/env python3
"""Project-level CLI shared by future React Flow Studio.

Initial verified output targets the synthetic Demo::EO ABI-v2 fixture only.
A literal official service catalog is not a verified official runtime binding.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from tools import umaa_service_contracts as contracts

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "catalog/umaa6-six-icd-reviewed.json"
MANIFEST = "umaa.component.json"
MARK = "// UMAA-COMPONENT-GENERATED: DO NOT EDIT"
CMAKE_MARK = "# UMAA-COMPONENT-GENERATED: DO NOT EDIT"
IDENT = re.compile(r"^[a-z][a-z0-9_]{0,62}$")
DEMO = {
    "Demo::EO::EngineControl": ("EngineControl", "demo:engine-v1", "command"),
    "Demo::EO::BatteryStatus": ("BatteryStatus", "demo:battery-v1", "report"),
}


class ProjectError(ValueError):
    pass


def validate(doc, *, nonempty=False):
    if not isinstance(doc, dict) or not {"schemaVersion", "component", "services"} <= set(doc) or set(doc) - {"schemaVersion", "component", "services", "catalogPin"}:
        raise ProjectError("invalid component manifest fields")
    if doc["schemaVersion"] != "1":
        raise ProjectError("unsupported manifest version")
    c = doc["component"]
    if not isinstance(c, dict) or set(c) != {"id", "artifact"} or not isinstance(c["id"], str) or not IDENT.fullmatch(c["id"]):
        raise ProjectError("invalid component id (expected lowercase C++ identifier)")
    if c["artifact"] != "shared-library":
        raise ProjectError("only shared-library artifact supported")
    if "catalogPin" in doc:
        pin = doc["catalogPin"]
        if (not isinstance(pin, dict) or set(pin) != {"kind", "sha256"}
                or pin["kind"] != "reviewed-icd-catalog"
                or not isinstance(pin["sha256"], str)
                or not re.fullmatch(r"[0-9a-f]{64}", pin["sha256"])):
            raise ProjectError("invalid versioned catalog pin")
    items = doc["services"]
    if not isinstance(items, list) or len(items) > 128 or (nonempty and not items):
        raise ProjectError("expected 1..128 services for generation")
    seen = set()
    for item in items:
        if not isinstance(item, dict) or set(item) != {"id", "role"}:
            raise ProjectError("service must contain exactly id and role")
        name, role = item["id"], item["role"]
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(?:::[A-Za-z][A-Za-z0-9_]*){2,5}", name):
            raise ProjectError("invalid fully qualified service id")
        if role not in ("provider", "consumer") or (name, role) in seen:
            raise ProjectError("invalid or duplicate service role")
        seen.add((name, role))
    return doc


def read(project):
    p = project / MANIFEST
    if p.is_symlink() or not p.is_file():
        raise ProjectError("manifest missing or symlinked")
    try:
        return validate(json.loads(p.read_text(encoding="utf-8")))
    except json.JSONDecodeError as ex:
        raise ProjectError("malformed manifest JSON") from ex


def save(project, doc):
    validate(doc)
    p = project / MANIFEST
    if p.is_symlink():
        raise ProjectError("refusing symlinked manifest")
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=project,
                                     prefix=".umaa-", delete=False) as out:
        temp = Path(out.name)
        json.dump(doc, out, indent=2)
        out.write("\n")
    try:
        os.replace(temp, p)
    finally:
        temp.unlink(missing_ok=True)


def new(name, target):
    if not IDENT.fullmatch(name):
        raise ProjectError("component id must match [a-z][a-z0-9_]{0,62}")
    if target.exists() or target.is_symlink():
        raise ProjectError("destination already exists")
    target.mkdir(parents=True)
    save(target, {"schemaVersion": "1", "component": {
        "id": name, "artifact": "shared-library"}, "services": []})


def check_project(project):
    """Shared CLI/Studio assembly gate, separate from *runtime* readiness."""
    doc = read(project)
    official = [s["id"] for s in doc["services"] if s["id"] not in DEMO]
    ready = all(s["role"] == "provider" and s["id"] in DEMO for s in doc["services"])
    current_pin = None
    if official:
        try:
            names, current_pin = contracts.catalog_names()
        except contracts.ContractError as exc:
            raise ProjectError(str(exc)) from exc
        if any(name not in names for name in official):
            raise ProjectError("assembly references unknown official service")
        declared = doc.get("catalogPin", {}).get("sha256")
        if not declared:
            raise ProjectError("unpinned official service assembly: explicit catalog pin required")
        if declared != current_pin:
            raise ProjectError("reviewed catalog changed: explicit contract revision is required")
    elif "catalogPin" in doc:
        _, current_pin = contracts.catalog_names()
        if doc["catalogPin"]["sha256"] != current_pin:
            raise ProjectError("reviewed catalog changed: explicit contract revision is required")
    return {
        "status": "VALID_ASSEMBLY_NOT_NORMATIVE_RUNTIME",
        "componentId": doc["component"]["id"],
        "services": len(doc["services"]),
        "catalogPinned": not official or doc.get("catalogPin", {}).get("sha256") == current_pin,
        "codegenReady": bool(doc["services"]) and ready,
        "officialRuntimeClaim": False,
        "normativeQosClaim": False,
    }


def add(project, name, role):
    doc = read(project)
    if name not in DEMO:
        try:
            names, digest = contracts.catalog_names()
        except contracts.ContractError as exc:
            raise ProjectError(str(exc)) from exc
        if name not in names:
            raise ProjectError("unknown service or invalid reviewed catalog")
        existing = doc.get("catalogPin", {}).get("sha256")
        if existing and existing != digest:
            raise ProjectError("reviewed catalog changed: explicit contract revision is required")
        doc["catalogPin"] = {"kind": "reviewed-icd-catalog", "sha256": digest}
    # Other existing official names must also match the pinned contract.
    if any(s["id"] not in DEMO for s in doc["services"]):
        check_project(project)
    doc["services"].append({"id": name, "role": role})
    save(project, doc)


def write_generated(path, body, mark=MARK):
    path.parent.mkdir(parents=True, exist_ok=True)
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ProjectError("refusing symlink output")
    if path.exists() and not path.read_text(encoding="utf-8").startswith(mark):
        raise ProjectError("refusing overwrite of user-owned file")
    path.write_text(body, encoding="utf-8")


def write_user(path, body):
    path.parent.mkdir(parents=True, exist_ok=True)
    if any(p.is_symlink() for p in (path, *path.parents)):
        raise ProjectError("refusing symlink user file")
    if not path.exists():
        with path.open("x", encoding="utf-8") as f:
            f.write(body)


def contract_header(services):
    out = [MARK, "#pragma once", '#include "umaa/component.hpp"',
           "#include <cstdint>", "#include <cstring>", "#include <vector>",
           "namespace generated {"]
    for service in services:
        short, schema, kind = DEMO[service["id"]]
        out += [f"struct {short} {{",
                f'  static constexpr const char* id = "{service["id"]}";',
                f'  static constexpr const char* schema_id = "{schema}";']
        if kind == "command":
            out += [
                "  struct Command { std::int32_t target_rpm{}; };",
                "  static bool decode_command(const std::uint8_t* p, std::size_t n, Command& c) {",
                "    if (!p || n != sizeof(c.target_rpm)) return false;",
                "    std::memcpy(&c.target_rpm, p, n); return true;",
                "  }"]
        else:
            out += [
                "  struct Report { std::int32_t level_pct{}; };",
                "  static std::vector<std::uint8_t> encode_report(const Report& r) {",
                "    std::vector<std::uint8_t> b(sizeof(r.level_pct));",
                "    std::memcpy(b.data(), &r.level_pct, b.size()); return b;",
                "  }"]
        out += ["};"]
    return "\n".join(out + ["} // namespace generated", ""])


def handler_header(name, kind):
    base = "CommandHandler" if kind == "command" else "ReportSource"
    methods = (
        f"  umaa::Result execute(const generated::{name}::Command&) override {{ return umaa::Result::Rejected; }}\n"
        "  umaa::Result cancel() override { return umaa::Result::Rejected; }\n"
        if kind == "command" else
        f"  generated::{name}::Report read() override {{\n"
        '    throw std::runtime_error("report handler not implemented");\n'
        "  }\n"
    )
    return ("#pragma once\n// USER OWNED: never overwritten on regeneration.\n"
            '#include "service_contracts.hpp"\n#include <stdexcept>\n'
            f"namespace user {{\nclass {name}Logic final : public umaa::{base}<generated::{name}> {{\n"
            f"public:\n{methods}}};\n}}\n")


def generate(project):
    check_project(project)
    doc = validate(read(project), nonempty=True)
    services = doc["services"]
    for item in services:
        if item["role"] != "provider":
            raise ProjectError("consumer module bindings not yet implemented: " + item["id"])
        if item["id"] not in DEMO:
            raise ProjectError("official service needs verified TypeSupport/key/QoS/role binding: " + item["id"])
    name = doc["component"]["id"]
    write_generated(project / "generated/service_contracts.hpp", contract_header(services))
    entries = [MARK, '#include "umaa/component.hpp"']
    for item in services:
        short, schema, kind = DEMO[item["id"]]
        filename = re.sub(r"(?<!^)(?=[A-Z])", "_", short).lower() + ".hpp"
        write_user(project / "src/handlers" / filename, handler_header(short, kind))
        entries.append(f'#include "handlers/{filename}"')
    entries += ["namespace {", "void register_services(umaa::Component& c) {"]
    for item in services:
        short = DEMO[item["id"]][0]
        entries += [f"  static user::{short}Logic h_{short};",
                    f"  c.provide<generated::{short}>(h_{short});"]
    entries += ["}", "}", f'UMAA_EXPORT_COMPONENT_V2("{name}", register_services)', ""]
    write_generated(project / "generated/component_entry.cpp", "\n".join(entries))
    dollar = "$"
    cmake = "\n".join([
        CMAKE_MARK, "cmake_minimum_required(VERSION 3.20)",
        f"project({name} LANGUAGES CXX)",
        "find_package(UMAAPrototype CONFIG REQUIRED)",
        "file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS src/*.cpp)",
        f"add_library({name} MODULE generated/component_entry.cpp {dollar}{{USER_SOURCES}})",
        f"target_compile_features({name} PRIVATE cxx_std_17)",
        f"target_include_directories({name} PRIVATE {dollar}{{CMAKE_CURRENT_SOURCE_DIR}}/generated {dollar}{{CMAKE_CURRENT_SOURCE_DIR}}/src)",
        f"target_link_libraries({name} PRIVATE UMAA::Core)",
        f"if(EXISTS {dollar}{{CMAKE_CURRENT_SOURCE_DIR}}/cmake/user.cmake)",
        f"  include({dollar}{{CMAKE_CURRENT_SOURCE_DIR}}/cmake/user.cmake)",
        "endif()",
        "enable_testing()",
        f"add_executable({name}_smoke tests/module_smoke.cpp)",
        f"target_link_libraries({name}_smoke PRIVATE UMAA::Core {dollar}{{CMAKE_DL_LIBS}})",
        f"add_test(NAME component_abi_smoke COMMAND {name}_smoke $<TARGET_FILE:{name}>)",
        f"install(TARGETS {name} LIBRARY DESTINATION lib RUNTIME DESTINATION bin)",
        ""])
    write_generated(project / "CMakeLists.txt", cmake, CMAKE_MARK)
    expected = [f'    {{"{s["id"]}", "{DEMO[s["id"]][1]}"}},' for s in services]
    smoke = [MARK, '#include "umaa/component_module.hpp"',
             "#include <string>", "#include <utility>", "#include <vector>",
             "int main(int argc, char** argv) {",
             "  if (argc != 2) return 2;",
             "  umaa::ComponentModule module(argv[1]);",
             f'  if (module.name() != "{name}") return 3;',
             "  const auto& got = module.services();",
             f"  if (got.size() != {len(services)}) return 4;",
             "  const std::vector<std::pair<std::string,std::string>> expected = {"]
    smoke += expected + [
        "  };",
        "  for (std::size_t i = 0; i < expected.size(); ++i)",
        "    if (got[i].service != expected[i].first || got[i].schema != expected[i].second) return 5;",
        "  return module.stop() ? 0 : 6;",
        "}", ""]
    write_generated(project / "tests/module_smoke.cpp", "\n".join(smoke))
    write_user(project / "README.md",
               f"# {name}\n\nSynthetic ABI v2 Component development fixture only.\n"
               "Implement business logic in src/handlers; defaults reject or throw.\n"
               "The official UMAA 6.0 wire contract is NOT generated.\n")


def run_cmake(project, prefix):
    if not prefix.is_dir() or not (project / "CMakeLists.txt").is_file():
        raise ProjectError("installed SDK prefix or generated CMake project missing")
    subprocess.run(["cmake", "-S", str(project), "-B", str(project / "build"),
                    "-DCMAKE_PREFIX_PATH=" + str(prefix)], check=True)
    subprocess.run(["cmake", "--build", str(project / "build"), "--config", "Release"], check=True)


def run_test(project):
    subprocess.run(["ctest", "--test-dir", str(project / "build"),
                    "--build-config", "Release", "--output-on-failure"], check=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description="UMAA Component CLI (synthetic ABI v2 proof)")
    commands = parser.add_subparsers(dest="cmd", required=True)
    n = commands.add_parser("new")
    n.add_argument("kind", choices=["component"])
    n.add_argument("name")
    n.add_argument("--output", type=Path)
    s = commands.add_parser("service")
    actions = s.add_subparsers(dest="action", required=True)
    a = actions.add_parser("add")
    a.add_argument("id")
    a.add_argument("--role", required=True, choices=["provider", "consumer"])
    a.add_argument("--project", type=Path, default=Path("."))
    l = actions.add_parser("list")
    l.add_argument("--project", type=Path, default=Path("."))
    validate_cmd = commands.add_parser("validate", help="validate a Component assembly")
    validate_cmd.add_argument("--project", type=Path, default=Path("."))
    ct = commands.add_parser("contract", help="inspect/export reviewed partial Service Contracts")
    ct_actions = ct.add_subparsers(dest="contract_action", required=True)
    export = ct_actions.add_parser("export")
    export.add_argument("--output", required=True, type=Path)
    verify = ct_actions.add_parser("verify")
    verify.add_argument("--input", required=True, type=Path)
    g = commands.add_parser("generate")
    g.add_argument("--project", type=Path, default=Path("."))
    b = commands.add_parser("build")
    b.add_argument("--project", type=Path, default=Path("."))
    b.add_argument("--sdk-prefix", required=True, type=Path)
    t = commands.add_parser("test")
    t.add_argument("--project", type=Path, default=Path("."))
    args = parser.parse_args(argv)
    try:
        if args.cmd == "new":
            new(args.name, args.output or Path(args.name))
        elif args.cmd == "service":
            if args.action == "add":
                add(args.project, args.id, args.role)
            else:
                for item in read(args.project)["services"]:
                    print(item["role"], item["id"])
        elif args.cmd == "validate":
            print(json.dumps(check_project(args.project), sort_keys=True, indent=2))
        elif args.cmd == "contract":
            if args.contract_action == "export":
                snapshot = contracts.create_snapshot()
                contracts.write_snapshot(args.output, snapshot)
                print(json.dumps(contracts.validate_snapshot(snapshot), sort_keys=True))
            else:
                snapshot = contracts.read_snapshot(args.input)
                print(json.dumps(contracts.validate_snapshot(snapshot), sort_keys=True))
        elif args.cmd == "generate":
            generate(args.project)
        elif args.cmd == "build":
            run_cmake(args.project, args.sdk_prefix)
        elif args.cmd == "test":
            run_test(args.project)
    except (ProjectError, contracts.ContractError, OSError, subprocess.CalledProcessError) as exc:
        print("UMAA COMPONENT CLI ERROR:", exc, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
