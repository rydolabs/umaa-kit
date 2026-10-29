"""Child crash and pending-session tests: never replay hardware commands."""
import pathlib
import struct
import tempfile
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from umaa_process_supervisor import ComponentProcessSupervisor, WorkerFault

class IsolatedWorkerTest(unittest.TestCase):
    worker = None
    module = None

    @classmethod
    def setUpClass(cls):
        if cls.worker is None or cls.module is None:
            raise unittest.SkipTest("standalone subprocess CTest supplies worker and DLL/SO")

    def make(self):
        return ComponentProcessSupervisor(self.worker, self.module, timeout=5)

    def test_worker_host_isolation_and_explicit_restart(self):
        with self.make() as host:
            self.assertTrue(host.ping())
            status, result, reply = host.call(
                "Demo::EO::EngineControl",1,struct.pack("=i",1500),1)
            self.assertEqual((status,result,reply),(0,0,b""))
            status, result, report = host.call(
                "Demo::EO::BatteryStatus",3)
            self.assertEqual((status,result,struct.unpack("=i",report)[0]),(0,0,80))
            with self.assertRaises(WorkerFault):
                host.test_crash()
            self.assertTrue(host.faulted)
            with self.assertRaises(WorkerFault):
                host.call("Demo::EO::EngineControl",1,struct.pack("=i",1500),1)
            with self.assertRaises(WorkerFault):
                host.restart()
            host.restart(hardware_reconciled=True)
            self.assertTrue(host.ping())
            status, result, report = host.call("Demo::EO::BatteryStatus",3)
            self.assertEqual((status,result,struct.unpack("=i",report)[0]),(0,0,80))
            self.assertTrue(host.stop())

    def test_durable_pending_session_refuses_implicit_recovery(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = pathlib.Path(tmp) / "device-intents.json"
            with ComponentProcessSupervisor(
                    self.worker, self.module, timeout=5,
                    journal_path=journal) as host:
                status, result, _ = host.call(
                    "Demo::EO::AsyncEngineControl", 1,
                    struct.pack("=i", 1500), 411)
                self.assertEqual((status, result), (0, 2))
                self.assertTrue(journal.exists())
                with self.assertRaises(WorkerFault):
                    host.test_crash()
            # Even a new Python supervisor/process cannot replay a
            # possibly running hardware command from a previous owner.
            from umaa_operation_journal import JournalFault
            with self.assertRaises(JournalFault):
                ComponentProcessSupervisor(self.worker, self.module,
                                           journal_path=journal)
            with self.assertRaises(JournalFault):
                ComponentProcessSupervisor.reconcile_journal(journal)
            ComponentProcessSupervisor.reconcile_journal(
                journal, hardware_reconciled=True)
            with ComponentProcessSupervisor(
                    self.worker, self.module, journal_path=journal) as fresh:
                self.assertTrue(fresh.ping())
                self.assertTrue(fresh.stop())

    def test_completed_command_releases_durable_intent(self):
        with tempfile.TemporaryDirectory() as tmp:
            journal = pathlib.Path(tmp) / "resolved.json"
            with ComponentProcessSupervisor(
                    self.worker, self.module, journal_path=journal) as host:
                self.assertEqual(host.call(
                    "Demo::EO::EngineControl", 1,
                    struct.pack("=i", 1200), 77)[:2], (0, 0))
                self.assertTrue(host.stop())
            # A verified terminal result is not left as an unknown outcome.
            with ComponentProcessSupervisor(
                    self.worker, self.module, journal_path=journal) as fresh:
                self.assertTrue(fresh.ping())
                self.assertTrue(fresh.stop())

    def test_pending_command_blocks_cooperative_stop(self):
        with self.make() as host:
            status, result, _ = host.call(
                "Demo::EO::AsyncEngineControl",1,struct.pack("=i",1500),123)
            self.assertEqual((status,result),(0,2))
            self.assertFalse(host.stop())
            for _ in range(2):
                self.assertEqual(host.call("Demo::EO::AsyncEngineControl",4,b"",123)[:2],(0,2))
            self.assertEqual(host.call("Demo::EO::AsyncEngineControl",4,b"",123)[:2],(0,0))
            self.assertTrue(host.stop())

if __name__ == "__main__":
    if len(sys.argv)!=3:
        raise SystemExit(2)
    IsolatedWorkerTest.worker,IsolatedWorkerTest.module=sys.argv[1:3]
    unittest.main(argv=[sys.argv[0]])
