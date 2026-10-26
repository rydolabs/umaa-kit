"""Fail-closed codegen checks without redistributing AUVSI IDL files."""
import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
FILE = ROOT / "tools" / "generate_official_engine_plugin.py"
spec = importlib.util.spec_from_file_location("official_engine_codegen", FILE)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

CMD = """typedef struct UMAA_EO_EngineControl_EngineCommandType {
  UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_IgnitionControlEnumType state;
  struct UMAA_Common_Measurement_DateTime timeStamp;
  struct UMAA_Common_IdentifierType source;
  UMAA_Common_Measurement_NumericGUID sessionID;
  struct UMAA_Common_IdentifierType destination;
} UMAA_EO_EngineControl_EngineCommandType;
"""
ENUM = """typedef enum UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_IgnitionControlEnumType {
 UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_OFF,
 UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_RUN
} UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_IgnitionControlEnumType;
"""


class OfficialCodegenTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name)
        self.command = self.root / "UMAA/EO/EngineControl/EngineCommandType.h"
        self.enum = self.root / "UMAA/Common/MaritimeEnumeration/MaritimeEnumerationSets.h"
        self.command.parent.mkdir(parents=True)
        self.enum.parent.mkdir(parents=True)
        self.command.write_text(CMD)
        self.enum.write_text(ENUM)

    def test_generates_only_protocol_free_user_interface_and_glue(self):
        out = self.root / "generated"
        mod.generate(self.root, out)
        header = (out / "engine_control_handler.hpp").read_text()
        glue = (out / "engine_control_module.cpp").read_text()
        self.assertIn("virtual bool set_ignition", header)
        self.assertIn("virtual bool stop()", header)
        self.assertNotIn("<dds/", header)
        self.assertIn("UMAA::EO::EngineControl", glue)
        self.assertIn("uma a".replace(" ",""), glue)
        self.assertEqual(sorted(p.name for p in out.iterdir()),
                         ["engine_control_handler.hpp", "engine_control_module.cpp"])

    def test_fails_closed_if_source_or_session_disappears(self):
        self.command.write_text(CMD.replace("  struct UMAA_Common_IdentifierType source;\n", ""))
        with self.assertRaisesRegex(ValueError, "mapping changed"):
            mod.generate(self.root, self.root / "generated")

    def test_fails_closed_on_unexpected_enum_or_value(self):
        self.enum.write_text(ENUM.replace("Module_RUN\n", "Module_RUN,\n UMAA_Common_MaritimeEnumeration_IgnitionControlEnumModule_START\n"))
        with self.assertRaisesRegex(ValueError, "unsupported ignition"):
            mod.generate(self.root, self.root / "generated")

    def test_repeated_generation_does_not_touch_user_file(self):
        out = self.root / "generated"
        out.mkdir()
        user = out / "user_engine.cpp"
        user.write_text("important_user_implementation")
        mod.generate(self.root, out)
        mod.generate(self.root, out)
        self.assertEqual(user.read_text(), "important_user_implementation")


if __name__ == "__main__":
    unittest.main()
