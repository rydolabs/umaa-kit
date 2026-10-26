# Verification evidence

The committed `github_runner_20261023.json` is a historical evidence manifest,
not proof that the newest code was exercised. Fresh runs are identified by
exact Git commit and GitHub Actions run IDs; see [validation index](../docs/README.md).

`evidence/ci/` is generated locally by `python tools/ci_verify.py` and is
intentionally gitignored to avoid committing machine-dependent build logs.
CI uploads its own verification artifacts to the associated GitHub run.

Official UMAA IDL and temporary/generated TypeSupport must never be stored
here or committed to the repository.
