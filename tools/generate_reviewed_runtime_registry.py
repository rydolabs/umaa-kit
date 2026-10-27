#!/usr/bin/env python3
"""Build the reviewed literal service/Topic registry; never invent UMAA QoS.

This file deliberately does not infer DDS Topic names from IDL type names.
It emits *catalog-level identities*, not normative DDS runtime Topic/QoS keys.
"""
import argparse
import json
from pathlib import Path

HEADER = '''// GENERATED from catalog/umaa6-six-icd-reviewed.json; do not edit.
// This is an ICD literal topic allow-list, NOT Component Definition/QoS proof.
#pragma once
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace umaa::reviewed {
enum class Role { Report, Ack, Status, Command, ExecutionStatus };
struct Topic {
  std::string_view service, name, type;
  Role role;
  bool qos_verified;
};
inline constexpr std::array<Topic, @COUNT@> topics{{
@ENTRIES@
}};
inline constexpr std::size_t service_count = @SERVICES@;

inline const Topic& require_topic(std::string_view service,
     std::string_view topic, std::string_view type, Role role,
     bool require_normative_qos = true) {
  for (const auto& candidate : topics) {
    if (candidate.service == service && candidate.name == topic) {
      if (candidate.type != type || candidate.role != role)
        throw std::invalid_argument("reviewed Topic type/role mismatch");
      if (require_normative_qos && !candidate.qos_verified)
        throw std::logic_error("UMAA Component Definition QoS not yet reviewed");
      return candidate;
    }
  }
  throw std::invalid_argument("Topic missing from reviewed ICD catalog");
}
} // namespace umaa::reviewed
'''

ROLE = {"report": "Report", "ack": "Ack", "status": "Status",
        "command": "Command", "execution_status": "ExecutionStatus"}


def entries(catalog):
    result, names = [], set()
    services = catalog["services"]
    if not services or catalog.get("complete") is not False:
        raise ValueError("only partial, explicitly reviewed catalogs may be generated")
    service_names = set()
    for svc in services:
        name = svc["name"]
        if name in service_names or not svc.get("icd_reviewed", False):
            raise ValueError("duplicate/unreviewed service " + name)
        service_names.add(name)
        for group in ("provider_topics", "consumer_topics"):
            for t in svc.get(group, []):
                topic, typ, role = t["name"], t["type"], t["role"]
                if role not in ROLE or not topic.startswith(name + "::") or not typ.startswith(name + "::"):
                    raise ValueError("ambiguous role/type/topic in " + name)
                if (name, topic) in names:
                    raise ValueError("duplicate Topic " + topic)
                names.add((name, topic))
                if group == "consumer_topics" and role != "command":
                    raise ValueError("unsupported inbound role "+role)
                if group == "provider_topics" and role == "command":
                    raise ValueError("unsupported outbound command")
                # Catalog currently lacks vetted Component Definitions QoS.
                # Do not let generated C++ claim more than source says.
                if svc.get("dds_qos_verified") is not False:
                    # No normative DDS policy has yet been translated into
                    # an applied Cyclone QoS config. A bare catalog flag must
                    # never unlock the production DDS entity path.
                    raise ValueError("normative QoS application/evidence missing for " + name)
                result.append((name, topic, typ, ROLE[role], False))
    return result, len(service_names)


def quote(s):
    return json.dumps(s, ensure_ascii=True)


def generate(catalog):
    records, count = entries(catalog)
    lines = ["  Topic{" + ", ".join((quote(a), quote(b), quote(c),
              "Role::" + role, "true" if qos else "false")) + "},"
             for a, b, c, role, qos in records]
    return (HEADER.replace("@COUNT@", str(len(records)))
                  .replace("@SERVICES@", str(count))
                  .replace("@ENTRIES@", "\n".join(lines)))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--catalog", type=Path, default=Path("catalog/umaa6-six-icd-reviewed.json"))
    p.add_argument("--output", type=Path, default=Path("sdk/include/umaa/reviewed_service_registry.hpp"))
    p.add_argument("--check", action="store_true")
    a = p.parse_args()
    data = generate(json.loads(a.catalog.read_text(encoding="utf-8")))
    if a.check:
        if not a.output.exists() or a.output.read_text(encoding="utf-8") != data:
            p.error("reviewed service registry stale; regenerate from catalog")
    else:
        a.output.parent.mkdir(parents=True, exist_ok=True)
        a.output.write_text(data, encoding="utf-8")
    rows, services = entries(json.loads(a.catalog.read_text(encoding="utf-8")))
    print(f"REVIEWED_SERVICE_REGISTRY_PASS services={services} topics={len(rows)} normative_qos_claim=none")


if __name__ == '__main__':
    main()
