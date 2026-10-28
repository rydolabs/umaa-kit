# UMAA Kit V6 — Studio Web / Data Foundation (2026-10-28)

## Implemented in the V6 development scope

- Separate `studio/` SQL/HTTP platform; NOT Sparx EA's own database.
- React/TypeScript read-only model explorer; local offline demonstration bundles
  legacy React 16 only for smoke validation (upgrade before deployment).
- Project member roles (owner/editor/viewer) and project-scoped API access.
- Immutable QEA import snapshots keyed by SHA-256 and EA GUID model diffs.
- DOCX inventory and project-extension IDL draft jobs, non-normative only.
- Independent local worker, artifact content hashes, audit and download checks.
- PostgreSQL-specific initial migration SQL; SQLite used for integration tests.
- Studio 46 unit/integration tests, Verifier 44 tests, and original SDK CI pass.
- Chromium in-memory bridge smoke: React events -> FastAPI TestClient -> worker.
  A standalone navigation to localhost was blocked by sandbox browser policy.

## Not completed / required external acceptance

1. Real Sparx EA `.qea` compatibility and customer DOCX template acceptance.
2. Live PostgreSQL migrations, concurrent project tests and backup/restore.
3. Current supported React build, production IdP/RBAC/SSO and security review.
4. Isolated execution workers, job crash recovery/retry/limits/monitoring.
5. Real Cyclone DDS `idlc`, actual DDS network integration, RTI interoperability.
6. Approved UMAA Component/ICD/Compliance requirement links and formal verdicts.
7. Verifier Web UI, extended protocols, production deployment and audit controls.

## Verification distinctions

- `PASS` on a Studio job indicates successful **draft generation and
  self-consistency checks**, NOT `idlc` compilation or UMAA conformance.
- Source and artifact SHA-256 demonstrate byte consistency, not identity or
  authenticity of proprietary EA models.
- Original SDK/Simulator sources are preserved unchanged by V6.
- No GitHub remote write was performed for this local handoff.
