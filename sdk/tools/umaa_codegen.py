#!/usr/bin/env python3
"""Fail-closed IDL-subset typed C++ command handler generator.

IDL declarations are parsed, never interpreted as executable instructions. An
external service manifest is required; guessing services from topic names is
unsafe (UMAA EO ICD 3.4 explicitly permits naming exceptions).

This is a working subset/prototype, NOT a full OMG IDL 4.2 parser.
"""
import argparse
import json
import pathlib
import re
import sys
from dataclasses import dataclass


class SchemaError(ValueError):
    pass


@dataclass(frozen=True)
class Field:
    type_name: str
    name: str
    is_key: bool = False


TOKEN = re.compile(r"@key|[A-Za-z_]\w*|\d+|::|[{};<>\[\],]|\S")
CPP_TYPES = {
    "string": "std::string", "boolean": "bool", "float": "float", "double": "double",
    "long": "std::int32_t", "long long": "std::int64_t", "short": "std::int16_t",
    "unsigned long": "std::uint32_t", "unsigned long long": "std::uint64_t",
    "unsigned short": "std::uint16_t", "octet": "std::uint8_t", "char": "char",
}


def parse_idl(contents: str) -> dict[str, list[Field]]:
    contents = re.sub(r"/\*.*?\*/|//[^\n]*", "", contents, flags=re.S)
    toks = TOKEN.findall(contents)
    at = 0
    result: dict[str, list[Field]] = {}

    def pop(wanted=None):
        nonlocal at
        if at >= len(toks):
            raise SchemaError(f"Unexpected end of IDL; expected {wanted or 'token'}")
        item = toks[at]; at += 1
        if wanted is not None and item != wanted:
            raise SchemaError(f"Expected {wanted}, got {item} at token {at}")
        return item

    def identifier():
        value = pop()
        if not re.fullmatch(r"[A-Za-z_]\w*", value):
            raise SchemaError(f"Invalid identifier {value}")
        return value

    def parse_scope(prefix=(), nested=False):
        nonlocal at
        while at < len(toks) and toks[at] != "}":
            token = pop()
            if token == "module":
                name = identifier()
                pop("{"); parse_scope(prefix + (name,), True)
                pop("}"); pop(";")
            elif token == "struct":
                name = identifier(); pop("{")
                fields = []
                while toks[at] != "}":
                    is_key = False
                    if toks[at] == "@key":
                        pop(); is_key = True
                    t = identifier()
                    if t in ("unsigned", "long") and at < len(toks) and toks[at] in ("long", "short"):
                        t += " " + pop()
                        if t == "unsigned long" and at < len(toks) and toks[at] == "long":
                            t += " " + pop()
                    if t not in CPP_TYPES:
                        raise SchemaError(f"Unsupported IDL type {t}; refusing guessed mapping")
                    fname = identifier()
                    pop(";")
                    fields.append(Field(t, fname, is_key))
                pop("}"); pop(";")
                fq = "::".join(prefix + (name,))
                if fq in result: raise SchemaError(f"Duplicate IDL type {fq}")
                result[fq] = fields
            else:
                raise SchemaError(f"Unsupported IDL declaration '{token}' at token {at}; no silent skip")
        if nested and at >= len(toks): raise SchemaError("Unclosed module")

    parse_scope()
    if at != len(toks): raise SchemaError("Unmatched closing brace")
    if not result: raise SchemaError("No structs found")
    return result


def load_service(catalog_path: pathlib.Path, service: str, types: dict):
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    items = [v for v in catalog.get("services", []) if v.get("name") == service]
    if len(items) != 1: raise SchemaError(f"Exactly one service manifest expected: {service}")
    item = items[0]
    fq_name = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z_][A-Za-z0-9_]*)+$")
    if not isinstance(item.get("name"), str) or not fq_name.fullmatch(item["name"]):
        raise SchemaError("Invalid fully-qualified service name")
    kind = item.get("kind")
    if kind not in ("command", "report"):
        raise SchemaError(f"Unsupported service kind {kind}; refusing guessed mapping")
    type_key = "command_type" if kind == "command" else "report_type"
    if item.get(type_key) not in types:
        raise SchemaError(f"IDL does not contain {item.get(type_key)}")
    required_fields = ["command_topic", "status_topic", "ack_topic"] if kind == "command" else ["report_topic"]
    for required in required_fields:
        if not item.get(required): raise SchemaError(f"Missing explicit {required}")
        if not isinstance(item[required], str) or not fq_name.fullmatch(item[required]):
            raise SchemaError(f"Invalid fully-qualified {required}")
    return item


def render_types(types: dict) -> str:
    lines = ["// GENERATED from input IDL subset - DO NOT EDIT", "#pragma once", "#include <string>", "#include <cstdint>", ""]
    for fq, fields in sorted(types.items()):
        *namespaces, name = fq.split("::")
        for ns in namespaces: lines.append(f"namespace {ns} {{")
        lines.append(f"struct {name} {{")
        for f in fields:
            lines.append(f"  {CPP_TYPES[f.type_name]} {f.name}{{}};" + ("  // IDL @key" if f.is_key else ""))
        lines.append("};")
        for ns in reversed(namespaces): lines.append(f"}}  // namespace {ns}")
    return "\n".join(lines) + "\n"


def generate(idl: pathlib.Path, catalog: pathlib.Path, service: str, output: pathlib.Path):
    types = parse_idl(idl.read_text(encoding="utf-8"))
    item = load_service(catalog, service, types)
    short = service.split("::")[-1]
    if not re.fullmatch(r"[A-Za-z_]\w*", short): raise SchemaError("Unsafe service identifier")
    types_file = output / "include" / "umaa" / "generated" / "types.hpp"
    service_file = types_file.parent / f"{short}.hpp"
    user_file = output / "user" / f"My{short}.hpp"
    for path in (types_file, service_file, user_file): path.parent.mkdir(parents=True, exist_ok=True)
    # Atomic not required: process dies on first invalid input before any writes.
    types_file.write_text(render_types(types), encoding="utf-8")
    if item["kind"] == "command":
        typedef = item["command_type"]
        service_file.write_text(f"""// GENERATED - DO NOT EDIT. UMAA service mapping comes from the manifest.
#pragma once
#include \"umaa/command_engine.hpp\"
#include \"umaa/generated/types.hpp\"
namespace umaa::generated {{
using {short}Payload = {typedef};
class {short}Handler : public umaa::Handler<{short}Payload> {{
public:
  virtual ~{short}Handler() = default;
  virtual umaa::Outcome execute(const {short}Payload& cmd) override = 0;
  virtual bool cancel(const {short}Payload& cmd) override = 0;
}};
using {short}Provider = umaa::CommandEngine<{short}Payload>;
struct {short}ServiceDescriptor {{
  static constexpr const char* name = "{item['name']}";
  static constexpr const char* command_topic = "{item['command_topic']}";
  static constexpr const char* status_topic = "{item['status_topic']}";
  static constexpr const char* ack_topic = "{item['ack_topic']}";
}};
}}  // namespace umaa::generated
""", encoding="utf-8")
        if not user_file.exists():
            user_file.write_text(f"""// USER FILE: will NOT be overwritten when regenerating.
#pragma once
#include \"umaa/generated/{short}.hpp\"
class My{short} final : public umaa::generated::{short}Handler {{
public:
  umaa::Outcome execute(const umaa::generated::{short}Payload& cmd) override {{
    (void)cmd;
    // TODO implement device/business action, not UMAA status management.
    return umaa::Outcome::Pending;
  }}
  bool cancel(const umaa::generated::{short}Payload& cmd) override {{
    (void)cmd;
    // TODO return true ONLY when the device can actually be safely canceled.
    return false;
  }}
}};
""", encoding="utf-8")
    else:
        typedef = item["report_type"]
        service_file.write_text(f"""// GENERATED - DO NOT EDIT. Report-by-subscription, no synthetic RPC.
#pragma once
#include \"umaa/report_engine.hpp\"
#include \"umaa/generated/types.hpp\"
namespace umaa::generated {{
using {short}Payload = {typedef};
class {short}Handler : public umaa::ReportHandler<{short}Payload> {{
public:
  virtual ~{short}Handler() = default;
  virtual {short}Payload read_report() const override = 0;
}};
using {short}Provider = umaa::ReportProvider<{short}Payload>;
using {short}ConsumerCache = umaa::ReportCache<{short}Payload>;
using {short}ProviderLifecycle = umaa::ReportProviderLifecycle<{short}Payload>;
using {short}ConsumerLifecycle = umaa::ReportConsumerLifecycle<{short}Payload>;
struct {short}ServiceDescriptor {{
  static constexpr const char* name = "{item['name']}";
  static constexpr const char* report_topic = "{item['report_topic']}";
}};
}}  // namespace umaa::generated
""", encoding="utf-8")
        if not user_file.exists():
            user_file.write_text(f"""// USER FILE: will NOT be overwritten when regenerating.
#pragma once
#include \"umaa/generated/{short}.hpp\"
class My{short} final : public umaa::generated::{short}Handler {{
public:
  umaa::generated::{short}Payload read_report() const override {{
    // TODO: supply your current device measurements.
    return {{}};
  }}
}};
""", encoding="utf-8")
    metadata = {"idl_input": idl.name, "schema_version": json.loads(catalog.read_text()).get("schema_version"), "service": item, "disclaimer": "Prototype typed bindings only, not generated DDS TypeSupport"}
    (output / "generation.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return [types_file, service_file, user_file]


def main(argv=None):
    parser = argparse.ArgumentParser(description="Prototype UMAA typed C++ user Handler generator")
    p = parser.add_subparsers(dest="command", required=True)
    g = p.add_parser("generate"); g.add_argument("--idl", type=pathlib.Path, required=True); g.add_argument("--catalog", type=pathlib.Path, required=True); g.add_argument("--service", required=True); g.add_argument("--out", type=pathlib.Path, required=True)
    i = p.add_parser("inspect"); i.add_argument("--idl", type=pathlib.Path, required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "inspect":
            print(json.dumps({n: [f.__dict__ for f in v] for n, v in parse_idl(args.idl.read_text()).items()}, indent=2))
        else:
            for path in generate(args.idl, args.catalog, args.service, args.out): print(path)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as exc:
        print(f"SCHEMA_ERROR: {exc}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__": sys.exit(main())
