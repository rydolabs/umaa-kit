"""Project-scoped Studio HTTP API (development build, not production hardened)."""
import hashlib
import hmac
import json
import os
import tempfile
from pathlib import Path
from typing import Annotated, Literal

from fastapi import Depends, FastAPI, File, UploadFile, Header, HTTPException, Query
from fastapi.responses import Response
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError

from .qea import extract, model_diff
from .platform_db import (connect, Base, User, Project, Membership, ModelImport,
                          Job, Artifact, Audit, uid)

MAX_IMPORT_BYTES = 32 * 1024 * 1024
ROLE_LEVEL = {'viewer': 0, 'editor': 1, 'owner': 2}

class ProjectIn(BaseModel):
    name: str = Field(min_length=1, max_length=160)

class MembershipIn(BaseModel):
    userId: str = Field(min_length=1, max_length=120)
    role: Literal['viewer', 'editor']

class GenerateIn(BaseModel):
    modelImportId: str = Field(min_length=1, max_length=36)
    kind: Literal['docx','idl']
    title: str = Field(default='Model Inventory', max_length=160)
    module: str = Field(default='ProjectTypes', max_length=80)
    elementGuids: list[str] = Field(default_factory=list, max_length=50)


def create_app(*, database_url=None, storage_dir=None, identities=None, init_schema=False):
    database_url = database_url or os.getenv('STUDIO_DATABASE_URL')
    storage_dir = storage_dir or os.getenv('STUDIO_STORE_DIR')
    if not database_url or not storage_dir:
        raise RuntimeError('STUDIO_DATABASE_URL and STUDIO_STORE_DIR required')
    if identities is None:
        identities = json.loads(os.getenv('STUDIO_API_TOKENS', '{}'))
    if not isinstance(identities, dict) or not identities:
        raise RuntimeError('Nonempty STUDIO_API_TOKENS required (no anonymous access)')
    known = []
    for token, entry in identities.items():
        if (not isinstance(token, str) or len(token) < 16 or
                not isinstance(entry, dict) or not isinstance(entry.get('id'), str) or
                not isinstance(entry.get('name'), str)):
            raise RuntimeError('Bad Studio token configuration')
        known.append((hashlib.sha256(token.encode()).digest(), entry['id'], entry['name']))
    registered = {uid: name for _, uid, name in known}
    engine, factory = connect(database_url)
    if init_schema:
        # Development/test bootstrap only. Production: apply reviewed SQL migrations.
        Base.metadata.create_all(engine)
    app = FastAPI(title='UMAA Studio API', version='0.6.0', docs_url='/api/docs')
    app.state.session_factory = factory
    app.state.engine = engine
    app.state.store = Path(storage_dir).resolve()
    app.state.store.mkdir(parents=True, exist_ok=True)

    def principal(authorization: Annotated[str | None, Header()] = None):
        if not authorization or not authorization.startswith('Bearer '):
            raise HTTPException(401, 'Bearer token required', headers={'WWW-Authenticate':'Bearer'})
        digest = hashlib.sha256(authorization[7:].encode()).digest()
        matching = [(u, n) for expected,u,n in known if hmac.compare_digest(digest, expected)]
        if not matching:
            raise HTTPException(401, 'Invalid credentials')
        return matching[0]

    def membership(db, project_id, user_id, role='viewer'):
        m = db.get(Membership, (project_id, user_id))
        if not m or ROLE_LEVEL[m.role] < ROLE_LEVEL[role]:
            # Avoid revealing other projects to unauthorized users.
            raise HTTPException(404, 'Project unavailable')
        return m

    def record_user(db, user):
        if not db.get(User, user[0]):
            db.add(User(id=user[0], display_name=user[1]))
            db.flush()

    def serialize_import(model):
        return {'id': model.id, 'projectId': model.project_id,
                'sourceSha256': model.source_sha256, 'counts': model.snapshot['counts'],
                'validation': model.snapshot['modelValidation'],
                'issues': model.snapshot['issues'], 'changes': model.changes,
                'generationApproved': False, 'createdAt': model.created_at.isoformat()}

    def serialize_job(job):
        return {'id': job.id, 'projectId': job.project_id,
                'modelImportId': job.model_import_id, 'kind': job.kind,
                'status': job.status, 'error': job.error,
                'conformanceClaim': False, 'createdAt': job.created_at.isoformat()}

    @app.get('/api/v1/health')
    def health():
        return {'status': 'up', 'conformanceClaim': False}

    @app.post('/api/v1/projects', status_code=201)
    def create_project(data: ProjectIn, user=Depends(principal)):
        with factory.begin() as db:
            record_user(db,user)
            project = Project(name=data.name.strip(), created_by=user[0]); db.add(project); db.flush()
            db.add(Membership(project_id=project.id, user_id=user[0], role='owner'))
            db.add(Audit(project_id=project.id, actor=user[0], event='project.create', subject=project.id))
            return {'id': project.id, 'name': project.name, 'role':'owner'}

    @app.get('/api/v1/projects')
    def list_projects(user=Depends(principal)):
        with factory() as db:
            rows = db.execute(select(Project, Membership.role).join(Membership,
                Project.id == Membership.project_id).where(Membership.user_id==user[0])
                .order_by(Project.name,Project.id)).all()
            return [{'id':p.id,'name':p.name,'role':role} for p,role in rows]

    @app.post('/api/v1/projects/{project_id}/members', status_code=201)
    def add_member(project_id: str, data: MembershipIn, user=Depends(principal)):
        if data.userId not in registered:
            raise HTTPException(422, 'Unknown configured user')
        with factory.begin() as db:
            membership(db, project_id, user[0], 'owner')
            record_user(db,(data.userId, registered[data.userId]))
            m = db.get(Membership, (project_id, data.userId))
            if m:
                m.role = data.role
            else:
                db.add(Membership(project_id=project_id, user_id=data.userId, role=data.role))
            db.add(Audit(project_id=project_id, actor=user[0], event='member.set', subject=data.userId))
            return {'userId':data.userId,'role':data.role}

    @app.get('/api/v1/projects/{project_id}/members')
    def list_members(project_id: str, user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0])
            rows = db.execute(select(Membership).where(Membership.project_id==project_id)).scalars().all()
            return [{'userId':m.user_id,'role':m.role} for m in rows]

    @app.post('/api/v1/projects/{project_id}/models', status_code=201)
    async def upload_model(project_id: str, file: UploadFile = File(...), user=Depends(principal)):
        if not file.filename or not file.filename.lower().endswith('.qea'):
            raise HTTPException(422, 'Expected .qea file')
        with factory() as db:
            membership(db, project_id, user[0], 'editor')
        with tempfile.TemporaryDirectory(prefix='umaa-studio-import-') as td:
            target = Path(td) / 'frozen.qea'
            total = 0
            with target.open('wb') as output:
                while chunk := await file.read(1024 * 1024):
                    total += len(chunk)
                    if total > MAX_IMPORT_BYTES:
                        raise HTTPException(413, 'QEA upload exceeds 32 MiB safety cap')
                    output.write(chunk)
            try:
                snapshot = extract(target)
            except (ValueError, OSError) as exc:
                raise HTTPException(422, 'QEA import rejected: ' + str(exc)) from exc
        try:
            with factory.begin() as db:
                membership(db, project_id, user[0], 'editor')
                previous = db.execute(select(ModelImport).where(ModelImport.project_id==project_id)
                                      .order_by(ModelImport.created_at.desc(),ModelImport.id.desc())
                                      .limit(1)).scalar_one_or_none()
                existing = db.execute(select(ModelImport).where(ModelImport.project_id==project_id,
                              ModelImport.source_sha256==snapshot['sourceSha256'])).scalar_one_or_none()
                if existing:
                    return serialize_import(existing) | {'deduplicated':True}
                try:
                    changes = model_diff(previous.snapshot, snapshot) if previous else None
                except ValueError as exc:
                    raise HTTPException(422, 'Model GUID diff rejected: '+str(exc)) from exc
                model = ModelImport(project_id=project_id, created_by=user[0],
                                    source_sha256=snapshot['sourceSha256'], snapshot=snapshot, changes=changes)
                db.add(model); db.flush()
                db.add(Audit(project_id=project_id, actor=user[0], event='model.import', subject=model.id))
                result = serialize_import(model) | {'deduplicated':False}
            return result
        except IntegrityError:
            # Concurrent identical imports may race at the unique constraint.
            # Return the already committed immutable snapshot, never a 500.
            with factory() as db:
                membership(db, project_id, user[0], 'editor')
                existing = db.execute(select(ModelImport).where(ModelImport.project_id==project_id,
                            ModelImport.source_sha256==snapshot['sourceSha256'])).scalar_one_or_none()
                if existing:
                    return serialize_import(existing) | {'deduplicated':True}
            raise HTTPException(409, 'Concurrent model update; retry')

    @app.get('/api/v1/projects/{project_id}/models')
    def list_models(project_id: str, user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0])
            models = db.execute(select(ModelImport).where(ModelImport.project_id==project_id)
                                .order_by(ModelImport.created_at.desc(),ModelImport.id.desc())).scalars().all()
            return [serialize_import(model) for model in models]

    @app.get('/api/v1/projects/{project_id}/models/{model_id}/elements')
    def list_elements(project_id: str, model_id: str,
                      kind: Literal['packages','elements','connectors','attributes','diagrams']='elements',
                      q: str='', offset: int=Query(0,ge=0), limit: int=Query(100,ge=1,le=500),
                      user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0])
            model = db.get(ModelImport, model_id)
            if not model or model.project_id != project_id:
                raise HTTPException(404, 'Model unavailable')
            records = model.snapshot['records'][kind]
            if q:
                records = [r for r in records if q.lower() in str(r.get('Name','')).lower()
                           or q.lower() in str(r.get('ea_guid','')).lower()]
            return {'total':len(records),'items':records[offset:offset+limit],
                    'modelImportId':model_id,'kind':kind}

    @app.post('/api/v1/projects/{project_id}/jobs', status_code=202)
    def enqueue(project_id: str, data: GenerateIn, user=Depends(principal)):
        with factory.begin() as db:
            membership(db, project_id, user[0], 'editor')
            model = db.get(ModelImport, data.modelImportId)
            if not model or model.project_id != project_id:
                raise HTTPException(404, 'Model unavailable')
            if model.snapshot.get('issues'):
                raise HTTPException(422, 'Model has structural issues; cannot generate')
            if data.kind == 'idl' and not data.elementGuids:
                raise HTTPException(422, 'IDL selection requires element GUIDs')
            options = {'title':data.title} if data.kind=='docx' else {
                'module':data.module,'elementGuids':data.elementGuids}
            job = Job(project_id=project_id, model_import_id=model.id, kind=data.kind,
                      options=options, created_by=user[0])
            db.add(job); db.flush()
            db.add(Audit(project_id=project_id, actor=user[0], event='job.queue', subject=job.id))
            return serialize_job(job)

    @app.get('/api/v1/projects/{project_id}/jobs')
    def list_jobs(project_id: str, user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0])
            jobs = db.execute(select(Job).where(Job.project_id==project_id)
                              .order_by(Job.created_at.desc(),Job.id.desc()).limit(200)).scalars().all()
            return [serialize_job(job) for job in jobs]

    @app.get('/api/v1/projects/{project_id}/jobs/{job_id}')
    def get_job(project_id: str, job_id: str, user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0])
            job = db.get(Job, job_id)
            if not job or job.project_id!=project_id:
                raise HTTPException(404, 'Job unavailable')
            return serialize_job(job)

    @app.get('/api/v1/projects/{project_id}/artifacts')
    def list_artifacts(project_id: str, user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0])
            rows = db.execute(select(Artifact).where(Artifact.project_id==project_id)
                              .order_by(Artifact.created_at.desc(),Artifact.id.desc()).limit(200)).scalars().all()
            return [{'id':a.id,'jobId':a.job_id,'kind':a.kind,'filename':a.filename,
                     'sha256':a.sha256,'size':a.size} for a in rows]

    @app.get('/api/v1/projects/{project_id}/artifacts/{artifact_id}/download')
    def download(project_id: str, artifact_id: str, user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0])
            a = db.get(Artifact, artifact_id)
            if not a or a.project_id!=project_id:
                raise HTTPException(404,'Artifact unavailable')
            digest, filename = a.sha256, a.filename
        # Content address and filename come from the server, not a request path.
        path = app.state.store / digest[:2] / digest
        if not path.is_file():
            raise HTTPException(503, 'Artifact missing from storage')
        payload = path.read_bytes()
        if hashlib.sha256(payload).hexdigest()!=digest:
            raise HTTPException(503, 'Artifact integrity mismatch')
        return Response(payload, media_type='application/octet-stream',
                        headers={'Content-Disposition':f'attachment; filename="{filename}"',
                                 'Cache-Control':'no-store'})

    @app.get('/api/v1/projects/{project_id}/audit')
    def list_audit(project_id: str, user=Depends(principal)):
        with factory() as db:
            membership(db, project_id, user[0], 'owner')
            rows = db.execute(select(Audit).where(Audit.project_id==project_id)
                              .order_by(Audit.created_at.desc()).limit(200)).scalars().all()
            return [{'event':a.event,'actor':a.actor,'subject':a.subject,
                     'createdAt':a.created_at.isoformat()} for a in rows]
    static_root = Path(__file__).resolve().parent / 'frontend' / 'public'
    if static_root.is_dir():
        app.mount('/studio', StaticFiles(directory=static_root, html=True), name='studio-ui')
    return app


def from_env():
    return create_app()
