"""Deterministic synthetic EA-like SQLite fixtures; NOT real EA QEA acceptance."""
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest

from studio.qea import extract, model_diff, save_snapshot


def qea_fixture(path, *, change=False, invalid=False):
    with sqlite3.connect(path) as db:
        db.executescript('''
            CREATE TABLE t_package (Package_ID INTEGER, ea_guid TEXT, Name TEXT, Parent_ID INTEGER);
            CREATE TABLE t_object (Object_ID INTEGER, ea_guid TEXT, Name TEXT, Object_Type TEXT, Package_ID INTEGER, Stereotype TEXT, Note TEXT);
            CREATE TABLE t_connector (Connector_ID INTEGER, ea_guid TEXT, Name TEXT, Connector_Type TEXT, Start_Object_ID INTEGER, End_Object_ID INTEGER);
            CREATE TABLE t_attribute (ID INTEGER, ea_guid TEXT, Object_ID INTEGER, Name TEXT, Type TEXT);
            CREATE TABLE t_diagram (Diagram_ID INTEGER, ea_guid TEXT, Name TEXT, Package_ID INTEGER);
        ''')
        db.execute("INSERT INTO t_package VALUES (1,'pkg-1','System',0)")
        db.execute("INSERT INTO t_object VALUES (7,'obj-7',?,'Class',1,'','')",('Modified' if change else 'Engine',))
        db.execute("INSERT INTO t_object VALUES (8,'obj-8','Status','Class',1,'','')")
        db.execute("INSERT INTO t_connector VALUES (5,'con-5','','Association',7,?)",(999 if invalid else 8,))
        db.execute("INSERT INTO t_attribute VALUES (3,'attr-3',7,'speed','double')")
        db.execute("INSERT INTO t_diagram VALUES (2,'diag-2','Overview',1)")
    db.close()
    return path


class QeaImportTests(unittest.TestCase):
    def test_read_only_import_and_structure(self):
        with tempfile.TemporaryDirectory() as td:
            p=qea_fixture(Path(td)/'sample.qea')
            initial=p.read_bytes()
            result=extract(p)
            self.assertEqual(initial,p.read_bytes())
            self.assertTrue(result['isReadOnlySnapshot'])
            self.assertFalse(result['generationApproved'])
            self.assertEqual(result['modelValidation'],'STRUCTURAL_CHECKS_PASSED')
            self.assertEqual(result['counts'],{'packages':1,'elements':2,'connectors':1,'attributes':1,'diagrams':1})
            self.assertEqual(result['records']['elements'][0]['ea_guid'],'obj-7')
            self.assertEqual(result['issues'],[])
            output=save_snapshot(result,Path(td)/'out/snapshot.json')
            self.assertTrue(output.exists())
            with self.assertRaises(FileExistsError): save_snapshot(result,output)

    def test_reimport_diff_by_stable_guid(self):
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            a=extract(qea_fixture(root/'one.qea'))
            b=extract(qea_fixture(root/'two.qea',change=True))
            diff=model_diff(a,b)
            self.assertEqual(diff['changes']['elements']['changed'],['obj-7'])
            self.assertTrue(diff['requiresArtifactReview'])
            self.assertEqual(diff['changes']['connectors']['changed'],[])

    def test_invalid_references_are_reported_not_approved(self):
        with tempfile.TemporaryDirectory() as td:
            result=extract(qea_fixture(Path(td)/'bad.qea',invalid=True))
            self.assertEqual(result['modelValidation'],'WARNINGS')
            self.assertIn('connector has unresolved endpoint',result['issues'][0])
            self.assertFalse(result['generationApproved'])

    def test_rejects_non_ea_sqlite_and_other_suffix(self):
        with tempfile.TemporaryDirectory() as td:
            path=Path(td)/'generic.qea'
            with sqlite3.connect(path) as db: db.execute('CREATE TABLE other (x INT)')
            db.close()
            with self.assertRaisesRegex(ValueError,'required tables absent'):extract(path)
            with self.assertRaisesRegex(ValueError,'Expected a regular local'):extract(path.with_suffix('.db'))

    def test_cli_import_then_diff(self):
        from studio.__main__ import main
        with tempfile.TemporaryDirectory() as td:
            root=Path(td)
            q1=qea_fixture(root/'first.qea')
            q2=qea_fixture(root/'second.qea',change=True)
            s1=root/'snap1.json'
            s2=root/'snap2.json'
            diff=root/'diff.json'
            self.assertEqual(main(['import-qea','--input',str(q1),'--output',str(s1)]),0)
            self.assertEqual(main(['import-qea','--input',str(q2),'--output',str(s2)]),0)
            self.assertEqual(main(['diff','--previous',str(s1),'--current',str(s2),'--output',str(diff)]),0)
            self.assertEqual(json.loads(diff.read_text())['changes']['elements']['changed'],['obj-7'])

    def test_rejects_duplicate_guids_in_diff(self):
        with tempfile.TemporaryDirectory() as td:
            old=extract(qea_fixture(Path(td)/'a.qea'))
            new=json.loads(json.dumps(old))
            new['records']['elements'][1]['ea_guid']='obj-7'
            with self.assertRaisesRegex(ValueError,'unique EA GUID'): model_diff(old,new)

if __name__=='__main__':unittest.main()
