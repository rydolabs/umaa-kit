"""Lease and fencing unit regressions (SQLite supports only single-worker semantics)."""
from datetime import timedelta
from pathlib import Path
import tempfile
import unittest
from studio.platform_db import Base, Project, User, Membership, ModelImport, Job, connect, utcnow
from studio.platform_worker import claim_job, finish_job, renew_lease

class WorkerLeaseTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.engine, self.sessions = connect('sqlite:///' + str(Path(self.tmp.name)/'test.sqlite'))
        Base.metadata.create_all(self.engine)
        self.addCleanup(self.engine.dispose)
        with self.sessions.begin() as db:
            db.add(User(id='test', display_name='Tester'))
            db.add(Project(id='p', name='P', created_by='test'))
            db.add(Membership(project_id='p', user_id='test', role='owner'))
            db.add(ModelImport(id='m', project_id='p', created_by='test',
                               source_sha256='x'*64, snapshot={'generationApproved': False}))
            db.add(Job(id='j', project_id='p', model_import_id='m',
                       created_by='test', kind='docx', options={}))

    def expire(self):
        with self.sessions.begin() as db:
            db.get(Job,'j').lease_expires_at = utcnow() - timedelta(seconds=1)

    def test_reclaim_and_stale_fencing(self):
        first = claim_job(self.sessions, worker_id='first', lease_seconds=30)
        self.assertEqual(first[0], 'j')
        self.assertIsNone(claim_job(self.sessions, worker_id='second'))
        self.assertTrue(renew_lease(self.sessions,'j',first[1]))
        self.expire()
        second = claim_job(self.sessions, worker_id='second', lease_seconds=30)
        self.assertEqual(second[0], 'j')
        self.assertNotEqual(first[1], second[1])
        self.assertFalse(renew_lease(self.sessions,'j',first[1]))
        self.assertFalse(finish_job(self.sessions,'j',first[1],error='stale overwrite'))
        self.assertTrue(finish_job(self.sessions,'j',second[1],error='expected failure'))
        with self.sessions() as db:
            self.assertEqual(db.get(Job,'j').attempts, 2)
            self.assertEqual(db.get(Job,'j').error, 'expected failure')

    def test_max_attempts_do_not_starve_queue(self):
        for _ in range(2):
            self.assertEqual(claim_job(self.sessions,max_attempts=2)[0], 'j')
            self.expire()
        with self.sessions.begin() as db:
            db.add(Job(id='j2',project_id='p',model_import_id='m',
                       created_by='test',kind='docx',options={}))
        next_job = claim_job(self.sessions,max_attempts=2)
        self.assertEqual(next_job[0], 'j2')
        with self.sessions() as db:
            self.assertEqual(db.get(Job,'j').status, 'FAIL')
            self.assertIn('maximum attempts',db.get(Job,'j').error)

    def test_invalid_lease_config_is_rejected(self):
        with self.assertRaises(ValueError): claim_job(self.sessions,lease_seconds=0)
        with self.assertRaises(ValueError): claim_job(self.sessions,max_attempts=0)
        with self.assertRaises(ValueError): renew_lease(self.sessions,'j','x',lease_seconds=0)
