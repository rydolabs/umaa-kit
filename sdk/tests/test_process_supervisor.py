"""Child crash and pending-session tests: never replay hardware commands."""
import pathlib
import struct
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
