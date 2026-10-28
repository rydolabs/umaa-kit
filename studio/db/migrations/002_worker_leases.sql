-- M7: leased worker claim and recovery. Apply after 001, once, on a dedicated DB.
BEGIN;
ALTER TABLE studio_jobs ADD COLUMN lease_owner VARCHAR(120);
ALTER TABLE studio_jobs ADD COLUMN lease_token VARCHAR(36);
ALTER TABLE studio_jobs ADD COLUMN lease_expires_at TIMESTAMPTZ;
ALTER TABLE studio_jobs ADD COLUMN attempts INTEGER NOT NULL DEFAULT 0;
ALTER TABLE studio_jobs ADD CONSTRAINT job_attempts_nonnegative CHECK (attempts >= 0);
CREATE INDEX ix_studio_jobs_claim ON studio_jobs (status, lease_expires_at, created_at);
INSERT INTO studio_schema_versions(version) VALUES (2);
COMMIT;
