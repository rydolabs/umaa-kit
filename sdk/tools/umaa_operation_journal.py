"""Durable, fail-closed device-operation intent journal for ONE supervisor owner.

A journal entry is flushed before a side-effecting IPC call. It survives a
worker crash, a supervisor crash, and an unknown reply. This is NOT a
distributed log or proof of physical hardware state. The path must be on a
local trusted filesystem; concurrent owners are unsupported.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import stat
import tempfile


class JournalFault(RuntimeError):
    pass


class OperationJournal:
    LIMIT = 65536

    def __init__(self, path):
        self.path = Path(path).absolute()
        self.owner_path = self.path.with_name(self.path.name + ".owner")
        self._owner_fd = None
        try:
            if self.path.is_dir():
                raise JournalFault("journal path is a directory")
            self.path.parent.mkdir(parents=True, exist_ok=True)
            # Fail closed across Python processes, not just across threads.
            # An owner marker left by a killed supervisor is deliberately
            # NOT silently stolen by a new worker instance.
            self._owner_fd = os.open(
                str(self.owner_path), os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
            os.write(self._owner_fd, str(os.getpid()).encode("ascii"))
            os.fsync(self._owner_fd)
            self.entries, self.settled = self._load()
            if self.entries:
                raise JournalFault("unreconciled device operation journal; refuse startup")
        except FileExistsError as exc:
            raise JournalFault("operation journal already owned; no concurrent supervisors") from exc
        except Exception:
            self.release_owner()
            raise

    def release_owner(self):
        fd = self._owner_fd
        if fd is None:
            return
        self._owner_fd = None
        try:
            os.close(fd)
        finally:
            try:
                self.owner_path.unlink()
            except FileNotFoundError:
                pass


    def _require_owner(self):
        # A stopped supervisor must not continue writing through its stale
        # Python journal object after the O_EXCL marker has been released.
        # Only the current exclusive owner can mutate durable state.
        if self._owner_fd is None:
            raise JournalFault("operation journal ownership already released")

    def _load(self):
        if self.path.is_symlink():
            raise JournalFault("refuse symlink journal")
        if not self.path.exists():
            return [], []
        try:
            if self.path.stat().st_size > self.LIMIT:
                raise JournalFault("oversized operation journal")
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (ValueError, UnicodeError, OSError) as exc:
            raise JournalFault("corrupt/unreadable journal, operator recovery needed") from exc
        # Python bool compares equal to 1; do not accept it as a schema version.
        if (not isinstance(data, dict) or type(data.get("version")) is not int
                or data["version"] != 1):
            raise JournalFault("unsupported operation journal version")
        rows = data.get("unresolved")
        if not isinstance(rows, list) or len(rows) > 128:
            raise JournalFault("invalid operation journal entries")
        seen = set()
        for row in rows:
            if not isinstance(row, dict) or set(row) != {"service", "session", "action"}:
                raise JournalFault("invalid operation journal shape")
            service, session, action = row["service"], row["session"], row["action"]
            if (not isinstance(service, str) or not service or len(service) > 256
                    or not isinstance(session, int) or isinstance(session, bool)
                    or not (0 < session < 2**64) or type(action) is not int
                    or action not in (1, 2)):
                raise JournalFault("invalid operation journal identity")
            key = (service, session, action)
            if key in seen:
                raise JournalFault("duplicate operation journal identity")
            seen.add(key)
        settled = data.get("settled", [])
        if not isinstance(settled, list) or len(settled) > 1024:
            raise JournalFault("invalid completed Command deduplication ledger")
        settled_ids = set()
        for item in settled:
            if not isinstance(item, dict) or set(item) != {"service", "session"}:
                raise JournalFault("invalid completed Command identity")
            name, number = item["service"], item["session"]
            if (not isinstance(name, str) or not name or len(name) > 256
                    or type(number) is not int or not 0 < number < 2**64):
                raise JournalFault("invalid completed Command key")
            key = (name, number)
            if key in settled_ids:
                raise JournalFault("duplicate completed Command key")
            settled_ids.add(key)
        if any((row["service"],row["session"]) in settled_ids for row in rows):
            raise JournalFault("settled Command also marked unresolved")
        return rows, settled

    def _save(self):
        payload = json.dumps({"version": 1, "unresolved": self.entries,
                              "settled": self.settled},
                             sort_keys=True, separators=(",", ":")).encode("utf-8")
        if len(payload) > self.LIMIT:
            raise JournalFault("operation journal exceeds durable bound")
        self.path.parent.mkdir(parents=True, exist_ok=True)
        name = None
        try:
            fd, name = tempfile.mkstemp(prefix=".umaa-journal-", dir=self.path.parent)
            with os.fdopen(fd, "wb") as f:
                if os.name != "nt":
                    os.fchmod(f.fileno(), stat.S_IRUSR | stat.S_IWUSR)
                f.write(payload)
                f.flush()
                os.fsync(f.fileno())
            os.replace(name, self.path)
            name = None
            if os.name != "nt":
                directory = os.open(str(self.path.parent), os.O_RDONLY)
                try:
                    os.fsync(directory)
                finally:
                    os.close(directory)
        except OSError as exc:
            raise JournalFault("durable operation journal write failed") from exc
        finally:
            if name is not None:
                try:
                    os.unlink(name)
                except OSError:
                    pass

    def begin(self, service: str, session: int, action: int):
        self._require_owner()
        if not service or len(service) > 256 or any(c in service for c in "\r\n\t"):
            raise ValueError("invalid durable command service")
        if type(session) is not int or type(action) is not int or not 0 < session < 2**64 or action not in (1, 2):
            raise ValueError("durable Command requires trackable nonzero session")
        entry = {"service": service, "session": session, "action": action}
        if entry in self.entries or (action == 1 and any(
                row["service"] == service and row["session"] == session
                for row in self.entries)):
            raise JournalFault("unresolved operation; replay forbidden")
        if any(row["service"] == service and row["session"] == session
               for row in self.settled):
            raise JournalFault("previously completed Command session; replay forbidden")
        if len(self.entries) >= 128:
            raise JournalFault("too many unresolved device operations")
        self.entries.append(entry)
        try:
            self._save()
        except Exception:
            self.entries.pop()
            raise

    def resolved(self, service: str, session: int, action=None):
        self._require_owner()
        # Only an unambiguous terminal result can clear an unresolved intent.
        # Keep a bounded durable deduplication ledger of completed sessions;
        # even a new Python process must never replay one by accident.
        before, old_settled = list(self.entries), list(self.settled)
        self.entries = [row for row in self.entries
                        if not (row["service"] == service and
                                row["session"] == session and
                                (action is None or row["action"] == action))]
        if self.entries != before:
            if not any(row["service"] == service and row["session"] == session
                       for row in self.entries + self.settled):
                self.settled.append({"service": service, "session": session})
            try:
                if len(self.settled) > 1024:
                    raise JournalFault("completed command ledger full; operator archival needed")
                self._save()
            except Exception:
                self.entries, self.settled = before, old_settled
                raise

    def resolved_all_after_reconciliation(self):
        self._require_owner()
        # Hardware reconciliation does NOT give permission to replay an old
        # session identifier. Transfer every unknown session to dedup history.
        before, old_settled = list(self.entries), list(self.settled)
        for row in self.entries:
            if not any(old["service"] == row["service"] and
                       old["session"] == row["session"] for old in self.settled):
                self.settled.append({"service": row["service"],
                                     "session": row["session"]})
        self.entries.clear()
        try:
            if len(self.settled) > 1024:
                raise JournalFault("completed command ledger full; operator archival needed")
            self._save()
        except Exception:
            self.entries, self.settled = before, old_settled
            raise

    @classmethod
    def reconcile(cls, path, *, hardware_reconciled=False):
        if not hardware_reconciled:
            raise JournalFault("explicit hardware reconciliation required")
        # Read and validate even when reconciliation is authorized.
        obj = cls.__new__(cls)
        obj.path = Path(path).absolute()
        obj.owner_path = obj.path.with_name(obj.path.name + ".owner")
        obj._owner_fd = None
        # Merely checking owner_path.exists() is a TOCTOU race: a new
        # supervisor could claim this journal after the check and before
        # reconciliation writes it. Claim the SAME exclusive marker used by
        # normal supervisors for the entire read-modify-fsync transaction.
        # A stale marker is never stolen. An existing owner's marker must
        # never be removed by a failed reconciliation attempt.
        try:
            obj._owner_fd = os.open(
                str(obj.owner_path), os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        except FileExistsError as exc:
            raise JournalFault(
                "journal owner present; stop supervisor and clear stale marker explicitly"
            ) from exc
        try:
            os.write(obj._owner_fd, str(os.getpid()).encode("ascii"))
            os.fsync(obj._owner_fd)
            obj.entries, obj.settled = obj._load()
            obj.resolved_all_after_reconciliation()
        finally:
            obj.release_owner()
