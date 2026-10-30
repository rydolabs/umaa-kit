-- UMAA Studio V6 PostgreSQL 15+ initial schema.

-- Apply once to a dedicated PostgreSQL database. EA repositories are NEVER changed.

BEGIN;

CREATE TABLE IF NOT EXISTS studio_schema_versions (version INTEGER PRIMARY KEY, applied_at TIMESTAMPTZ NOT NULL DEFAULT now());

CREATE TABLE studio_users (
	id VARCHAR(120) NOT NULL, 
	display_name VARCHAR(160) NOT NULL, 
	PRIMARY KEY (id)
);

CREATE TABLE studio_projects (
	id VARCHAR(36) NOT NULL, 
	name VARCHAR(160) NOT NULL, 
	created_by VARCHAR(120) NOT NULL, 
	created_at TIMESTAMP WITH TIME ZONE NOT NULL, 
	PRIMARY KEY (id), 
	FOREIGN KEY(created_by) REFERENCES studio_users (id)
);

CREATE TABLE studio_audit (
	id VARCHAR(36) NOT NULL, 
	project_id VARCHAR(36) NOT NULL, 
	actor VARCHAR(120) NOT NULL, 
	event VARCHAR(50) NOT NULL, 
	subject VARCHAR(120) NOT NULL, 
	created_at TIMESTAMP WITH TIME ZONE NOT NULL, 
	PRIMARY KEY (id), 
	FOREIGN KEY(project_id) REFERENCES studio_projects (id), 
	FOREIGN KEY(actor) REFERENCES studio_users (id)
);

CREATE TABLE studio_memberships (
	project_id VARCHAR(36) NOT NULL, 
	user_id VARCHAR(120) NOT NULL, 
	role VARCHAR(20) NOT NULL, 
	PRIMARY KEY (project_id, user_id), 
	CONSTRAINT membership_valid_role CHECK (role IN ('owner','editor','viewer')), 
	FOREIGN KEY(project_id) REFERENCES studio_projects (id) ON DELETE CASCADE, 
	FOREIGN KEY(user_id) REFERENCES studio_users (id)
);

CREATE TABLE studio_model_imports (
	id VARCHAR(36) NOT NULL, 
	project_id VARCHAR(36) NOT NULL, 
	source_sha256 VARCHAR(64) NOT NULL, 
	snapshot JSONB NOT NULL, 
	changes JSONB, 
	created_by VARCHAR(120) NOT NULL, 
	created_at TIMESTAMP WITH TIME ZONE NOT NULL, 
	PRIMARY KEY (id), 
	CONSTRAINT uq_project_source UNIQUE (project_id, source_sha256), 
	FOREIGN KEY(project_id) REFERENCES studio_projects (id), 
	FOREIGN KEY(created_by) REFERENCES studio_users (id)
);

CREATE TABLE studio_jobs (
	id VARCHAR(36) NOT NULL, 
	project_id VARCHAR(36) NOT NULL, 
	model_import_id VARCHAR(36) NOT NULL, 
	created_by VARCHAR(120) NOT NULL, 
	kind VARCHAR(12) NOT NULL, 
	options JSONB NOT NULL, 
	status VARCHAR(16) NOT NULL, 
	error TEXT, 
	created_at TIMESTAMP WITH TIME ZONE NOT NULL, 
	finished_at TIMESTAMP WITH TIME ZONE, 
	PRIMARY KEY (id), 
	CONSTRAINT job_valid_kind CHECK (kind IN ('docx','idl')), 
	CONSTRAINT job_valid_status CHECK (status IN ('QUEUED','RUNNING','PASS','FAIL')), 
	FOREIGN KEY(project_id) REFERENCES studio_projects (id), 
	FOREIGN KEY(model_import_id) REFERENCES studio_model_imports (id), 
	FOREIGN KEY(created_by) REFERENCES studio_users (id)
);

CREATE TABLE studio_artifacts (
	id VARCHAR(36) NOT NULL, 
	project_id VARCHAR(36) NOT NULL, 
	job_id VARCHAR(36) NOT NULL, 
	kind VARCHAR(32) NOT NULL, 
	filename VARCHAR(100) NOT NULL, 
	sha256 VARCHAR(64) NOT NULL, 
	size INTEGER NOT NULL, 
	created_at TIMESTAMP WITH TIME ZONE NOT NULL, 
	PRIMARY KEY (id), 
	FOREIGN KEY(project_id) REFERENCES studio_projects (id), 
	FOREIGN KEY(job_id) REFERENCES studio_jobs (id)
);

CREATE INDEX ix_studio_audit_project_id ON studio_audit (project_id);

CREATE INDEX ix_studio_model_imports_project_id ON studio_model_imports (project_id);

CREATE INDEX ix_studio_jobs_project_id ON studio_jobs (project_id);

CREATE INDEX ix_studio_artifacts_project_id ON studio_artifacts (project_id);

INSERT INTO studio_schema_versions(version) VALUES (1);

COMMIT;

