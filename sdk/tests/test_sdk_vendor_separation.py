"""Architecture guard: SDK application+protocol headers must be vendor-neutral.

Vendor SDK headers are restricted to dds-backends/. Tests may use
explicit test-only DDS shims; SDK public headers remain vendor neutral.
"""
from pathlib import Path
import re
import unittest

SDK = Path(__file__).resolve().parents[1]
ROOT = SDK.parent
PUBLIC = SDK / "include" / "umaa"
FORBIDDEN = re.compile(r'#\s*include\s*[<"](?:dds/dds\.h|fastdds/|fastrtps/|ndds/|rti/|dds/core/)', re.I)


class VendorBoundaryTests(unittest.TestCase):
    def test_unified_backend_layout(self):
        self.assertFalse((ROOT / "dds").exists(),
                         "legacy dds/ vendor and integration trees must not return")
        for backend in ("cyclone", "fastdds", "rti"):
            self.assertTrue((ROOT / "dds-backends" / backend).is_dir())
        for backend in ("cyclone", "fastdds"):
            self.assertTrue((ROOT / "dds-backends" / backend / "CMakeLists.txt").is_file())
            for name in ("adapter.cpp", "command_adapter.cpp", "collection_adapter.cpp"):
                self.assertTrue((ROOT / "dds-backends" / backend / "src" / name).is_file())
        self.assertTrue((ROOT / "tests/integration/cyclone/CMakeLists.txt").is_file())
        for name in ("DemoBattery.idl", "DemoCommandEvent.idl", "DemoCollectionBatch.idl"):
            self.assertTrue((ROOT / "tests/fixtures/idl" / name).is_file())

    def test_sdk_public_and_core_headers_have_no_dds_vendor_imports(self):
        offending = []
        for file in PUBLIC.rglob("*.h"):
            if FORBIDDEN.search(file.read_text(encoding="utf-8")):
                offending.append(str(file.relative_to(SDK)))
        for file in PUBLIC.rglob("*.hpp"):
            if FORBIDDEN.search(file.read_text(encoding="utf-8")):
                offending.append(str(file.relative_to(SDK)))
        self.assertEqual([], offending)

    def test_runtime_and_application_have_no_vendor_native_imports(self):
        for f in [SDK / "src/component_runtime.cpp", PUBLIC / "application.hpp"]:
            self.assertNotRegex(f.read_text(encoding="utf-8"), FORBIDDEN)

    def test_single_vendor_backend_contract_exists(self):
        header = PUBLIC / "internal/dds_backend.h"
        content = header.read_text(encoding="utf-8")
        self.assertIn("UmaaDdsBackendV1", content)
        self.assertIn("UMAA_DDS_BACKEND_ENTRY", content)
        self.assertNotRegex(content, FORBIDDEN)

    def test_device_module_fixture_never_includes_dds_headers(self):
        source = SDK / "examples/modules/vehicle_component.cpp"
        self.assertNotRegex(source.read_text(encoding="utf-8"), FORBIDDEN)


if __name__ == "__main__":
    unittest.main()
