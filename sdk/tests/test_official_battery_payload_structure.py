"""Synthetic source-closure tests for official Battery host structure.

Never ships or transcribes AUVSI raw IDL; all generated source is fixture-only.
"""
from __future__ import annotations

from copy import deepcopy
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.official_icd_evidence import SourceError, sha
from tools.official_battery_payload_structure import (
    DEFAULT_MANIFEST, DEFAULT_SOURCE_PINS, HostProjection, inspect, main,
)


def synthetic_sources(spec: dict) -> dict[str, bytes]:
    """Generate structurally matching but invented IDL from metadata only."""
    sources: dict[str, str] = {p: "" for p in spec["sourceSha256"]}
    by_source: dict[str, list[tuple[str, dict]]] = {}
    for name, struct in spec["structures"].items():
        by_source.setdefault(struct["source"], []).append((name, struct))
    for name, typ in spec["scalarTypedefs"].items():
        suffix = "[" + str(typ["arrayLength"]) + "]" if "arrayLength" in typ else ""
        statement = "typedef " + typ["base"] + " " + name + suffix + ";"
        if "min" in typ:
            statement += " // maxInclusive=" + str(typ["max"]) + " minInclusive=" + str(typ["min"])
        sources[typ["source"]] += statement + "\n"
    for path, entries in by_source.items():
        for fqn, entry in entries:
            name = fqn.split("::")[-1]
            content = ""
            if name.endswith("ReportType") or name.endswith("ListElement"):
                content += 'const string ' + name + 'Topic = "' + fqn + '";\n'
            content += "struct " + name + " {\n"
            for field in entry["fields"]:
                annotation = "@" + field["annotation"] + " " if field["annotation"] else ""
                content += "  " + annotation + field["type"] + " " + field["name"] + ";\n"
            content += "};\n"
            # All target struct IDs in the same source share one namespace.
            pieces = fqn.split("::")[:-1]
            opened = "".join("module " + p + " {\n" for p in pieces)
            source_body = opened + content + "};\n" * len(pieces)
            sources[path] += source_body

    enumeration = spec["enumDefinition"]
    fqn = "UMAA::Common::MaritimeEnumeration::PowerPlantStateEnumModule"
    wrapped = "".join("module " + part + " {\n" for part in fqn.split("::"))
    wrapped += "enum " + enumeration["name"] + " {" + ", ".join(
        enumeration["values"]
    ) + "};\n"
    wrapped += "};\n" * len(fqn.split("::"))
    sources[enumeration["source"]] += wrapped
    return {"LICENSE": b"synthetic IDL fixture ONLY"} | {
        path: body.encode("utf-8") for path, body in sources.items()
    }


class BatteryPayloadStructureTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.archive = self.root / "test-synthetic.zip"
        self.spec = json.loads(DEFAULT_MANIFEST.read_text(encoding="utf-8"))
        self.pins = json.loads(DEFAULT_SOURCE_PINS.read_text(encoding="utf-8"))
        self.members = synthetic_sources(self.spec)
        self.pack()

    def pack(self):
        with zipfile.ZipFile(self.archive, "w") as z:
            for path, body in sorted(self.members.items()):
                z.writestr(path, body)
        for path in self.spec["sourceSha256"]:
            self.spec["sourceSha256"][path] = sha(self.members[path])
        self.spec["idlArchiveSha256"] = sha(self.archive.read_bytes())
        self.pins["sourcePins"]["officialIdlArchiveSha256"] = self.spec["idlArchiveSha256"]
        report = self.spec["structures"]["UMAA::EO::BatteryStatus::BatteryReportType"]
        elem = self.spec["structures"]["UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement"]
        for obj, fqn, record in (
            (report, "UMAA::EO::BatteryStatus::BatteryReportType", self.pins["topics"]["report"]),
            (elem, "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement",
             self.pins["topics"]["collectionElement"]),
        ):
            record["idlPath"] = obj["source"]
            record["idlSha256"] = sha(self.members[obj["source"]])
            record["ddsTopicName"] = fqn
            record["keyFields"] = [x["name"] for x in obj["fields"] if x["annotation"] == "key"]
            record["keyFieldTypes"] = [x["type"] for x in obj["fields"] if x["annotation"] == "key"]
        self.spec_file = self.root / "payload-spec.json"
        self.pins_file = self.root / "pins.json"
        self.write_records()

    def write_records(self):
        self.spec_file.write_text(json.dumps(self.spec, sort_keys=True), encoding="utf-8")
        self.pins_file.write_text(json.dumps(self.pins, sort_keys=True), encoding="utf-8")

    @staticmethod
    def guid(n: int) -> str:
        return f"{n:032x}"

    def report(self):
        return {
            "source": {"id": self.guid(1), "parentID": self.guid(0)},
            "timeStamp": {"seconds": 1700000000, "nanoseconds": 1},
            "state": "ON",
            "chargeRemaining": 42.5,
            "voltage": 24.0,
            "cellsListMetadata": {
                "listID": self.guid(8), "updateElementID": self.guid(9),
                "startingElementID": self.guid(9), "size": 1,
            },
        }

    def element(self):
        return {
            "element": {"current": 5.0, "temperature": 20.5, "voltage": 24.0},
            "listID": self.guid(8), "elementID": self.guid(9),
            "elementTimestamp": {"seconds": 1700000000, "nanoseconds": 42},
        }

    def test_nested_source_structure_and_host_values(self):
        found = inspect(self.archive, self.spec_file, self.pins_file)
        self.assertEqual("SOURCE_CHECKED_UNAPPROVED_BATTERY_PAYLOAD_STRUCTURE", found["kind"])
        self.assertEqual(6, len(found["verifiedStructs"]))
        self.assertEqual(9, len(found["verifiedScalarTypedefs"]))
        self.assertEqual(["FAULT", "OFF", "ON"], found["verifiedStateEnum"]["values"])
        self.assertFalse(found["officialRuntimeBinding"])
        self.assertFalse(found["nativeDdsCodec"])
        self.assertFalse(found["collectionLifecycle"])
        self.assertFalse(found["formalConformance"])
        model = HostProjection(found)
        a = self.report()
        b = self.element()
        self.assertEqual(a, model.validate("UMAA::EO::BatteryStatus::BatteryReportType", a))
        self.assertEqual(b, model.validate(
            "UMAA::EO::BatteryStatus::BatteryReportTypeCellsListElement", b
        ))
        self.assertNotIn("current", model.validate(
            "UMAA::EO::BatteryStatus::BatteryReportType", a
        ))
        a["timeStamp"]["seconds"] = 9223372036854775807
        self.assertEqual(9223372036854775807, model.validate(
            "UMAA::EO::BatteryStatus::BatteryReportType", a
        )["timeStamp"]["seconds"])

    def test_fail_closed_on_nested_values_and_optionals(self):
        model = HostProjection(inspect(self.archive, self.spec_file, self.pins_file))
        type_name = "UMAA::EO::BatteryStatus::BatteryReportType"
        for case in ("missing-state", "unknown", "null", "bad-guid", "bad-state",
                     "numeric-bool", "nan", "bound", "nanos", "seconds",
                     "negative-size", "nested-extra", "optional-unknown"):
            sample = self.report()
            if case == "missing-state":
                del sample["state"]
            elif case == "unknown":
                sample["futurePayload"] = "unreviewed"
            elif case == "null":
                sample["chargeRemaining"] = None
            elif case == "bad-guid":
                sample["source"]["id"] = "AA" * 16
            elif case == "bad-state":
                sample["state"] = "UNKNOWN"
            elif case == "numeric-bool":
                sample["chargeRemaining"] = True
            elif case == "nan":
                sample["voltage"] = float("nan")
            elif case == "bound":
                sample["chargeRemaining"] = 1000.00001
            elif case == "nanos":
                sample["timeStamp"]["nanoseconds"] = 1000000000
            elif case == "seconds":
                sample["timeStamp"]["seconds"] = 9223372036854775808
            elif case == "negative-size":
                sample["cellsListMetadata"]["size"] = -1
            elif case == "nested-extra":
                sample["source"]["junk"] = 42
            elif case == "optional-unknown":
                sample["cellsListMetadata"]["updateElementTimestamp"] = None
            with self.subTest(case=case), self.assertRaises(SourceError):
                model.validate(type_name, sample)
        with self.assertRaises(SourceError):
            model.validate("Demo::EO::BatteryReportType", self.report())

    def test_tampered_source_and_manifest_are_rejected(self):
        original = deepcopy(self.spec)
        actual = inspect(self.archive, self.spec_file, self.pins_file)
        self.assertTrue(actual["allIdlImportsPresent"])
        self.spec["structures"]["UMAA::EO::BatteryStatus::BatteryReportType"]["fields"][0]["annotation"] = "key"
        self.write_records()
        with self.assertRaisesRegex(SourceError, "payload nested field/optional/Key drift"):
            inspect(self.archive, self.spec_file, self.pins_file)
        self.spec = deepcopy(original)
        self.spec["scalarTypedefs"]["EnergyPercent"]["max"] = 500
        self.write_records()
        with self.assertRaisesRegex(SourceError, "scalar IDL range drift"):
            inspect(self.archive, self.spec_file, self.pins_file)
        self.spec = deepcopy(original)
        self.spec["enumDefinition"]["values"] = ["OFF", "ON"]
        self.write_records()
        with self.assertRaisesRegex(SourceError, "Battery state enum drift"):
            inspect(self.archive, self.spec_file, self.pins_file)
        self.spec = deepcopy(original)
        self.spec["readiness"]["officialRuntimeBinding"] = True
        self.write_records()
        with self.assertRaisesRegex(SourceError, "fail-closed"):
            inspect(self.archive, self.spec_file, self.pins_file)
        self.spec = deepcopy(original)
        self.write_records()
        self.members["UMAA/Common/IdentifierType.idl"] += b"\n// TAMPER"
        self.pack()
        # pack() re-pins the source, so explicitly restore the reviewed source pin.
        self.spec["sourceSha256"]["UMAA/Common/IdentifierType.idl"] = original[
            "sourceSha256"
        ]["UMAA/Common/IdentifierType.idl"]
        self.write_records()
        with self.assertRaisesRegex(SourceError, "reviewed IDL member hash mismatch"):
            inspect(self.archive, self.spec_file, self.pins_file)

    def test_cli_host_projection_and_output_collision(self):
        value_file = self.root / "sample.json"
        value_file.write_text(json.dumps(self.report()), encoding="utf-8")
        output = self.root / "out.json"
        args = ["--idl-zip", str(self.archive), "--manifest", str(self.spec_file),
                "--source-pins", str(self.pins_file), "--json", str(value_file),
                "--out", str(output)]
        self.assertEqual(0, main(args))
        obj = json.loads(output.read_text(encoding="utf-8"))
        self.assertFalse(obj["nativeDdsCodec"])
        self.assertEqual(self.report(), obj["hostProjection"])
        self.assertEqual(2, main(args))


if __name__ == "__main__":
    unittest.main()
