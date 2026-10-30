# UMAA Studio V6 — executable development workspace

**Scope:** isolated multi-user project API, immutable EA model imports, read-only
Model Explorer, project-extension draft IDL and draft DOCX generation, queued
jobs, content-addressed artifacts, role checks and audit events.

## Run local development instance (SQLite only)

```bash
export STUDIO_DATABASE_URL=sqlite:////tmp/umaa-studio-dev.sqlite
export STUDIO_STORE_DIR=/tmp/umaa-studio-private-store
# Create a unique 32+ byte high-entropy value for each local user; the example
# is deliberately NOT a usable default and must not be committed.
export STUDIO_API_TOKENS='{"REPLACE_WITH_RANDOM_TOKEN_32_CHARS":{"id":"dev1","name":"Developer"}}'
python -m studio.platform_cli init-dev
uvicorn studio.platform_api:from_env --factory --host 127.0.0.1 --port 8000
# Open http://127.0.0.1:8000/studio/ in a browser
# In separate shell after requesting DOCX/IDL:
python -m studio.platform_cli work-until-empty
```

For PostgreSQL deployment install `studio/requirements-platform.txt`, configure
`postgresql+psycopg://...` and run `studio/db/migrations/001_initial_postgresql.sql`
with `psql`; do not use `init-dev` as a production migration tool. The
PostgreSQL driver/server are not provided by this offline handoff.

## Project roles

- `owner`: import/generate/read, manage registered team members, audit view.
- `editor`: import/generate/read; cannot grant permissions.
- `viewer`: inspect models, jobs, artifacts; cannot import or generate.

All project routes check membership server-side. User identities are configured
outside the DB through `STUDIO_API_TOKENS`. Token matching uses a SHA-256 digest
and constant-time comparison, but this **is not** a production SSO/IdP.

## Architectural constraints

- **No shared DB with Sparx EA.** QEA imports are read-only. EA GUIDs remain in
  snapshots, and a re-import creates a separately versioned model snapshot.
- **Non-normative output only.** Official UMAA IDL is not recreated. Generated
  project-extension IDL is not marked compiled/approved. DOCX is inventory only.
- A separate worker picks up `QUEUED` jobs with SQLAlchemy `FOR UPDATE SKIP LOCKED`
  on PostgreSQL; job status `PASS` means draft generation + self-verification,
  **not** UMAA compliance or `idlc` compilation.
- Artifacts are stored as content-addressed bytes under a protected directory;
  every download repeats SHA-256 checks. Files do not become public URLs.
- QEA schema compatibility was tested using synthetic EA-like SQLite fixtures
  only, not a real Sparx QEA file.
- Local processing has neither OS sandboxing nor per-worker resource budgets.
  Do not accept hostile uploads on an exposed server until it is isolated.
- No official requirement-based verification, RTI, Cyclone DDS runtime, or
  production-grade auth/SSO is implied by this milestone.

## CI / tests

```bash
python -m unittest discover -s studio/tests -v
python -m unittest discover -s verifier/tests -v
python -m tools.ci_verify
cd studio/frontend && tsc -p .
```

## Acceptance evidence

`studio/tests/test_platform_api.py` validates auth isolation, import/dedup,
change detection, DOCX/IDL job lifecycle, draft-only manifest and tampered
artifact rejection using a real SQLite database, FastAPI TestClient and the
worker. Full PostgreSQL integration requires an external DB service and driver.


## Component Designer backend foundation — 2026-10-30 (Draft)

The future React Flow designer can use the **same vetted Service Contract
catalog and assembly validator as the CLI**. The current V6 browser is
still a model explorer, not a React Flow editor.

New project-scoped, **read-only/stateless** endpoints (Bearer-token
authentication and existing project membership checks apply):

~~~http
GET /api/v1/projects/{projectId}/service-contracts
POST /api/v1/projects/{projectId}/assemblies/validate
Content-Type: application/json
Authorization: Bearer <project-user-token>
~~~

The GET response is a reviewed, source-SHA-pinned snapshot with 53 services,
113 directional Topic identity records, Operation names and explicit evidence
flags. This is **not** official key/QoS/runtime-complete evidence.

The POST body is a **Component Assembly Manifest**, exactly matching
`schemas/component-assembly-v1.schema.json`, not React Flow nodes or an
entire EA/QEA snapshot. Successful validation returns:

~~~json
{
  "status": "VALID_ASSEMBLY_NOT_NORMATIVE_RUNTIME",
  "componentId": "demo_engine",
  "services": 1,
  "catalogPinned": true,
  "codegenReady": true,
  "officialRuntimeClaim": false,
  "normativeQosClaim": false
}
~~~

`codegenReady` refers **only to currently implemented synthetic Demo
Provider** generation. Standard UMAA service selection with the correct
catalogPin can pass assembly validation but returns
`codegenReady: false` and still fails actual official service generation.
Invalid/unknown/unpinned/stale assemblies return HTTP 422, oversized
assemblies return HTTP 413, and unauthorized or nonmember requests never
reveal project data (401/404). Existing project viewers can inspect and
validate a draft because these calls do not mutate any resource.

There is **no write endpoint**, no storage of a Component Assembly,
no arbitrary CMake/IDL execution and no React Flow graph persistence.
Model-derived normative Service Contracts, accepted real QEA files,
production OIDC, sandboxed build worker and UI modernization remain OPEN.

See [Component generation contract](../docs/COMPONENT_PROJECT_GENERATION.md).
