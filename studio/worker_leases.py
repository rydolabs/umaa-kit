"""Fenced leases for Studio workers; PostgreSQL SKIP LOCKED is required for multiworker."""
from contextlib import contextmanager
from datetime import timedelta, timezone
import logging
from threading import Event, Thread
from uuid import uuid4

from sqlalchemy import and_, or_, select
from .platform_db import Audit, Job, utcnow


def _expired(expiry, now):
    if expiry is None:
        return True
    if expiry.tzinfo is None:
        expiry = expiry.replace(tzinfo=timezone.utc)  # SQLite development fallback
    return expiry <= now


def claim_job(session_factory, *, worker_id=None, lease_seconds=300, max_attempts=3):
    if lease_seconds < 1 or max_attempts < 1:
        raise ValueError("Invalid worker lease configuration")
    owner = worker_id or str(uuid4())
    if len(owner) > 120:
        raise ValueError("Worker id too long")
    now = utcnow()
    with session_factory.begin() as db:
        expired = and_(Job.status == 'RUNNING',
                       or_(Job.lease_expires_at.is_(None), Job.lease_expires_at <= now))
        excluded = []
        while True:
            job = db.execute(select(Job).where(
                or_(Job.status == 'QUEUED', expired), ~Job.id.in_(excluded))
                .order_by(Job.created_at, Job.id)
                .with_for_update(skip_locked=True).limit(1)).scalar_one_or_none()
            if job is None:
                return None
            if job.attempts < max_attempts:
                break
            excluded.append(job.id)
            job.status = 'FAIL'
            job.error = 'Worker lease expired; maximum attempts exhausted'
            job.finished_at = now
            job.lease_token = None
            job.lease_owner = None
            job.lease_expires_at = None
            db.add(Audit(project_id=job.project_id, actor=job.created_by,
                         event='job.fail', subject=job.id))
            db.flush()
        job.status = 'RUNNING'
        job.attempts += 1
        job.lease_owner = owner
        job.lease_token = str(uuid4())
        job.lease_expires_at = now + timedelta(seconds=lease_seconds)
        return job.id, job.lease_token


def renew_lease(session_factory, job_id, token, *, lease_seconds=300):
    if lease_seconds < 1:
        raise ValueError("Invalid lease seconds")
    with session_factory.begin() as db:
        job = db.execute(select(Job).where(Job.id == job_id).with_for_update()).scalar_one_or_none()
        now = utcnow()
        if (job is None or job.status != 'RUNNING' or job.lease_token != token or
                _expired(job.lease_expires_at, now)):
            return False
        job.lease_expires_at = now + timedelta(seconds=lease_seconds)
        return True


def finish_job(session_factory, job_id, token, *, artifacts=(), error=None):
    """Only an unexpired lease owner may publish terminal state and artifact rows."""
    with session_factory.begin() as db:
        job = db.execute(select(Job).where(Job.id == job_id).with_for_update()).scalar_one_or_none()
        now = utcnow()
        if (job is None or job.status != 'RUNNING' or job.lease_token != token or
                _expired(job.lease_expires_at, now)):
            return False
        if error is None:
            for artifact in artifacts:
                db.add(artifact)
            job.status = 'PASS'
            job.error = None
            event = 'job.pass'
        else:
            job.status = 'FAIL'
            job.error = str(error)[:1000]
            event = 'job.fail'
        job.finished_at = now
        job.lease_token = None
        job.lease_owner = None
        job.lease_expires_at = None
        db.add(Audit(project_id=job.project_id, actor=job.created_by,
                     event=event, subject=job.id))
        return True


@contextmanager
def lease_heartbeat(session_factory, job_id, token, lease_seconds):
    stop = Event()
    def beat():
        while not stop.wait(max(0.25, lease_seconds / 3)):
            try:
                if not renew_lease(session_factory, job_id, token, lease_seconds=lease_seconds):
                    break
            except Exception:
                logging.exception("Lease heartbeat failed (id=%s)", job_id)
                break
    thread = Thread(target=beat, daemon=True)
    thread.start()
    try:
        yield
    finally:
        stop.set()
        thread.join(timeout=2)
