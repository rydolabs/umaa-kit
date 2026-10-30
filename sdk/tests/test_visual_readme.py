"""README SVG assets remain parseable, accessible, and accurately scoped."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
README = ROOT / "README.md"
ASSETS = ("umaa-kit-hero.svg", "umaa-kit-workflows.svg")
FENCE = chr(96) * 3


class VisualReadmeTest(unittest.TestCase):
    def test_diagrams_are_accessible_and_parseable_svg(self):
        for filename in ASSETS:
            with self.subTest(filename=filename):
                path = ROOT / "docs" / "assets" / filename
                self.assertTrue(path.is_file())
                tree = ET.parse(path)
                root = tree.getroot()
                ns = {"svg": "http://www.w3.org/2000/svg"}
                self.assertEqual(root.tag, "{http://www.w3.org/2000/svg}svg")
                self.assertEqual(root.attrib.get("role"), "img")
                self.assertGreater(len(root.find("svg:title", ns).text.strip()), 10)
                self.assertGreater(len(root.find("svg:desc", ns).text.strip()), 40)
                source = path.read_text(encoding="utf-8").lower()
                self.assertFalse(any(x in source for x in
                                     ("<script", "<foreignobject", "javascript:")))
                self.assertGreater(len(list(root.iter())), 20)

    def test_readme_links_images_and_feature_scope(self):
        body = README.read_text(encoding="utf-8")
        for asset in ASSETS:
            self.assertIn("](docs/assets/" + asset + ")", body)
        self.assertGreaterEqual(body.count(FENCE + "mermaid"), 3)
        self.assertIn("React Flow", body)
        self.assertIn("planned", body.lower())
        self.assertIn("synthetic", body.lower())
        self.assertIn("CLI", body)
        self.assertNotIn("lives in a Draft development branch", body)

    def test_readme_code_fences_are_balanced(self):
        fenced = [line for line in README.read_text(encoding="utf-8").splitlines()
                  if line.startswith(FENCE)]
        self.assertEqual(len(fenced) % 2, 0)
        for line in fenced[1::2]:
            self.assertEqual(line, FENCE)


if __name__ == "__main__":
    unittest.main()
