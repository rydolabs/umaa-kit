"""Fail-closed optional process-isolated module supervisor.

The worker executes one Component DLL/SO in a child process. This is bounded
development IPC, not an authenticated production protocol or a claim of
hardware state reconciliation. Optional durable intent journaling fences
unknown outcomes but does NOT reconcile physical hardware automatically.
"""
from __future__ import annotations
import queue
import subprocess
import threading
from pathlib import Path
from umaa_operation_journal import OperationJournal, JournalFault

class WorkerFault(RuntimeError):
    pass

class ComponentProcessSupervisor:
    def __init__(self, worker: str, module: str, timeout: float = 5.0,
                 journal_path=None):
        self.worker = str(Path(worker))
        self.module = str(Path(module))
        if timeout <= 0 or timeout > 30:
            raise ValueError("invalid IPC timeout")
        self.timeout = timeout
        self._proc = None
        self._queue = None
        self._faulted = False
        self._lock = threading.RLock()
        self._journal = OperationJournal(journal_path) if journal_path is not None else None
        self._spawn()

    @property
    def faulted(self):
        return self._faulted

    def _spawn(self):
        self._queue = queue.Queue()
        self._proc = subprocess.Popen(
            [self.worker, self.module], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, bufsize=1)
        q, p = self._queue, self._proc

        def read_lines():
            for line in p.stdout:
                q.put(line.rstrip("\r\n"))
            q.put(None)
        threading.Thread(target=read_lines, daemon=True).start()
        if self._read() != "READY":
            self._faulted = True
            self._kill()
            raise WorkerFault("worker initialization failed")
        self._faulted = False

    def _kill(self):
        p = self._proc
        if p and p.poll() is None:
            p.kill()
        if p:
            try:
                p.wait(timeout=2)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait()

    def _read(self):
        try:
            item = self._queue.get(timeout=self.timeout)
        except queue.Empty:
            raise WorkerFault("worker IPC deadline exceeded")
        if item is None:
            raise WorkerFault("worker exited without response")
        if len(item) > 2097152:
            raise WorkerFault("oversized worker response")
        return item

    def _rpc(self, request):
        with self._lock:
            return self._rpc_locked(request)

    def _rpc_locked(self, request):
        if self._faulted or self._proc is None:
            raise WorkerFault("worker quarantined")
        if len(request) > 1048576 or "\n" in request or "\r" in request:
            raise ValueError("invalid IPC frame")
        try:
            if self._proc.poll() is not None:
                raise WorkerFault("worker crashed")
            self._proc.stdin.write(request + "\n")
            self._proc.stdin.flush()
            reply = self._read()
            if reply.startswith("FAULT\t"):
                raise WorkerFault(reply)
            return reply
        except (BrokenPipeError, OSError, WorkerFault) as err:
            self._faulted = True
            self._kill()
            raise WorkerFault("unknown device operation outcome; no automatic replay") from err

    def ping(self):
        return self._rpc("PING") == "PONG"

    def call(self, service: str, action: int, payload: bytes = b"",
             session: int = 0):
        if not service or any(c in service for c in "\n\r\t"):
            raise ValueError("invalid service")
        if not 1 <= action <= 4 or not 0 <= session < 2**64:
            raise ValueError("invalid action or session")
        if len(payload) > 262144:
            raise ValueError("payload too large")
        with self._lock:
            if self._journal and action in (1, 2):
                try:
                    self._journal.begin(service, session, action)
                except (JournalFault, OSError):
                    self._faulted = True
                    self._kill()
                    raise
            reply = self._rpc_locked(f"CALL\t{service}\t{action}\t{session}\t{payload.hex()}")
            return self._interpret_call(service, action, session, reply)

    def _interpret_call(self, service, action, session, reply):
        parts = reply.split("\t")
        if len(parts) != 4 or parts[0] != "RESULT":
            self._faulted = True
            self._kill()
            raise WorkerFault("invalid worker response")
        try:
            status, result, data = int(parts[1]), int(parts[2]), bytes.fromhex(parts[3])
            if self._journal and status == 0:
                if action == 1 and result != 2:
                    self._journal.resolved(service, session, action=1)
                elif action == 4 and result != 2:
                    self._journal.resolved(service, session)
            return status, result, data
        except (ValueError, OverflowError) as err:
            self._faulted = True
            self._kill()
            raise WorkerFault("malformed worker response") from err

    def stop(self):
        if self._proc is None:
            return True
        reply = self._rpc("STOP")
        if reply == "BUSY":
            return False  # Pending device work: never unload it implicitly.
        if reply != "STOPPED":
            self._faulted = True
            self._kill()
            raise WorkerFault("unknown shutdown state")
        self._proc.wait(timeout=self.timeout)
        self._proc = None
        return True

    @staticmethod
    def reconcile_journal(path, *, hardware_reconciled=False):
        OperationJournal.reconcile(path, hardware_reconciled=hardware_reconciled)

    def restart(self, *, hardware_reconciled: bool = False):
        # Never restart a crashed module without operator/hardware policy.
        # In particular, never repeat the last Command.
        if not hardware_reconciled:
            raise WorkerFault("explicit hardware reconciliation required")
        if self._proc is not None and not self._faulted:
            raise WorkerFault("cannot restart a running worker")
        with self._lock:
            self._kill()
            if self._journal:
                self._journal.resolved_all_after_reconciliation()
            self._spawn()

    def test_crash(self):
        self._rpc("TEST_CRASH")

    def close(self):
        if self._proc is not None:
            self._kill()
            self._proc = None
            self._faulted = True

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()
