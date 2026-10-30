"""Cross-platform regression for fail-closed supervisor journal recovery.

No device module, DDS middleware, or hardware is required. The ownership
race is forced by blocking the reconciler while it holds the owner marker.
"""
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from umaa_operation_journal import JournalFault, OperationJournal
from umaa_process_supervisor import ComponentProcessSupervisor, WorkerFault


class DurableJournalRecoveryTest(unittest.TestCase):
    def test_reconciliation_does_not_unlock_older_session(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "commands.json"
            first = OperationJournal(path)
            try:
                first.begin("Demo::EO::EngineControl", 701, 1)
            finally:
                first.release_owner()

            with self.assertRaises(JournalFault):
                OperationJournal(path)
            with self.assertRaises(JournalFault):
                OperationJournal.reconcile(path)

            OperationJournal.reconcile(path, hardware_reconciled=True)
            self.assertFalse(path.with_name(path.name + ".owner").exists())
            second = OperationJournal(path)
            try:
                self.assertEqual(second.entries, [])
                self.assertEqual(second.settled, [
                    {"service": "Demo::EO::EngineControl", "session": 701}
                ])
                with self.assertRaises(JournalFault):
                    second.begin("Demo::EO::EngineControl", 701, 1)
            finally:
                second.release_owner()

    def test_reconciliation_excludes_concurrent_startup(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "commands.json"
            journal = OperationJournal(path)
            try:
                journal.begin("Demo::EO::EngineControl", 702, 1)
            finally:
                journal.release_owner()

            entered = threading.Event()
            proceed = threading.Event()
            errors = []
            original_load = OperationJournal._load

            def blocked_load(instance):
                if threading.current_thread() is thread:
                    entered.set()
                    if not proceed.wait(5):
                        raise RuntimeError("test reconciliation timed out")
                return original_load(instance)

            def run_reconcile():
                try:
                    OperationJournal.reconcile(path, hardware_reconciled=True)
                except BaseException as exc:
                    errors.append(exc)

            thread = threading.Thread(target=run_reconcile)
            with mock.patch.object(OperationJournal, "_load", blocked_load):
                thread.start()
                try:
                    self.assertTrue(entered.wait(5), "reconciliation never acquired owner")
                    owner = path.with_name(path.name + ".owner")
                    self.assertTrue(owner.exists())
                    with self.assertRaises(JournalFault):
                        OperationJournal(path)
                    # A competing operator may not reconcile this active owner.
                    with self.assertRaises(JournalFault):
                        OperationJournal.reconcile(path, hardware_reconciled=True)
                    self.assertTrue(owner.exists())
                finally:
                    proceed.set()
                    thread.join(5)
            self.assertFalse(thread.is_alive())
            self.assertFalse(errors, str(errors))
            self.assertFalse(owner.exists())
            recovered = OperationJournal(path)
            try:
                self.assertEqual(len(recovered.settled), 1)
                self.assertEqual(recovered.settled[0]["session"], 702)
            finally:
                recovered.release_owner()

    def test_corrupt_journal_does_not_leak_owner_after_reconcile(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "broken.json"
            owner = path.with_name(path.name + ".owner")
            path.write_text("{unparseable", encoding="utf-8")
            with self.assertRaises(JournalFault):
                OperationJournal.reconcile(path, hardware_reconciled=True)
            self.assertFalse(owner.exists())
            # The corrupt state remains on disk; recovery must not erase it.
            self.assertEqual(path.read_text(encoding="utf-8"), "{unparseable")
            with self.assertRaises(JournalFault):
                OperationJournal(path)
            self.assertFalse(owner.exists())

    def test_boolean_schema_version_cannot_impersonate_version_one(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "wrong-version.json"
            path.write_text(json.dumps({
                "version": True, "unresolved": [], "settled": []
            }), encoding="utf-8")
            with self.assertRaises(JournalFault):
                OperationJournal(path)
            with self.assertRaises(JournalFault):
                OperationJournal.reconcile(path, hardware_reconciled=True)

    def test_bounded_restart_cycles_preserve_completed_session_dedup(self):
        # Exercise repeated fsync/atomic replacements and process-independent
        # reopen, without using a physical device or asserting durability
        # under power loss. Bounded deliberately for Windows CI.
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "restart-cycles.json"
            for session in range(900, 924):
                active = OperationJournal(path)
                try:
                    active.begin("Demo::EO::EngineControl", session, 1)
                    active.resolved("Demo::EO::EngineControl", session, 1)
                finally:
                    active.release_owner()
                reopened = OperationJournal(path)
                try:
                    self.assertEqual(len(reopened.settled), session - 899)
                    with self.assertRaises(JournalFault):
                        reopened.begin("Demo::EO::EngineControl", session, 1)
                finally:
                    reopened.release_owner()

    def test_quarantined_or_absent_worker_cannot_create_new_intent(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "quarantined.json"
            journal = OperationJournal(path)
            try:
                supervisor = object.__new__(ComponentProcessSupervisor)
                supervisor._lock = threading.RLock()
                supervisor._journal = journal
                supervisor._faulted = True
                supervisor._proc = None
                for faulted in (True, False):
                    supervisor._faulted = faulted
                    with self.assertRaises(WorkerFault):
                        supervisor.call("Demo::EO::EngineControl", 1, b"run", 803)
                    self.assertEqual(journal.entries, [])
                with self.assertRaises(ValueError):
                    supervisor.call("Demo::EO::EngineControl", True, b"run", 803)
                with self.assertRaises(ValueError):
                    supervisor.call("Demo::EO::EngineControl", 1, b"run", True)
            finally:
                journal.release_owner()

    def test_exited_worker_cannot_create_new_intent(self):
        class ExitedProcess:
            def poll(self):
                return 17
            def wait(self, timeout=None):
                return 17
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "exited.json"
            journal = OperationJournal(path)
            try:
                supervisor = object.__new__(ComponentProcessSupervisor)
                supervisor._lock = threading.RLock()
                supervisor._journal = journal
                supervisor._faulted = False
                supervisor._proc = ExitedProcess()
                with self.assertRaises(WorkerFault):
                    supervisor.call("Demo::EO::EngineControl", 1, b"run", 804)
                self.assertTrue(supervisor.faulted)
                self.assertEqual(journal.entries, [])
            finally:
                journal.release_owner()


if __name__ == "__main__":
    unittest.main()
