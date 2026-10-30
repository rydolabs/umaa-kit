"""Studio platform persistence. PostgreSQL in deployment; SQLite for isolated tests.

Not a replacement for Sparx EA: QEA snapshots are immutable, with Studio metadata
and generated artefacts held in a separate database / private file store.
"""
from datetime import datetime, timezone
from uuid import uuid4
from sqlalchemy import (create_engine, Column, String, DateTime, ForeignKey, Integer,
                        UniqueConstraint, CheckConstraint, Index, JSON, Text)
from sqlalchemy.orm import declarative_base, sessionmaker
from sqlalchemy.dialects.postgresql import JSONB

Base = declarative_base()
J = JSON().with_variant(JSONB(), 'postgresql')

def uid():
    return str(uuid4())

def utcnow():
    return datetime.now(timezone.utc)

class User(Base):
    __tablename__ = 'studio_users'
    id = Column(String(120), primary_key=True)
    display_name = Column(String(160), nullable=False)

class Project(Base):
    __tablename__ = 'studio_projects'
    id = Column(String(36), primary_key=True, default=uid)
    name = Column(String(160), nullable=False)
    created_by = Column(String(120), ForeignKey('studio_users.id'), nullable=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

class Membership(Base):
    __tablename__ = 'studio_memberships'
    project_id = Column(String(36), ForeignKey('studio_projects.id', ondelete='CASCADE'), primary_key=True)
    user_id = Column(String(120), ForeignKey('studio_users.id'), primary_key=True)
    role = Column(String(20), nullable=False)
    __table_args__ = (CheckConstraint("role IN ('owner','editor','viewer')", name='membership_valid_role'),)

class ModelImport(Base):
    __tablename__ = 'studio_model_imports'
    id = Column(String(36), primary_key=True, default=uid)
    project_id = Column(String(36), ForeignKey('studio_projects.id'), nullable=False, index=True)
    source_sha256 = Column(String(64), nullable=False)
    snapshot = Column(J, nullable=False)
    changes = Column(J)
    created_by = Column(String(120), ForeignKey('studio_users.id'), nullable=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    __table_args__ = (UniqueConstraint('project_id','source_sha256', name='uq_project_source'),)

class Job(Base):
    __tablename__ = 'studio_jobs'
    id = Column(String(36), primary_key=True, default=uid)
    project_id = Column(String(36), ForeignKey('studio_projects.id'), nullable=False, index=True)
    model_import_id = Column(String(36), ForeignKey('studio_model_imports.id'), nullable=False)
    created_by = Column(String(120), ForeignKey('studio_users.id'), nullable=False)
    kind = Column(String(12), nullable=False)
    options = Column(J, nullable=False)
    status = Column(String(16), nullable=False, default='QUEUED')
    error = Column(Text)
    lease_owner = Column(String(120))
    lease_token = Column(String(36))
    lease_expires_at = Column(DateTime(timezone=True))
    attempts = Column(Integer, nullable=False, default=0)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)
    finished_at = Column(DateTime(timezone=True))
    __table_args__ = (
        CheckConstraint("attempts >= 0", name='job_attempts_nonnegative'),
        CheckConstraint("kind IN ('docx','idl')", name='job_valid_kind'),
        CheckConstraint("status IN ('QUEUED','RUNNING','PASS','FAIL')", name='job_valid_status'),
        Index('ix_studio_jobs_claim', 'status', 'lease_expires_at', 'created_at'),
    )

class Artifact(Base):
    __tablename__ = 'studio_artifacts'
    id = Column(String(36), primary_key=True, default=uid)
    project_id = Column(String(36), ForeignKey('studio_projects.id'), nullable=False, index=True)
    job_id = Column(String(36), ForeignKey('studio_jobs.id'), nullable=False)
    kind = Column(String(32), nullable=False)
    filename = Column(String(100), nullable=False)
    sha256 = Column(String(64), nullable=False)
    size = Column(Integer, nullable=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

class Audit(Base):
    __tablename__ = 'studio_audit'
    id = Column(String(36), primary_key=True, default=uid)
    project_id = Column(String(36), ForeignKey('studio_projects.id'), nullable=False, index=True)
    actor = Column(String(120), ForeignKey('studio_users.id'), nullable=False)
    event = Column(String(50), nullable=False)
    subject = Column(String(120), nullable=False)
    created_at = Column(DateTime(timezone=True), default=utcnow, nullable=False)

def connect(url):
    args = {'check_same_thread': False} if url.startswith('sqlite:') else {}
    engine = create_engine(url, connect_args=args, pool_pre_ping=True)
    return engine, sessionmaker(bind=engine, expire_on_commit=False)
