# Studio Web UI (V6 development build)

- Source: `src/app.tsx` (React + TypeScript; no EA editing).
- Compiled offline with `tsc -p studio/frontend`, creating `public/app.js`.
- Development UI available at `http://localhost:8000/studio/` when FastAPI is running.
- The bundled React/ReactDOM 16 UMD files are **legacy offline proof-of-execution**
  assets from an MIT licensed distribution in the available test environment.
  They MUST be replaced with a supported React release and modern build/dependency
  lock before production. No npm registry access was available during V6.
- Authentication is a manually supplied bearer token held only in tab memory.
  No token is persisted to localStorage or cookies by the UI. Require HTTPS,
  an identity provider, CSRF review, worker isolation, and audited deployment
  controls before real multi-user production deployment.
- Custom uploaded DOCX templates, diagrams and official conformance rendering
  are out of scope in V6. Draft inventory is generated server-side.
