"""Strict reviewed QoS minimum generation regressions."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0,str(ROOT))
from tools.generate_qos_requirements import INPUT,OUTPUT,parse,render

class QosContractGenerationTest(unittest.TestCase):
    def test_generated_header_is_exact_and_source_reviewed(self):
        entries=parse(INPUT)
        self.assertEqual(1,len(entries))
        self.assertEqual(render(entries),OUTPUT.read_text(encoding="utf-8"))
        self.assertIn("require_ordered_access=true",render(entries))
        self.assertNotIn("transient_local",render(entries))

    def test_unknown_source_and_extra_properties_fail(self):
        raw=INPUT.read_text(encoding="utf-8")
        for alt in (
            raw.replace('page: 14','page: 15'),
            raw.replace('orderedAccess: true','orderedAccess: false'),
            raw.replace('presentationScope: "topic"','presentationScope: "group"'),
            raw + "  unknown: true\n",
            raw.replace('orderedAccess: true','orderedAccess: true\n      orderedAccess: false'),
        ):
            with self.subTest(alt=alt[-90:]):
                with tempfile.TemporaryDirectory() as t:
                    p=Path(t)/"bad.yaml"
                    p.write_text(alt,encoding="utf-8")
                    with self.assertRaises(ValueError):
                        parse(p)

    def test_stale_output_check_fails(self):
        with tempfile.TemporaryDirectory() as t:
            out=Path(t)/"stale.hpp"
            out.write_text("manual edit",encoding="utf-8")
            cmd=[sys.executable,"-m","tools.generate_qos_requirements",
                "--input",str(INPUT),"--output",str(out),"--check"]
            proc=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True)
            self.assertNotEqual(proc.returncode,0)
            self.assertEqual("manual edit",out.read_text())

if __name__=="__main__":
    unittest.main()
