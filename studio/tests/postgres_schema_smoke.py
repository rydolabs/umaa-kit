"""GitHub Actions-only PostgreSQL DDL and basic constraint smoke checks.

This does NOT imply that the Studio HTTP API was tested using PostgreSQL.
"""
import os
from pathlib import Path

import psycopg
from psycopg import errors
from psycopg.types.json import Jsonb


def reject(conn, sql, params, error_type):
    try:
        with conn.transaction():
            conn.execute(sql, params)
    except error_type:
        return
    raise AssertionError("Expected database constraint violation: " + error_type.__name__)


def main():
    dsn = os.environ["STUDIO_TEST_DATABASE_URL"]
    sql_file = Path(__file__).resolve().parents[1] / "db/migrations/001_initial_postgresql.sql"
    with psycopg.connect(dsn, autocommit=True) as conn:
        conn.execute(sql_file.read_text(encoding="utf-8"), prepare=False)
        expected = {
            "studio_schema_versions", "studio_users", "studio_projects",
            "studio_audit", "studio_memberships", "studio_model_imports",
            "studio_jobs", "studio_artifacts",
        }
        tables = {r[0] for r in conn.execute(
            "SELECT tablename FROM pg_tables WHERE schemaname='public'"
        ).fetchall()}
        assert expected.issubset(tables), expected - tables
        assert conn.execute("SELECT version FROM studio_schema_versions").fetchall() == [(1,)]
        conn.execute("INSERT INTO studio_users (id, display_name) VALUES (%s,%s)", ("ci-user", "CI User"))
        conn.execute(
            "INSERT INTO studio_projects (id,name,created_by,created_at) VALUES (%s,%s,%s,now())",
            ("ci-project", "CI Project", "ci-user"),
        )
        conn.execute(
            "INSERT INTO studio_memberships (project_id,user_id,role) VALUES (%s,%s,%s)",
            ("ci-project", "ci-user", "owner"),
        )
        reject(
            conn,
            "INSERT INTO studio_memberships (project_id,user_id,role) VALUES (%s,%s,%s)",
            ("ci-project", "ci-user", "administrator"),
            errors.CheckViolation,
        )
        reject(
            conn,
            "INSERT INTO studio_memberships (project_id,user_id,role) VALUES (%s,%s,%s)",
            ("unknown-project", "ci-user", "viewer"),
            errors.ForeignKeyViolation,
        )
        conn.execute(
            """INSERT INTO studio_model_imports
               (id,project_id,source_sha256,snapshot,created_by,created_at)
               VALUES (%s,%s,%s,%s,%s,now())""",
            ("ci-import", "ci-project", "a"*64, Jsonb({"packages": []}), "ci-user"),
        )
        reject(
            conn,
            """INSERT INTO studio_model_imports
               (id,project_id,source_sha256,snapshot,created_by,created_at)
               VALUES (%s,%s,%s,%s,%s,now())""",
            ("ci-duplicate", "ci-project", "a"*64, Jsonb({"packages": []}), "ci-user"),
            errors.UniqueViolation,
        )
    print("PASS: PostgreSQL 16 migration, 8 tables, version, JSONB, role, FK, and dedup constraints")


if __name__ == "__main__":
    main()
