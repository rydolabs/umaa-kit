"""Real PostgreSQL M7 E2E; disposable CI service container ONLY.

Apply 001 and 002, exercise HTTP RBAC/upload, PostgreSQL multi-worker claims,
content-addressed downloads, stale fencing, retries and failure recovery.
"""
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
from datetime import timedelta
from pathlib import Path
import hashlib
import os
import sqlite3
import tempfile

import psycopg
from fastapi.testclient import TestClient
from sqlalchemy import select
from studio.platform_api import create_app
from studio.platform_db import Job, Artifact, Audit, utcnow
from studio.platform_worker import claim_job, finish_job, renew_lease, run_one

TOKENS={'a'*32:{'id':'alice-ci','name':'Alice'},'b'*32:{'id':'bob-ci','name':'Bob'}}
ALICE={'Authorization':'Bearer '+'a'*32}
BOB={'Authorization':'Bearer '+'b'*32}

def check(ok, context):
    if not ok:
        raise AssertionError(context)

def fixture(path):
    with closing(sqlite3.connect(path)) as db:
        db.executescript("""
        CREATE TABLE t_package (Package_ID INTEGER, ea_guid TEXT, Name TEXT, Parent_ID INTEGER);
        CREATE TABLE t_object (Object_ID INTEGER, ea_guid TEXT, Name TEXT, Object_Type TEXT, Package_ID INTEGER);
        CREATE TABLE t_connector (Connector_ID INTEGER, ea_guid TEXT, Name TEXT, Connector_Type TEXT, Start_Object_ID INTEGER, End_Object_ID INTEGER);
        CREATE TABLE t_attribute (ID INTEGER, ea_guid TEXT, Object_ID INTEGER, Name TEXT, Type TEXT, Classifier TEXT, Pos INTEGER);
        CREATE TABLE t_diagram (Diagram_ID INTEGER, ea_guid TEXT, Name TEXT, Package_ID INTEGER);
        INSERT INTO t_package VALUES (1,'p1','System',0);
        INSERT INTO t_object VALUES (7,'o7','Engine','DataType',1);
        INSERT INTO t_attribute VALUES (1,'a1',7,'speed','double',NULL,0);
        """)
        db.commit()
    return path

def main():
    url=os.environ['STUDIO_TEST_DATABASE_URL']
    check(url.startswith('postgresql+psycopg://'), 'Dedicated PostgreSQL DSN required')
    sql=Path(__file__).resolve().parents[1]/'db'/'migrations'
    with psycopg.connect(url.replace('postgresql+psycopg://','postgresql://',1),autocommit=True) as con:
        for name in ('001_initial_postgresql.sql','002_worker_leases.sql'):
            con.execute((sql/name).read_text(encoding='utf-8'),prepare=False)
        check(con.execute('SELECT version FROM studio_schema_versions ORDER BY version').fetchall()==[(1,),(2,)],'Migration versions 1 and 2')
    with tempfile.TemporaryDirectory() as td:
        root=Path(td)
        app=create_app(database_url=url,storage_dir=root/'artifacts',identities=TOKENS)
        with TestClient(app) as client:
            def post(path, **kwargs):
                return client.post(path,headers=ALICE,**kwargs)
            response=post('/api/v1/projects',json={'name':'Postgres E2E'})
            check(response.status_code==201,response.text)
            pid=response.json()['id']
            prefix=f'/api/v1/projects/{pid}'
            check(client.get(prefix+'/models',headers=BOB).status_code==404,'Cross-project isolation')
            check(post(prefix+'/members',json={'userId':'bob-ci','role':'viewer'}).status_code==201,'Role registration')
            source=fixture(root/'model.qea')
            def upload(_):
                return post(prefix+'/models',files={'file':('model.qea',source.read_bytes(),'application/octet-stream')})
            with ThreadPoolExecutor(max_workers=4) as ex:
                rows=list(ex.map(upload,range(4)))
            check(all(r.status_code in (200,201) for r in rows),f'Concurrent imports: {[r.status_code for r in rows]}')
            ids={r.json()['id'] for r in rows}
            check(len(ids)==1,'Concurrent hash-based dedup')
            model_id=ids.pop()
            check(client.get(prefix+'/models',headers=BOB).status_code==200,'Viewer may read')
            jobs=[]
            for i in range(12):
                request={'modelImportId':model_id,'kind':'docx','title':f'Inventory {i}'}
                if i==0: request={'modelImportId':model_id,'kind':'idl','module':'ProjectTypes','elementGuids':['o7']}
                r=post(prefix+'/jobs',json=request)
                check(r.status_code==202,r.text)
                jobs.append(r.json()['id'])
            check(client.post(prefix+'/jobs',headers=BOB,json={'modelImportId':model_id,'kind':'docx'}).status_code==404,'Viewer forbidden to enqueue')
            def worker(index):
                handled=[]
                for _ in range(16):
                    job=run_one(app.state.session_factory,app.state.store,worker_id=f'worker-{index}')
                    if job is None: break
                    handled.append(job)
                return handled
            with ThreadPoolExecutor(max_workers=4) as ex:
                handled=[j for batch in ex.map(worker,range(4)) for j in batch]
            check(sorted(handled)==sorted(jobs),'Four workers claim 12 jobs exactly once')
            with app.state.session_factory() as db:
                found=db.execute(select(Job).where(Job.id.in_(jobs))).scalars().all()
                check(len(found)==12 and all(j.status=='PASS' and j.attempts==1 for j in found),'All 12 jobs completed once')
                artifacts=db.execute(select(Artifact).where(Artifact.project_id==pid)).scalars().all()
                check(len(artifacts)==23,'11 DOCX pairs and one IDL ZIP')
                events=db.execute(select(Audit).where(Audit.project_id==pid,Audit.event=='job.pass')).all()
                check(len(events)==12,'Exactly one success audit per job')
            for entry in client.get(prefix+'/artifacts',headers=ALICE).json():
                result=client.get(prefix+'/artifacts/'+entry['id']+'/download',headers=BOB)
                check(result.status_code==200,'Authorized download')
                check(hashlib.sha256(result.content).hexdigest()==entry['sha256'],'Artifact digest')
            crashed=post(prefix+'/jobs',json={'modelImportId':model_id,'kind':'docx'}).json()['id']
            first=claim_job(app.state.session_factory,worker_id='crashed-A',lease_seconds=30)
            check(first[0]==crashed,'Initial lease')
            with app.state.session_factory.begin() as db:
                db.get(Job,crashed).lease_expires_at=utcnow()-timedelta(seconds=1)
            second=claim_job(app.state.session_factory,worker_id='recovered-B',lease_seconds=30)
            check(second[0]==crashed and second[1]!=first[1],'Expired job reclaimed by distinct token')
            check(not finish_job(app.state.session_factory,crashed,first[1],error='stale'),'Stale worker refused')
            check(not renew_lease(app.state.session_factory,crashed,first[1]),'Stale heartbeat refused')
            check(renew_lease(app.state.session_factory,crashed,second[1]),'New heartbeat accepted')
            check(finish_job(app.state.session_factory,crashed,second[1],error='fault injected'),'Terminal failure preserved')
            exhausted=post(prefix+'/jobs',json={'modelImportId':model_id,'kind':'docx'}).json()['id']
            for attempt in range(2):
                claim=claim_job(app.state.session_factory,max_attempts=2)
                check(claim[0]==exhausted,'Retry claimed')
                with app.state.session_factory.begin() as db:
                    db.get(Job,exhausted).lease_expires_at=utcnow()-timedelta(seconds=1)
            following=post(prefix+'/jobs',json={'modelImportId':model_id,'kind':'docx'}).json()['id']
            claim=claim_job(app.state.session_factory,max_attempts=2)
            check(claim[0]==following,'Exhausted job cannot starve queue')
            check(finish_job(app.state.session_factory,following,claim[1],error='expected'),'Following job terminates')
            with app.state.session_factory() as db:
                check(db.get(Job,exhausted).status=='FAIL','Exhausted job fails closed')
                check(db.get(Job,crashed).attempts==2 and db.get(Job,crashed).error=='fault injected','Correct recovered failure')
                check(not db.execute(select(Artifact).where(Artifact.job_id==crashed)).first(),'Failed job has no published artifact')
        app.state.engine.dispose()
    print('PASS M7: PostgreSQL migrations, API, RBAC, 4 workers / 12 jobs, 23 artifacts, lease fencing and fault recovery')

if __name__=='__main__':
    main()
