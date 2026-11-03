"""Native generator does not parse IDL; it delegates to idlc/fastddsgen.

No real vendor installation is needed here: compiler calls and output
expectations are tested with controlled subprocess stubs. The Actions workflow
is the independent genuine-compiler acceptance gate.
"""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import TestCase
from unittest.mock import patch
import hashlib
import sys

ROOT = Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from tools.native_dds_codegen import native_generate


class NativeCompilerOrchestrationTests(TestCase):
    def setUp(self):
        self.temporary = TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        self.root = self.folder / "idl"
        self.root.mkdir()
        (self.root / "Common.idl").write_text(
            "module Demo { struct Value { long n; }; };")
        (self.root / "Device.idl").write_text(
            '#include "Common.idl"\nmodule Demo { struct Device { sequence<Value> values; }; };')
        self.out = self.folder / "generated"

    def fake_compiler(self, args, **kwargs):
        self.assertTrue(kwargs.get("check"))
        self.assertGreater(kwargs.get("timeout", 0), 0)
        self.commands.append(args)
        if args[0] == "idlc":
            dest = Path(args[args.index("-o") + 1])
            stem = Path(args[-1]).stem
            dest.mkdir(parents=True, exist_ok=True)
            (dest / (stem + ".c")).write_text("/* stub not DDS */")
            (dest / (stem + ".h")).write_text("/* stub not DDS */")
        else:
            self.assertEqual(args[0], "fastddsgen")
            dest = Path(args[args.index("-d") + 1])
            stem = Path(args[-1]).stem
            dest.mkdir(parents=True, exist_ok=True)
            (dest / (stem + "PubSubTypes.cxx")).write_text("/* stub */")
        return None

    def test_real_vendor_invocation_and_pinned_manifest(self):
        self.commands = []
        digest = hashlib.sha256((self.root / "Device.idl").read_bytes()).hexdigest()
        with patch("tools.native_dds_codegen.shutil.which", side_effect=lambda x: x), \
                patch("tools.native_dds_codegen.subprocess.run",
                      side_effect=self.fake_compiler):
            record = native_generate(self.root, "Device.idl", self.out, vendor="both",
                                     source_sha256=digest)
        self.assertEqual(2, record["transitiveIdls"])
        self.assertEqual(4, len(self.commands))
        self.assertEqual(["idlc", "idlc", "fastddsgen", "fastddsgen"],
                         [x[0] for x in self.commands])
        self.assertTrue(record["nativeTypeSupportGenerated"])
        self.assertFalse(record["officialConformanceClaim"])
        self.assertFalse(record["serviceQosVerified"])
        self.assertTrue((self.out / "native-generation.json").is_file())
        self.assertTrue(any("DevicePubSubTypes.cxx" in s
                            for s in record["nativeSources"]["fastdds"]))

    def test_pin_mismatch_blocks_before_compiler_or_output(self):
        with patch("tools.native_dds_codegen.subprocess.run") as run:
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                native_generate(self.root, "Device.idl", self.out, vendor="both",
                                source_sha256="0" * 64)
            run.assert_not_called()
        self.assertFalse(self.out.exists())

    def test_path_traversal_and_symlink_rejected(self):
        with self.assertRaisesRegex(ValueError, "relative"):
            native_generate(self.root, "../secret.idl", self.out, vendor="both")
        (self.root / "Linked.idl").symlink_to(self.root / "Device.idl")
        with self.assertRaisesRegex(ValueError, "symlink"):
            native_generate(self.root, "Linked.idl", self.out, vendor="both")

    def test_missing_actual_vendor_executable_fails_closed(self):
        with patch("tools.native_dds_codegen.shutil.which", return_value=None):
            with self.assertRaises(FileNotFoundError):
                native_generate(self.root, "Device.idl", self.out, vendor="both")
        self.assertFalse(self.out.exists())
