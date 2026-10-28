# Development status (as of local V3 increment)

## Workstreams

| Track | Current locally demonstrated functionality | Not yet delivered |
|---|---|---|
| `sdk/`, `dds/` | C++17 SDK and fake-DDS ABI unit tests, earlier scoped official DDS CI evidence | complete services/UMAA normative conformity, fresh real DDS run, RTI |
| `simulator/` | independent stdio oracle and existing DDS test supervisors | full formal independently approved verification contracts |
| `verifier/` | V3 local CLI, suites, evidence integrity and additional expected markers | local actual DDS run, reviewed normative contracts, secure worker, PostgreSQL/UI |
| `studio/` | read-only EA-like SQLite import and GUID diff PoC | real EA QEA acceptance, PostgreSQL, React, approved IDL/DOCX generation |

## Next gates (not passed)

- Obtain a representative QEA file in a controlled test environment and
  validate imported GUIDs, attributes, Tagged Values, diagrams and relations
  against a reference EA export.
- Approve official UMAA component/scope and traceable ICD/Compliance
  requirements with independent reviewers; do not auto-approve using
  development simulator results.
- Install Cyclone DDS and approved official TypeSupport in a physically/logically
  isolated authorized lab; rerun the packaged DDS suites with captured results.
- Introduce application-layer PostgreSQL persistence and authentication,
  isolated workers, Web UI and DOCX template management before multi-user
  deployment. Use verified snapshots and keep EA repository isolated.

Local test completion is not product completion.
