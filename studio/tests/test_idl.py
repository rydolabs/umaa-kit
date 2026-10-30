"""Synthetic EA-shaped fixture tests. NOT acceptance on real Sparx QEA or idlc."""
from copy import deepcopy
import json
from pathlib import Path
import sqlite3
from contextlib import closing
import subprocess
import sys
import tempfile
import unittest

from studio.idl import draft_profile, generate_idl, verify_idl, build_idl
from studio.qea import extract, save_snapshot, sha


def build_fixture(path: Path, variant='valid'):
    with closing(sqlite3.connect(path)) as db:
        db.executescript('''
        CREATE TABLE t_package (Package_ID INTEGER, ea_guid TEXT, Name TEXT, Parent_ID INTEGER);
        CREATE TABLE t_object (Object_ID INTEGER, ea_guid TEXT, Name TEXT, Object_Type TEXT, Package_ID INTEGER);
        CREATE TABLE t_connector (Connector_ID INTEGER, ea_guid TEXT, Name TEXT, Connector_Type TEXT, Start_Object_ID INTEGER, End_Object_ID INTEGER);
        CREATE TABLE t_attribute (ID INTEGER, ea_guid TEXT, Object_ID INTEGER, Name TEXT, Type TEXT, Classifier TEXT, Pos INTEGER);
        CREATE TABLE t_diagram (Diagram_ID INTEGER, ea_guid TEXT, Name TEXT, Package_ID INTEGER);
        ''')
        db.execute("INSERT INTO t_package VALUES (1, 'package-1', 'Demo', 0)")
        objects = [('point-guid', 'Point', 'DataType', 1),
                   ('state-guid', 'State', 'Class', 2),
                   ('enum-guid', 'Color', 'Enumeration', 3)]
        for guid, name, kind, id_ in objects:
            db.execute('INSERT INTO t_object VALUES (?,?,?,?,1)', (id_, guid, name, kind))
        attrs = [('a1', 1, 'x', 'double'), ('a2', 1, 'y', 'double'),
                 ('a3', 2, 'location', 'Point'), ('a4', 2, 'count', 'int32')]
        if variant == 'unmapped': attrs[-1] = ('a4', 2, 'count', 'std::vector<float>')
        elif variant == 'recursive': attrs[-1] = ('a4', 2, 'count', 'State')
        elif variant == 'duplicate_field': attrs[-1] = ('a4', 2, 'location', 'int32')
        elif variant == 'reserved_field': attrs[-1] = ('a4', 2, 'module', 'int32')
        elif variant == 'missing_guid': attrs[-1] = (None, 2, 'count', 'int32')
        for i, (guid, owner, name, type_) in enumerate(attrs, 1):
            db.execute('INSERT INTO t_attribute VALUES (?,?,?,?,?,NULL,?)', (i, guid, owner, name, type_, (i-1) % 2))
        db.execute("INSERT INTO t_diagram VALUES (1, 'diag', 'Model', 1)")
        db.commit()
    return path


class StudioIdlTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.qea = build_fixture(self.root / 'test.qea')
        self.snapshot_path = save_snapshot(extract(self.qea), self.root / 'snapshot.json')
        self.profile_path = self.root / 'profile.json'
        self.profile = draft_profile(self.snapshot_path, 'ProjectTypes',
                                     ['state-guid', 'point-guid'], self.profile_path)

    def render(self):
        return build_idl(json.loads(self.snapshot_path.read_text()),
                         json.loads(self.profile_path.read_text()), sha(self.snapshot_path))

    def test_end_to_end_and_dependency_order(self):
        original = self.qea.read_bytes()
        manifest = generate_idl(self.snapshot_path, self.profile_path, self.root / 'out')
        text = (self.root / 'out/ProjectTypes.idl').read_text()
        self.assertLess(text.index('struct Point'), text.index('struct State'))
        self.assertIn('double x;', text)
        self.assertIn('Point location;', text)
        self.assertIn('long count;', text)
        self.assertIn('NOT official UMAA IDL', text)
        self.assertFalse(manifest['approved'])
        self.assertFalse(manifest['idlCompilerValidated'])
        self.assertFalse(manifest['conformanceClaim'])
        self.assertEqual(verify_idl(self.snapshot_path, self.profile_path, self.root / 'out'), [])
        self.assertEqual(original, self.qea.read_bytes())

    def test_deterministic_source_across_runs(self):
        generate_idl(self.snapshot_path, self.profile_path, self.root / 'a')
        generate_idl(self.snapshot_path, self.profile_path, self.root / 'b')
        self.assertEqual((self.root / 'a/ProjectTypes.idl').read_bytes(),
                         (self.root / 'b/ProjectTypes.idl').read_bytes())
        self.assertEqual((self.root / 'a/manifest.json').read_bytes(),
                         (self.root / 'b/manifest.json').read_bytes())

    def test_reject_missing_or_wrong_profile_scope(self):
        p = deepcopy(self.profile)
        p['scope'] = 'official-umaa'
        with self.assertRaisesRegex(ValueError, 'project-extension-only'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))
        p = deepcopy(self.profile)
        p['approved'] = True
        with self.assertRaisesRegex(ValueError, 'deny approval'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))

    def test_reject_official_like_names(self):
        p = deepcopy(self.profile)
        p['module'] = 'UMAA6_MyTypes'
        with self.assertRaisesRegex(ValueError, 'module names'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))
        p['module'] = 'module'
        with self.assertRaisesRegex(ValueError, 'reserved IDL identifier'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))

    def test_reject_unselected_external_dependency(self):
        p = deepcopy(self.profile)
        p['selectedElementGuids'] = ['state-guid']
        with self.assertRaisesRegex(ValueError, 'Unmapped/non-selected'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))

    def test_reject_unsupported_enum(self):
        p = deepcopy(self.profile)
        p['selectedElementGuids'].append('enum-guid')
        with self.assertRaisesRegex(ValueError, 'Unsupported EA element type'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))

    def test_reject_bad_type(self):
        s = json.loads(self.snapshot_path.read_text())
        s['records']['attributes'][0]['Type'] = 'vector<float>'
        with self.assertRaisesRegex(ValueError, 'Unmapped/non-selected'):
            build_idl(s, self.profile, sha(self.snapshot_path))

    def test_reject_recursive_type(self):
        s = json.loads(self.snapshot_path.read_text())
        s['records']['attributes'][3]['Type'] = 'State'
        with self.assertRaisesRegex(ValueError, 'Recursive'):
            build_idl(s, self.profile, sha(self.snapshot_path))

    def test_reject_bad_attribute_names_and_missing_guid(self):
        for field, value, err in [('Name', 'module', 'reserved IDL identifier'),
                                  ('ea_guid', None, 'distinct EA attribute GUIDs')]:
            s = json.loads(self.snapshot_path.read_text())
            s['records']['attributes'][3][field] = value
            with self.assertRaisesRegex(ValueError, err):
                build_idl(s, self.profile, sha(self.snapshot_path))
        s = json.loads(self.snapshot_path.read_text())
        s['records']['attributes'][3]['Name'] = 'location'
        with self.assertRaisesRegex(ValueError, 'Duplicate/case-colliding field'):
            build_idl(s, self.profile, sha(self.snapshot_path))

    def test_reject_model_count_and_warning(self):
        s = json.loads(self.snapshot_path.read_text())
        s['counts']['attributes'] += 1
        with self.assertRaisesRegex(ValueError, 'snapshot count'):
            build_idl(s, self.profile, sha(self.snapshot_path))
        s = json.loads(self.snapshot_path.read_text())
        s['modelValidation'] = 'WARNINGS'
        with self.assertRaisesRegex(ValueError, 'structurally checked'):
            build_idl(s, self.profile, sha(self.snapshot_path))

    def test_reject_model_or_profile_mismatch(self):
        p = deepcopy(self.profile)
        p['expectedSnapshotSha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'pin mismatch'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))
        p = deepcopy(self.profile)
        p['expectedSourceQeaSha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'pin mismatch'):
            build_idl(json.loads(self.snapshot_path.read_text()), p, sha(self.snapshot_path))

    def test_reject_overwrite_without_modifying_outputs(self):
        generate_idl(self.snapshot_path, self.profile_path, self.root / 'out')
        before = (self.root / 'out/ProjectTypes.idl').read_bytes()
        with self.assertRaises(FileExistsError):
            generate_idl(self.snapshot_path, self.profile_path, self.root / 'out')
        self.assertEqual(before, (self.root / 'out/ProjectTypes.idl').read_bytes())

    def test_detect_idl_tamper_and_manifest_tamper(self):
        generate_idl(self.snapshot_path, self.profile_path, self.root / 'out')
        idl = self.root / 'out/ProjectTypes.idl'
        idl.write_text(idl.read_text().replace('double x;', 'long x;'))
        self.assertTrue(any('IDL file differs' in e for e in
                            verify_idl(self.snapshot_path, self.profile_path, self.root / 'out')))
        idl.write_text(self.render()[0])
        manifest_path = self.root / 'out/manifest.json'
        manifest = json.loads(manifest_path.read_text())
        manifest['approved'] = True
        manifest_path.write_text(json.dumps(manifest))
        self.assertTrue(any('Manifest mismatch: approved' in e for e in
                            verify_idl(self.snapshot_path, self.profile_path, self.root / 'out')))

    def test_reject_extra_files(self):
        generate_idl(self.snapshot_path, self.profile_path, self.root / 'out')
        (self.root / 'out/extra.txt').write_text('anything')
        self.assertTrue(any('Unexpected files' in e for e in
                            verify_idl(self.snapshot_path, self.profile_path, self.root / 'out')))

    def test_cli_roundtrip_and_bad_evidence_code(self):
        cwd = Path(__file__).resolve().parents[2]
        cmd = [sys.executable, '-m', 'studio']
        gen = subprocess.run(cmd + ['generate-idl', '--snapshot', str(self.snapshot_path),
                    '--profile', str(self.profile_path), '--output-dir', str(self.root / 'out')],
                    cwd=cwd, capture_output=True, text=True, timeout=20)
        self.assertEqual(gen.returncode, 0, gen.stderr)
        verify = subprocess.run(cmd + ['verify-idl', '--snapshot', str(self.snapshot_path),
                    '--profile', str(self.profile_path), '--output-dir', str(self.root / 'out')],
                    cwd=cwd, capture_output=True, text=True, timeout=20)
        self.assertEqual(verify.returncode, 0, verify.stderr)
        (self.root / 'out/ProjectTypes.idl').write_text('bad')
        bad = subprocess.run(cmd + ['verify-idl', '--snapshot', str(self.snapshot_path),
                    '--profile', str(self.profile_path), '--output-dir', str(self.root / 'out')],
                    cwd=cwd, capture_output=True, text=True, timeout=20)
        self.assertEqual(bad.returncode, 2)

    def test_field_order_is_pos_not_random_guid(self):
        s = json.loads(self.snapshot_path.read_text())
        # GUID sort is the reverse of EA t_attribute.Pos sort.
        s['records']['attributes'][0]['ea_guid'] = 'zz'
        s['records']['attributes'][1]['ea_guid'] = 'aa'
        output, _ = build_idl(s, self.profile, sha(self.snapshot_path))
        self.assertLess(output.index('double x;'), output.index('double y;'))

    def test_reject_invalid_or_duplicate_field_pos(self):
        for new_pos in (-1, None, 0, '1'):
            s = json.loads(self.snapshot_path.read_text())
            s['records']['attributes'][3]['Pos'] = new_pos
            with self.assertRaisesRegex(ValueError, 'field order'):
                build_idl(s, self.profile, sha(self.snapshot_path))

    def test_reject_conflicting_ea_classifier(self):
        s = json.loads(self.snapshot_path.read_text())
        s['records']['attributes'][2]['Classifier'] = '999'
        with self.assertRaisesRegex(ValueError, 'Classifier disagrees'):
            build_idl(s, self.profile, sha(self.snapshot_path))
        s = json.loads(self.snapshot_path.read_text())
        s['records']['attributes'][0]['Classifier'] = '12'
        with self.assertRaisesRegex(ValueError, 'Primitive field'):
            build_idl(s, self.profile, sha(self.snapshot_path))

    def test_case_colliding_names_are_rejected(self):
        s = json.loads(self.snapshot_path.read_text())
        s['records']['elements'][1]['Name'] = 'point'
        with self.assertRaisesRegex(ValueError, 'case-colliding generated struct'):
            build_idl(s, self.profile, sha(self.snapshot_path))
        s = json.loads(self.snapshot_path.read_text())
        s['records']['attributes'][3]['Name'] = 'LOCATION'
        with self.assertRaisesRegex(ValueError, 'case-colliding field'):
            build_idl(s, self.profile, sha(self.snapshot_path))

    def test_reject_circular_dependency(self):
        s = json.loads(self.snapshot_path.read_text())
        s['records']['attributes'][0]['Type'] = 'State'
        with self.assertRaisesRegex(ValueError, 'Circular IDL struct dependency'):
            build_idl(s, self.profile, sha(self.snapshot_path))


if __name__ == '__main__':
    unittest.main()

# Additional gate tests kept outside the class for clarity.
class IdlcGateTests(unittest.TestCase):
    def setUp(self):
        StudioIdlTests.setUp(self)
    def test_missing_idlc_is_blocked_not_pass(self):
        from studio.idl import check_idlc
        generate_idl(self.snapshot_path, self.profile_path, self.root / 'out')
        result = check_idlc(self.snapshot_path, self.profile_path, self.root / 'out',
                            compiler='nonexistent-idlc-for-test-hopefully')
        self.assertEqual(result['status'], 'BLOCKED')
        self.assertFalse(result['idlcAccepted'])
        self.assertFalse(result['conformanceClaim'])

    def test_tampered_idl_fails_before_idlc(self):
        from studio.idl import check_idlc
        generate_idl(self.snapshot_path, self.profile_path, self.root / 'out')
        (self.root / 'out/ProjectTypes.idl').write_text('tampered')
        result = check_idlc(self.snapshot_path, self.profile_path, self.root / 'out',
                            compiler='nonexistent-idlc-for-test-hopefully')
        self.assertEqual(result['status'], 'FAIL')
        self.assertIn('evidence', result['reason'])
