#!/usr/bin/env python3
"""Generate source-pinned, business-only EO BatteryStatus Consumer MODULE.

This is an SDK host-local *preview* of a typed consumer DLL/SO, not the
official CDR codec, DDS endpoint generator, or approved runtime.
Original source files are read privately and never copied to the output.
"""
from __future__ import annotations

import json
from pathlib import Path

from tools import umaa_component_cli as component
from tools import umaa_eo_original_evidence as evidence

SERVICE = "UMAA::EO::BatteryStatus"
PROFILE = "official-battery-host-preview-consumer-v1"


def _quoted(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def _exact_record(original_idl_zip: Path, original_eo_icd: Path) -> dict:
    if original_idl_zip is None or original_eo_icd is None:
        raise component.ProjectError(
            "Battery host preview requires BOTH original IDL ZIP and EO ICD PDF")
    records = evidence.verify_originals(
        original_idl_zip, original_eo_icd, {SERVICE})
    reviewed = evidence.load_record(SERVICE)
    if records != {SERVICE: reviewed}:
        raise component.ProjectError("original Battery source verification changed")
    return reviewed


def _binding_header(record: dict) -> str:
    topics = record["topics"]
    elements = record["collectionTopics"]
    operations = record["operations"]
    if len(topics) != 1 or len(elements) != 1 or len(operations) != 2:
        raise component.ProjectError("unsupported official Battery preview contract")
    report, element = topics[0], elements[0]
    if (report["keyFields"] != ["source"] or
        element["keyFields"] != ["listID", "elementID"]):
        raise component.ProjectError("Battery host preview Key contract changed")
    operations = sorted(operations, key=lambda op: op["name"])
    op_rows = [
        "    Operation{" + ", ".join(_quoted(v) for v in
            (op["name"], op["topicId"], op["action"], op["actor"])) + "},"
        for op in operations
    ]
    return "\n".join([
        component.MARK,
        "#pragma once",
        '#include "umaa/official_battery_component_bridge.hpp"',
        "#include <array>",
        "#include <string_view>",
        "namespace generated {",
        "using BatteryStatus = ::umaa::official_battery::BatteryStatusHostPreview;",
        "struct BatteryStatusBinding final {",
        '  static constexpr std::string_view profile = ' + _quoted(PROFILE) + ";",
        '  static constexpr std::string_view service = ' + _quoted(SERVICE) + ";",
        '  static constexpr std::string_view originalEoIcdSha256 = ' +
            _quoted(record["sources"]["icd"]["sha256"]) + ";",
        '  static constexpr std::string_view originalIdlArchiveSha256 = ' +
            _quoted(record["sources"]["idlArchive"]["sha256"]) + ";",
        '  static constexpr std::string_view hostOnlySchema = BatteryStatus::schema_id;',
        '  static constexpr std::string_view reportTopic = ' +
            _quoted(report["ddsTopicName"]) + ";",
        '  static constexpr std::string_view reportType = ' +
            _quoted(report["idlType"]) + ";",
        '  static constexpr std::string_view collectionElementTopic = ' +
            _quoted(element["ddsTopicName"]) + ";",
        '  static constexpr std::string_view collectionElementType = ' +
            _quoted(element["idlType"]) + ";",
        '  static constexpr std::array<std::string_view, 1> reportKeys{{"source"}};',
        '  static constexpr std::array<std::string_view, 2> collectionElementKeys{{"listID", "elementID"}};',
        '  static constexpr bool collectionElementTopicOrderedMinimum = true;',
        '  static constexpr bool nativeDdsQosVerified = false;',
        '  static constexpr bool nativeDdsRuntimeApproved = false;',
        '  static constexpr bool hostPreviewOnly = true;',
        "  struct Operation {",
        "    std::string_view name, topic, action, actor;",
        "  };",
        '  static constexpr std::array<Operation, 2> operations{{',
        *op_rows,
        "  }};",
        "};",
        "} // namespace generated",
        "",
    ])


def _handler_header() -> str:
    return "\n".join([
        "#pragma once",
        "// USER OWNED: never overwritten by regeneration.",
        "// Host-local StableList, NOT DDS CDR. Implement business logic only.",
        '#include "service_binding.hpp"',
        "#include <cstdint>",
        "namespace user {",
        "class BatteryStatusConsumerLogic final",
        "    : public ::umaa::ReportSink<generated::BatteryStatus> {",
        "public:",
        "  ::umaa::Result on_report(const ::umaa::official_battery::StableList&,",
        "                           std::uint64_t) override {",
        "    return ::umaa::Result::Rejected;",
        "  }",
        "  ::umaa::Result on_dispose(std::uint64_t) override {",
        "    return ::umaa::Result::Rejected;",
        "  }",
        "};",
        "} // namespace user",
        "",
    ])


def _module_cpp(module: str) -> str:
    return "\n".join([
        component.MARK,
        '#include "umaa/component.hpp"',
        '#include "handlers/battery_status_consumer.hpp"',
        "namespace {",
        "void register_services(::umaa::Component& c) {",
        "  static user::BatteryStatusConsumerLogic handler;",
        "  c.consume<generated::BatteryStatus>(handler);",
        "}",
        "}",
        'UMAA_EXPORT_COMPONENT_V2(' + _quoted(module) + ', register_services)',
        "",
    ])


def _smoke(module: str) -> str:
    return "\n".join([
        component.MARK,
        '#include "umaa/component_module.hpp"',
        '#include "service_binding.hpp"',
        "#include <cstdint>",
        "#include <string>",
        "#include <vector>",
        "#include <stdexcept>",
        "#include <iostream>",
        "int main(int argc, char** argv) {",
        "  if(argc!=2)return 2;",
        "  using B=generated::BatteryStatusBinding;",
        "  static_assert(B::hostPreviewOnly && !B::nativeDdsRuntimeApproved,",
        '    "preview MUST NOT approve official native DDS runtime");',
        "  static_assert(!B::nativeDdsQosVerified,",
        '    "source minimum is NOT full mandatory DDS QoS");',
        "  static_assert(B::operations.size()==2,",
        '    "expected exactly two reviewed Battery operations");',
        "  ::umaa::ComponentModule component(argv[1]);",
        "  if(component.name()!=" + _quoted(module) + ")return 3;",
        "  const auto& services=component.services();",
        "  if(services.size()!=1 || services[0].service!=B::service ||",
        "     services[0].schema!=B::hostOnlySchema || services[0].actions!=24u)",
        "    return 4;",
        "  ::umaa::official_battery::StableList value{};",
        "  value.report.source.id[0]=0x33;",
        "  value.report.cellsListMetadata.listID[0]=0x11;",
        "  value.report.state=::umaa::official_battery::State::ON;",
        "  value.report.cellsListMetadata.size=0;",
        "  value.generation=1;",
        "  const auto payload=generated::BatteryStatus::encode_report(value);",
        "  std::vector<std::uint8_t> out;",
        "  std::int32_t business=-1;",
        "  if(component.call(std::string(B::service),",
        "       UMAA_COMPONENT_REPORT_ACCEPT, payload, out, business, 42)",
        "       !=UMAA_COMPONENT_OK ||",
        "     business<0 ||",
        "     business>static_cast<std::int32_t>(::umaa::Result::Failed))",
        "    return 5;",
        "  if(component.call(std::string(B::service),",
        "       UMAA_COMPONENT_REPORT_DISPOSE, {}, out, business, 42)",
        "       !=UMAA_COMPONENT_OK ||",
        "     business<0 ||",
        "     business>static_cast<std::int32_t>(::umaa::Result::Failed))",
        "    return 6;",
        "  if(component.call(std::string(B::service),",
        "       UMAA_COMPONENT_REPORT_ACCEPT, payload, out, business, 0)",
        "       !=UMAA_COMPONENT_INVALID) return 7;",
        "  if(component.call(std::string(B::service),",
        "       UMAA_COMPONENT_REPORT_ACCEPT, {1u,2u}, out, business, 42)",
        "       !=UMAA_COMPONENT_INVALID) return 8;",
        "  if(!component.stop())return 9;",
        '  std::cout<<"PASS Battery host-preview Component ABI v2; user outcomes remain user-owned"<<"\\n";',
        "  return 0;",
        "}",
        "",
    ])


def _cmake(module: str) -> str:
    dollar = "$"
    return "\n".join([
        component.CMAKE_MARK,
        "cmake_minimum_required(VERSION 3.20)",
        "project(" + module + " LANGUAGES CXX)",
        "find_package(UMAAPrototype CONFIG REQUIRED)",
        "file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS src/*.cpp)",
        "add_library(" + module + " MODULE generated/component_entry.cpp " +
            dollar + "{USER_SOURCES})",
        "target_compile_features(" + module + " PRIVATE cxx_std_17)",
        "target_include_directories(" + module + " PRIVATE " +
            dollar + "{CMAKE_CURRENT_SOURCE_DIR}/generated " +
            dollar + "{CMAKE_CURRENT_SOURCE_DIR}/src)",
        "target_link_libraries(" + module + " PRIVATE UMAA::Core)",
        "if(EXISTS " + dollar + "{CMAKE_CURRENT_SOURCE_DIR}/cmake/user.cmake)",
        "  include(" + dollar + "{CMAKE_CURRENT_SOURCE_DIR}/cmake/user.cmake)",
        "endif()",
        "enable_testing()",
        "add_executable(" + module + "_smoke tests/module_smoke.cpp)",
        "target_compile_features(" + module + "_smoke PRIVATE cxx_std_17)",
        "target_include_directories(" + module + "_smoke PRIVATE " +
            dollar + "{CMAKE_CURRENT_SOURCE_DIR}/generated)",
        "target_link_libraries(" + module + "_smoke PRIVATE UMAA::Core " +
            dollar + "{CMAKE_DL_LIBS})",
        "add_test(NAME component_abi_smoke COMMAND " + module +
            "_smoke $<TARGET_FILE:" + module + ">)",
        "install(TARGETS " + module + " LIBRARY DESTINATION lib RUNTIME DESTINATION bin)",
        "",
    ])


def generate(project: Path, original_idl_zip: Path, original_eo_icd: Path) -> dict:
    """Explicit Consumer-only module generation from the two exact originals."""
    document = component.validate(component.read(project), nonempty=True)
    selected = document["services"]
    if selected != [{"id": SERVICE, "role": "consumer"}]:
        raise component.ProjectError(
            "Battery host-preview generator supports exactly one BatteryStatus consumer")
    if document.get("catalogPin", {}).get("kind") != "reviewed-icd-catalog":
        raise component.ProjectError("original Battery Consumer requires catalog pin")
    # The project's reviewed Catalog pin must be current. This is NOT a
    # Component Definition membership or QoS approval.
    component.check_assembly(document)
    record = _exact_record(original_idl_zip, original_eo_icd)
    module = document["component"]["id"]

    # All original source checks complete before any output is created.
    component.write_generated(project / "generated/service_binding.hpp",
                              _binding_header(record))
    component.write_user(project / "src/handlers/battery_status_consumer.hpp",
                         _handler_header())
    component.write_generated(project / "generated/component_entry.cpp",
                              _module_cpp(module))
    component.write_generated(project / "tests/module_smoke.cpp",
                              _smoke(module))
    component.write_generated(project / "CMakeLists.txt",
                              _cmake(module), component.CMAKE_MARK)
    component.write_user(project / "README.md",
        "# " + module + "\n\n"
        "Development-only UMAA EO BatteryStatus **Consumer Host Preview**.\n"
        "The only user code is in src/handlers/battery_status_consumer.hpp.\n"
        "The default Handler rejects all Report/Dispose calls until implemented.\n"
        "The data type is a *host-local joined StableList*, not DDS CDR.\n"
        "The original ICD/IDL were SHA-verified when generated, but official\n"
        "DDS QoS, Component membership and native Runtime approval are OPEN.\n"
        "Use an independently verified native Battery Reader/Bridge to deliver\n"
        "events; do NOT load this module into the synthetic-only Report Host.\n")
    return {
        "kind": "UMAA_BATTERY_HOST_PREVIEW_COMPONENT",
        "profile": PROFILE,
        "componentId": module,
        "serviceId": SERVICE,
        "role": "consumer",
        "source": {
            "originalEoIcdSha256": record["sources"]["icd"]["sha256"],
            "originalIdlArchiveSha256": record["sources"]["idlArchive"]["sha256"],
            "reportIdlSha256": record["topics"][0]["idlSha256"],
        },
        "businessHandler": "src/handlers/battery_status_consumer.hpp",
        "buildableModule": True,
        "hostOnlyCodec": True,
        "nativeDdsRuntimeApproved": False,
        "mandatoryQosVerified": False,
        "officialConformanceClaim": False,
    }
