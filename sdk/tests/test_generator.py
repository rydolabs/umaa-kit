import importlib.util
import pathlib
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("codegen", HERE / "tools" / "umaa_codegen.py")
mod = importlib.util.module_from_spec(spec)
import sys
sys.modules[spec.name] = mod
spec.loader.exec_module(mod)

class TestIdl(unittest.TestCase):
    def test_fixture(self):
        types = mod.parse_idl((HERE / "fixtures/demo.idl").read_text())
        self.assertEqual([f.name for f in types["Demo::EO::EngineCommandType"]], ["sessionID", "targetRpm"])
        self.assertTrue(types["Demo::EO::EngineCommandType"][0].is_key)
    def test_fail_closed(self):
        for s in ("module X { union Y switch (long) {}; };", "struct A { sequence<long> bad; };", "module X { struct A { long a; };", "#include <x.idl>"):
            with self.subTest(s=s), self.assertRaises(mod.SchemaError): mod.parse_idl(s)
    def test_generation_and_preserve_user(self):
        with tempfile.TemporaryDirectory() as d:
            out = pathlib.Path(d)
            p = mod.generate(HERE / "fixtures/demo.idl", HERE / "fixtures/services.json", "Demo::EO::EngineControl", out)
            self.assertEqual(len(p), 3)
            self.assertIn("EngineControlProvider", p[1].read_text())
            p[2].write_text("MY CUSTOM CODE")
            mod.generate(HERE / "fixtures/demo.idl", HERE / "fixtures/services.json", "Demo::EO::EngineControl", out)
            self.assertEqual(p[2].read_text(), "MY CUSTOM CODE")
    def test_report_generation(self):
        with tempfile.TemporaryDirectory() as d:
            out = pathlib.Path(d)
            p = mod.generate(HERE / "fixtures/demo.idl", HERE / "fixtures/services.json", "Demo::EO::BatteryStatus", out)
            self.assertIn("read_report", p[1].read_text())
            self.assertIn("BatteryStatusConsumerCache", p[1].read_text())
            self.assertIn("read_report", p[2].read_text())
    def test_no_service_heuristic(self):
        with self.assertRaises(mod.SchemaError):
            mod.load_service(HERE / "fixtures/services.json", "Nonexistent", {"Demo::EO::EngineCommandType": []})

if __name__ == "__main__": unittest.main()
