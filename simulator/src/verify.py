"""Independent, fail-closed observer for the development-only line protocol.

Never imports code from SDK or uses its state machine. This is NOT an
accredited UML/UMAA conformance test engine. ICD 6.0 EO 5.1 is the reference.
"""
from dataclasses import dataclass, field


class VerificationError(AssertionError):
    pass


@dataclass
class State:
    status: str | None = None
    issued: bool = False
    ack_seen: bool = False
    status_disposed: bool = False
    ack_disposed: bool = False


@dataclass
class Oracle:
    states: dict[str, State] = field(default_factory=dict)

    def feed(self, events: list[str]):
        for line in events:
            words = line.strip().split()
            if not words:
                continue
            kind = words[0]
            if len(words) < 2 or kind not in {"ACK", "STATUS", "IGNORED", "DISPOSE_STATUS", "DISPOSE_ACK"}:
                raise VerificationError(f"Unrecognized event: {line}")
            session = words[1]
            current = self.states.setdefault(session, State())
            if kind == "IGNORED":
                if len(words) != 3: raise VerificationError(f"Bad IGNORE: {line}")
                continue
            if kind == "STATUS":
                if len(words) != 4: raise VerificationError(f"Bad STATUS: {line}")
                to, reason = words[2:]
                old = current.status
                if old is None:
                    allowed = to == "ISSUED" and reason == "SUCCEEDED"
                elif to == "ISSUED":
                    allowed = old in {"ISSUED", "COMMANDED", "EXECUTING"} and reason == "UPDATED"
                elif to == "COMMANDED":
                    allowed = old == "ISSUED" and reason == "SUCCEEDED"
                elif to == "EXECUTING":
                    allowed = old == "COMMANDED" and reason == "SUCCEEDED"
                elif to == "COMPLETED":
                    allowed = old == "EXECUTING" and reason == "SUCCEEDED"
                elif to == "CANCELED":
                    allowed = old in {"ISSUED", "COMMANDED", "EXECUTING"} and reason == "CANCELED"
                elif to == "FAILED":
                    failure_reasons = {
                        "ISSUED": {"VALIDATION_FAILED", "RESOURCE_FAILED", "INTERRUPTED", "TIMEOUT", "SERVICE_FAILED"},
                        "COMMANDED": {"RESOURCE_REJECTED", "INTERRUPTED", "TIMEOUT", "SERVICE_FAILED"},
                        "EXECUTING": {"OBJECTIVE_FAILED", "RESOURCE_FAILED", "INTERRUPTED", "TIMEOUT", "SERVICE_FAILED"},
                    }
                    allowed = reason in failure_reasons.get(old, set())
                else:
                    allowed = False
                if not allowed: raise VerificationError(f"Invalid ICD-like transition {old} -> {to}/{reason} at {line}")
                if to == "ISSUED":
                    current.issued = True
                    current.ack_seen = False
                if to in {"COMMANDED", "EXECUTING"} and not current.ack_seen:
                    raise VerificationError("ACK missing before COMMANDED/EXECUTING")
                current.status = to
            elif kind == "ACK":
                if len(words) != 2 or not current.issued or current.status != "ISSUED":
                    raise VerificationError(f"Unexpected ACK {line}")
                current.ack_seen = True
            elif kind == "DISPOSE_STATUS":
                if len(words) != 2 or current.status not in {"CANCELED", "FAILED", "COMPLETED"}:
                    raise VerificationError(f"Premature STATUS dispose {line}")
                current.status_disposed = True
            elif kind == "DISPOSE_ACK":
                if len(words) != 2 or not current.status_disposed:
                    raise VerificationError(f"ACK disposal before STATUS disposal {line}")
                current.ack_disposed = True

    def assert_complete(self):
        for session, s in self.states.items():
            if s.status_disposed != s.ack_disposed:
                raise VerificationError(f"Only half of instance cleanup for {session}")
