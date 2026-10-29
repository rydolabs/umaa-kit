"""Architecture guard: SDK application+protocol headers must be vendor-neutral.

The DDS vendor SDK headers are allowed ONLY in dds/vendors/<vendor>/ and
existing dds/cyclone/ (until its transition). SDK fake DDS tests are exempt.
"""
from pathlib import Path
import re
import unittest

SDK = Path(__file__).resolve().parents[1]
PUBLIC = SDK / "include" / "umaa"
FORBIDDEN = re.compile(r'#\s*include\s*[<"](?:dds/dds\.h|fastdds/|fastrtps/|ndds/|rti/|dds/core/)', re.I)


class VendorBoundaryTests(unittest.TestCase):
    def test_sdk_public_and_core_headers_have_no_dds_vendor_imports(self):
        offending = []
        for file in PUBLIC.rglob("*.h"):
            if FORBIDDEN.search(file.read_text(encoding="utf-8")):
                offending.append(str(file.relative_to(SDK)))
        for file in PUBLIC.rglob("*.hpp"):
            if FORBIDDEN.search(file.read_text(encoding="utf-8")):
                offending.append(str(file.relative_to(SDK)))
        self.assertEqual([], offending)

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
