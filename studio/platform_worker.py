"""Separately invoked Studio development worker; no shell evaluation.

IMPORTANT: local worker is not a security sandbox. Deploy an isolated worker
with OS-level resource/network controls before enabling untrusted templates.
"""
import io
import json
import logging
import hashlib
import shutil
import tempfile
import zipfile
from uuid import uuid4
from pathlib import Path
from sqlalchemy import select
from .platform_db import Job, ModelImport, Artifact, Audit, utcnow
from .worker_leases import claim_job, renew_lease, finish_job, lease_heartbeat
from .documents import generate as generate_docx, verify_document
from .idl import draft_profile, generate_idl, verify_idl


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


def archive_files(folder):
    """Stable bytes for project-extension IDL drafts and manifests."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        for path in sorted(folder.rglob('*')):
            if not path.is_file():
                continue
            entry = zipfile.ZipInfo(path.relative_to(folder).as_posix(), date_time=(1980,1,1,0,0,0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o644 << 16
            z.writestr(entry, path.read_bytes())
    return buf.getvalue()


def run_one(session_factory, store: Path, *, worker_id=None, lease_seconds=300):
    """One leased job; stale workers cannot commit terminal status or artifacts."""
    claim = claim_job(session_factory, worker_id=worker_id, lease_seconds=lease_seconds)
    if claim is None:
        return None
    job_id, token = claim
    with lease_heartbeat(session_factory, job_id, token, lease_seconds):
        return _process_claim(session_factory, store, job_id, token)


def _process_claim(session_factory, store: Path, job_id, token):
    with session_factory() as db:
        job = db.get(Job, job_id)
        model = db.get(ModelImport, job.model_import_id)
        try:
            if not model or model.project_id != job.project_id:
                raise ValueError('Model/project consistency check failed')
            if model.snapshot.get('generationApproved') is not False:
                raise ValueError('Unexpected approved model snapshot')
            with tempfile.TemporaryDirectory(prefix='umaa-studio-job-') as td:
                base = Path(td)
                snap = base / 'snapshot.json'
                snap.write_text(json.dumps(model.snapshot, ensure_ascii=False) + '\n', encoding='utf-8')
                if job.kind == 'docx':
                    path, man = base / 'inventory.docx', base / 'manifest.json'
                    generate_docx(snap, path, man, title=job.options.get('title', 'Model Inventory'))
                    errors = verify_document(snap, path, man)
                    if errors:
                        raise ValueError('DOCX verification failed: ' + '; '.join(errors))
                    outputs = [('docx', 'inventory.docx', path.read_bytes()),
                               ('manifest', 'manifest.json', man.read_bytes())]
                elif job.kind == 'idl':
                    prof = base / 'profile.json'
                    draft_profile(snap, job.options['module'], job.options['elementGuids'], prof)
                    out = base / 'generated'
                    generate_idl(snap, prof, out)
                    errors = verify_idl(snap, prof, out)
                    if errors:
                        raise ValueError('IDL verification failed: ' + '; '.join(errors))
                    outputs = [('idl-draft', 'extension-idl.zip', archive_files(out))]
                else:
                    raise ValueError('Unsupported job kind')
                # Files are content-addressed and never based on user filenames.
                saved = []
                for kind, filename, payload in outputs:
                    digest = sha_bytes(payload)
                    destination = store / digest[:2] / digest
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    if destination.exists() and sha_bytes(destination.read_bytes()) != digest:
                        raise ValueError('Existing content-addressed artifact was corrupted')
                    if not destination.exists():
                        temporary = destination.with_name(destination.name + '.tmp-' + str(uuid4()))
                        with temporary.open('xb') as fp:
                            fp.write(payload)
                        temporary.replace(destination)
                    saved.append(Artifact(project_id=job.project_id, job_id=job.id,
                                          kind=kind, filename=filename,
                                          sha256=digest, size=len(payload)))
            # Commit metadata only while this worker still owns an unexpired lease.
            finish_job(session_factory, job_id, token, artifacts=saved)
        except Exception as exc:
            logging.exception('Studio job failed (id=%s)', job_id)
            finish_job(session_factory, job_id, token,
                       error=(type(exc).__name__ + ': ' + str(exc)))
    return job_id
