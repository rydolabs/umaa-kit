# Studio Persistence

EA's QEA file and EA's own repository are **never** shared with this database.
Use an independently administered PostgreSQL database for Studio.

Apply `migrations/001_initial_postgresql.sql` exactly once using `psql -v ON_ERROR_STOP=1 -f ...`.
This migration targets PostgreSQL 15+; it is not an EA model schema.
Production migration sequencing/rollback must be managed by the deployer.

For isolated development tests `python -m studio.platform_cli init-dev` creates
matching SQLAlchemy tables on SQLite. Do not run that helper against production.

The JSON snapshots are immutable by application convention and source content
SHA-256. Database superusers can rewrite rows; add DB audit/backups to harden.

A PG database server and psycopg package are required for live PG validation;
both were unavailable during the offline V6 build.
