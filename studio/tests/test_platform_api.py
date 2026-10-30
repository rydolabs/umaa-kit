"""Integration tests for isolated Studio SQL, API, and filesystem worker.

SQLite is the local test database. PostgreSQL itself requires a separate DB test.
"""
from contextlib import closing
import hashlib
import io
import json
from pathlib import Path
import sqlite3
import tempfile
import unittest
from fastapi.testclient import TestClient
from studio.platform_api import create_app
from studio.platform_worker import run_one

TOKENS = {
    'a' * 32: {'id': 'alice', 'name': 'Alice'},
    'b' * 32: {'id': 'bob', 'name': 'Bob'},
    'c' * 32: {'id': 'charlie', 'name': 'Charlie'},
}

def fixture(path, *, change=False, bad=False):
    with closing(sqlite3.connect(path)) as db:
        db.executescript('''
        CREATE TABLE t_package (Package_ID INTEGER, ea_guid TEXT, Name TEXT, Parent_ID INTEGER);
        CREATE TABLE t_object (Object_ID INTEGER, ea_guid TEXT, Name TEXT, Object_Type TEXT, Package_ID INTEGER);
        CREATE TABLE t_connector (Connector_ID INTEGER, ea_guid TEXT, Name TEXT, Connector_Type TEXT, Start_Object_ID INTEGER, End_Object_ID INTEGER);
        CREATE TABLE t_attribute (ID INTEGER, ea_guid TEXT, Object_ID INTEGER, Name TEXT, Type TEXT, Classifier TEXT, Pos INTEGER);
        CREATE TABLE t_diagram (Diagram_ID INTEGER, ea_guid TEXT, Name TEXT, Package_ID INTEGER);
        INSERT INTO t_package VALUES (1,'p-1','System',0);
        INSERT INTO t_object VALUES (7,'obj-7','Engine','DataType',1);
        INSERT INTO t_attribute VALUES (1,'a-1',7,'speed','double',NULL,0);
        INSERT INTO t_diagram VALUES (5,'dia-5','Overview',1);
        ''')
        if change:
            db.execute("UPDATE t_object SET Name='EngineUpdated' WHERE Object_ID=7")
        if bad:
            db.execute("INSERT INTO t_connector VALUES (4,'c-4','','Association',7,1234)")
        db.commit()
    return path

class StudioPlatformTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.app = create_app(database_url='sqlite:///' + str(self.root/'platform.sqlite'),
                              storage_dir=self.root/'storage', identities=TOKENS, init_schema=True)
        self.client = TestClient(self.app)
        self.addCleanup(self.client.close)
        self.alice, self.bob, self.charlie = ({'Authorization':'Bearer '+c*32} for c in 'abc')
        project = self.client.post('/api/v1/projects', json={'name':'Demo'},headers=self.alice)
        self.assertEqual(project.status_code,201,project.text)
        self.project = project.json()['id']
        self.prefix = f'/api/v1/projects/{self.project}'

    def upload(self,path,headers=None):
        return self.client.post(self.prefix+'/models', headers=headers or self.alice,
                   files={'file':(path.name,path.read_bytes(),'application/octet-stream')})

    def test_auth_and_no_cross_project_leak(self):
        self.assertEqual(self.client.get('/api/v1/projects').status_code,401)
        self.assertEqual(self.client.get('/api/v1/projects',headers={'Authorization':'Bearer wrong'}).status_code,401)
        self.assertEqual(self.client.get(self.prefix+'/models',headers=self.bob).status_code,404)
        self.assertEqual(self.client.get('/api/v1/projects',headers=self.bob).json(),[])
        self.assertEqual(self.client.post(self.prefix+'/members',headers=self.bob,json={
            'userId':'bob','role':'editor'}).status_code,404)
        self.assertEqual(self.client.post(self.prefix+'/members',headers=self.alice,json={
            'userId':'nobody','role':'editor'}).status_code,422)

    def test_membership_roles_import_and_diff(self):
        r=self.client.post(self.prefix+'/members',headers=self.alice,json={'userId':'bob','role':'viewer'})
        self.assertEqual(r.status_code,201,r.text)
        self.assertEqual(self.upload(fixture(self.root/'initial.qea'),self.bob).status_code,404)
        self.assertEqual(self.client.get(self.prefix+'/models',headers=self.bob).json(),[])
        a=self.upload(self.root/'initial.qea').json()
        self.assertEqual(a['counts']['elements'],1)
        self.assertFalse(a['generationApproved'])
        self.assertEqual(self.upload(self.root/'initial.qea').json()['deduplicated'],True)
        b=self.upload(fixture(self.root/'changed.qea',change=True)).json()
        self.assertEqual(b['changes']['changes']['elements']['changed'],['obj-7'])
        self.assertTrue(b['changes']['requiresArtifactReview'])
        items=self.client.get(self.prefix+f'/models/{b["id"]}/elements',headers=self.bob,params={'q':'Updated'}).json()
        self.assertEqual(items['total'],1)
        self.assertEqual(items['items'][0]['Name'],'EngineUpdated')
        self.assertEqual(self.client.get(self.prefix+f'/models/{b["id"]}/elements',headers=self.charlie).status_code,404)
        events=self.client.get(self.prefix+'/audit',headers=self.alice).json()
        self.assertIn('model.import',[e['event'] for e in events])
        self.assertEqual(self.client.get(self.prefix+'/audit',headers=self.bob).status_code,404)

    def test_default_docx_job_and_integrity(self):
        model=self.upload(fixture(self.root/'sample.qea')).json()
        url=self.prefix+'/jobs'
        self.assertEqual(self.client.post(url,json={'modelImportId':model['id'],'kind':'docx'},
                                          headers=self.bob).status_code,404)
        new=self.client.post(url,headers=self.alice,json={
            'modelImportId':model['id'],'kind':'docx','title':'Engine Inventory'})
        self.assertEqual(new.status_code,202,new.text)
        job_id=new.json()['id']
        self.assertEqual(new.json()['status'],'QUEUED')
        result=run_one(self.app.state.session_factory,self.app.state.store)
        self.assertEqual(result,job_id)
        self.assertEqual(self.client.get(url+'/'+job_id,headers=self.alice).json()['status'],'PASS')
        artifacts=self.client.get(self.prefix+'/artifacts',headers=self.alice).json()
        self.assertEqual(set(a['kind'] for a in artifacts),{'docx','manifest'})
        for a in artifacts:
            response=self.client.get(self.prefix+'/artifacts/'+a['id']+'/download',headers=self.alice)
            self.assertEqual(response.status_code,200,response.text[:100] if response.status_code!=200 else '')
            self.assertEqual(hashlib.sha256(response.content).hexdigest(),a['sha256'])
            self.assertEqual(self.client.get(self.prefix+'/artifacts/'+a['id']+'/download',headers=self.bob).status_code,404)
        a=artifacts[0]; dest=self.app.state.store/a['sha256'][:2]/a['sha256']
        dest.write_bytes(b'corrupted')
        self.assertEqual(self.client.get(self.prefix+'/artifacts/'+a['id']+'/download',headers=self.alice).status_code,503)

    def test_corrupted_preexisting_artifact_rejected_at_generation(self):
        model=self.upload(fixture(self.root/'sample.qea')).json()
        first=self.client.post(self.prefix+'/jobs',headers=self.alice,json={
            'modelImportId':model['id'],'kind':'idl','module':'ProjectTypes',
            'elementGuids':['obj-7']}).json()['id']
        run_one(self.app.state.session_factory,self.app.state.store)
        artifact=self.client.get(self.prefix+'/artifacts',headers=self.alice).json()[0]
        dest=self.app.state.store/artifact['sha256'][:2]/artifact['sha256']
        dest.write_bytes(b'corrupted')
        second=self.client.post(self.prefix+'/jobs',headers=self.alice,json={
            'modelImportId':model['id'],'kind':'idl','module':'ProjectTypes',
            'elementGuids':['obj-7']}).json()['id']
        run_one(self.app.state.session_factory,self.app.state.store)
        self.assertEqual(self.client.get(self.prefix+'/jobs/'+second,headers=self.alice).json()['status'],'FAIL')
        self.assertEqual(len(self.client.get(self.prefix+'/artifacts',headers=self.alice).json()),1)

    def test_idl_job_nonconformance_and_zip(self):
        model=self.upload(fixture(self.root/'sample.qea')).json()
        response=self.client.post(self.prefix+'/jobs',headers=self.alice,json={
            'modelImportId':model['id'], 'kind':'idl','module':'ProjectTypes',
            'elementGuids':['obj-7']})
        self.assertEqual(response.status_code,202,response.text)
        run_one(self.app.state.session_factory,self.app.state.store)
        self.assertEqual(self.client.get(self.prefix+'/jobs/'+response.json()['id'],headers=self.alice).json()['status'],'PASS')
        artifact=self.client.get(self.prefix+'/artifacts',headers=self.alice).json()[0]
        import zipfile
        data=self.client.get(self.prefix+'/artifacts/'+artifact['id']+'/download',headers=self.alice).content
        with zipfile.ZipFile(io.BytesIO(data)) as zipped:
            self.assertIn('ProjectTypes.idl',zipped.namelist())
            self.assertIn(b'NOT official UMAA IDL',zipped.read('ProjectTypes.idl'))
            self.assertFalse(json.loads(zipped.read('manifest.json'))['conformanceClaim'])

    def test_failed_job_recorded_and_not_downloadable(self):
        model=self.upload(fixture(self.root/'sample.qea')).json()
        response=self.client.post(self.prefix+'/jobs',headers=self.alice,json={
            'modelImportId':model['id'], 'kind':'idl','module':'UMAA6_illegal',
            'elementGuids':['obj-7']})
        self.assertEqual(response.status_code,202)
        run_one(self.app.state.session_factory,self.app.state.store)
        verdict=self.client.get(self.prefix+'/jobs/'+response.json()['id'],headers=self.alice).json()
        self.assertEqual(verdict['status'],'FAIL')
        self.assertEqual(self.client.get(self.prefix+'/artifacts',headers=self.alice).json(),[])

    def test_editor_can_generate_and_other_project_cannot_access(self):
        self.client.post(self.prefix+'/members',headers=self.alice,json={'userId':'bob','role':'editor'})
        second=self.client.post('/api/v1/projects',headers=self.bob,json={'name':'Bob Project'})
        self.assertEqual(second.status_code,201)
        second_id=second.json()['id']
        imp=self.upload(fixture(self.root/'shared.qea'),self.bob)
        self.assertEqual(imp.status_code,201)
        model_id=imp.json()['id']
        queued=self.client.post(self.prefix+'/jobs',headers=self.bob,json={
            'modelImportId':model_id,'kind':'docx'})
        self.assertEqual(queued.status_code,202)
        self.assertEqual(self.client.post('/api/v1/projects/'+second_id+'/jobs',headers=self.bob,
            json={'modelImportId':model_id,'kind':'docx'}).status_code,404)
        run_one(self.app.state.session_factory,self.app.state.store)
        artifact=self.client.get(self.prefix+'/artifacts',headers=self.bob).json()[0]
        self.assertEqual(self.client.get('/api/v1/projects/'+second_id+'/artifacts/'+artifact['id']+'/download',
                                         headers=self.bob).status_code,404)
        self.assertEqual(self.client.get(self.prefix+'/artifacts/'+artifact['id']+'/download',
                                         headers=self.bob).status_code,200)
        self.assertEqual(self.client.get(self.prefix+'/audit',headers=self.bob).status_code,404)

    def test_service_contract_catalog_has_project_rbac_and_partial_scope(self):
        route = self.prefix + '/service-contracts'
        self.assertEqual(self.client.get(route).status_code, 401)
        self.assertEqual(self.client.get(route, headers=self.bob).status_code, 404)
        self.assertEqual(self.client.post(self.prefix+'/members', headers=self.alice,
                          json={'userId':'bob','role':'viewer'}).status_code, 201)
        result = self.client.get(route, headers=self.bob)
        self.assertEqual(result.status_code, 200, result.text)
        snapshot = result.json()
        self.assertEqual(snapshot['kind'], 'umaa-reviewed-service-contract-snapshot')
        self.assertEqual(len(snapshot['services']), 53)
        self.assertEqual(sum(len(x['topics']) for x in snapshot['services']), 113)
        self.assertEqual(snapshot['source']['scope'],
                         'partial-reviewed-mapping-not-normative-runtime')
        self.assertEqual([x['id'] for x in snapshot['services']
                          if x['source']['reference'] is None],
                         ['UMAA::EO::BatteryStatus', 'UMAA::EO::EngineControl'])
        self.assertTrue(all(not x['evidence']['ddsQosVerified']
                            for x in snapshot['services']))
        # Studio does not persist a writable alternate official service catalog.
        snapshot['services'].clear()
        self.assertEqual(len(self.client.get(route, headers=self.alice).json()['services']), 53)
        self.assertEqual(self.client.get(route, headers=self.charlie).status_code, 404)

    def test_component_assembly_validation_uses_cli_gate_without_writes(self):
        self.client.post(self.prefix+'/members', headers=self.alice,
                         json={'userId':'bob','role':'viewer'})
        route = self.prefix + '/assemblies/validate'
        demo = {'schemaVersion':'1',
                'component':{'id':'demo_engine','artifact':'shared-library'},
                'services':[{'id':'Demo::EO::EngineControl','role':'provider'}]}
        self.assertEqual(self.client.post(route, json=demo).status_code, 401)
        self.assertEqual(self.client.post(route, headers=self.charlie, json=demo).status_code, 404)
        result = self.client.post(route, headers=self.bob, json=demo)
        self.assertEqual(result.status_code, 200, result.text)
        self.assertTrue(result.json()['codegenReady'])
        self.assertFalse(result.json()['officialRuntimeClaim'])
        self.assertEqual(self.client.get(self.prefix+'/jobs', headers=self.alice).json(), [])

        catalog = self.client.get(self.prefix+'/service-contracts',
                                  headers=self.alice).json()
        pin = catalog['source']['catalogSha256']
        official = {'schemaVersion':'1',
                    'component':{'id':'autopilot','artifact':'shared-library'},
                    'catalogPin':{'kind':'reviewed-icd-catalog','sha256':pin},
                    'services':[
                        {'id':'UMAA::SA::GlobalPoseStatus','role':'provider'},
                        {'id':'UMAA::SA::GlobalPoseStatus','role':'consumer'},
                    ]}
        result = self.client.post(route, headers=self.alice, json=official)
        self.assertEqual(result.status_code, 200, result.text)
        self.assertTrue(result.json()['catalogPinned'])
        self.assertFalse(result.json()['codegenReady'])
        self.assertFalse(result.json()['normativeQosClaim'])
        mutated = json.loads(json.dumps(official))
        mutated['catalogPin']['sha256'] = '0'*64
        self.assertEqual(self.client.post(route, headers=self.alice,
                                         json=mutated).status_code, 422)
        official.pop('catalogPin')
        self.assertEqual(self.client.post(route, headers=self.alice,
                                         json=official).status_code, 422)
        demo['services'][0]['id'] = 'UMAA::SA::ForgedService'
        self.assertEqual(self.client.post(route, headers=self.alice,
                                         json=demo).status_code, 422)
        oversized = {'payload':'x' * (128 * 1024)}
        self.assertEqual(self.client.post(route, headers=self.alice,
                                         json=oversized).status_code, 413)
        self.assertEqual(self.client.get(self.prefix+'/jobs', headers=self.alice).json(), [])

    def test_ui_served_on_same_origin(self):
        home=self.client.get('/studio/')
        self.assertEqual(home.status_code,200)
        self.assertIn('UMAA Studio',home.text)
        self.assertEqual(self.client.get('/studio/app.js').status_code,200)
        self.assertEqual(self.client.get('/studio/vendor/react.production.min.js').status_code,200)
        self.assertNotIn("script-src 'unsafe-inline'",home.text)

    def test_bad_model_rejection_and_input_limits(self):
        bad=fixture(self.root/'bad.qea',bad=True)
        result=self.upload(bad)
        self.assertEqual(result.status_code,201)
        self.assertEqual(result.json()['validation'],'WARNINGS')
        self.assertEqual(self.client.post(self.prefix+'/jobs',headers=self.alice,json={
            'modelImportId':result.json()['id'],'kind':'docx'}).status_code,422)
        other=self.root/'wrong.qea'; other.write_bytes(b'not a sqlite database')
        self.assertEqual(self.upload(other).status_code,422)
        self.assertEqual(self.client.get(self.prefix+'/models/other/elements',headers=self.alice).status_code,404)

if __name__=='__main__':unittest.main()
