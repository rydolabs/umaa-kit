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
        self.path = Path(path).resolve(strict=False)
        if self.path.is_dir():
            raise JournalFault("journal path is a directory")
        self.entries = self._load()
        if self.entries:
            raise JournalFault("unreconciled device operation journal; refuse startup")

    def _load(self):
        if not self.path.exists():
            return []
        if self.path.is_symlink():
            raise JournalFault("refuse symlink journal")
        try:
            if self.path.stat().st_size > self.LIMIT:
                raise JournalFault("oversized operation journal")
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (ValueError, UnicodeError, OSError) as exc:
            raise JournalFault("corrupt/unreadable journal, operator recovery needed") from exc
        if not isinstance(data, dict) or data.get("version") != 1:
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
                    or not (0 < session < 2**64) or action not in (1, 2)):
                raise JournalFault("invalid operation journal identity")
            key = (service, session, action)
            if key in seen:
                raise JournalFault("duplicate operation journal identity")
            seen.add(key)
        return rows

    def _save(self):
        payload = json.dumps({"version": 1, "unresolved": self.entries},
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
        if not service or len(service) > 256 or any(c in service for c in "\r\n\t"):
            raise ValueError("invalid durable command service")
        if not 0 < session < 2**64 or action not in (1, 2):
            raise ValueError("durable Command requires trackable nonzero session")
        entry = {"service": service, "session": session, "action": action}
        if entry in self.entries:
            raise JournalFault("unresolved operation; replay forbidden")
        if len(self.entries) >= 128:
            raise JournalFault("too many unresolved device operations")
        self.entries.append(entry)
        try:
            self._save()
        except Exception:
            self.entries.pop()
            raise

    def resolved(self, service: str, session: int, action=None):
        # Only an unambiguous terminal device result may clear an intent.
        before = list(self.entries)
        self.entries = [row for row in self.entries
                        if not (row["service"] == service and
                                row["session"] == session and
                                (action is None or row["action"] == action))]
        if self.entries != before:
            try:
                self._save()
            except Exception:
                self.entries = before
                raise

    def resolved_all_after_reconciliation(self):
        self.entries.clear()
        self._save()

    @classmethod
    def reconcile(cls, path, *, hardware_reconciled=False):
        if not hardware_reconciled:
            raise JournalFault("explicit hardware reconciliation required")
        # Read and validate even when reconciliation is authorized.
        obj = cls.__new__(cls)
        obj.path = Path(path).resolve(strict=False)
        obj.entries = obj._load()
        obj.entries.clear()
        obj._save()
