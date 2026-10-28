"""Optional Chromium UI smoke with in-memory HTTP bridge to FastAPI TestClient.

Browser navigation to localhost is forbidden in some sandboxes. This harness
still exercises real React UI events and a real SQLite-backed FastAPI app;
Playwright's multipart interception lacks file bytes in this environment, so
QEA input is injected at the bridge only. Real multipart is separately tested
by studio/tests/test_platform_api.py.
"""
import argparse
from pathlib import Path
import tempfile
from urllib.parse import urlsplit
from fastapi.testclient import TestClient
from studio.platform_api import create_app
from studio.platform_worker import run_one
from studio.tests.test_platform_api import fixture, TOKENS


def main(argv=None):
    p=argparse.ArgumentParser()
    p.add_argument('--screenshot', type=Path)
    args=p.parse_args(argv)
    from playwright.sync_api import sync_playwright
    root=Path(__file__).resolve().parents[1]/'frontend'/'public'
    with tempfile.TemporaryDirectory() as td:
        model=fixture(Path(td)/'demo.qea')
        app=create_app(database_url='sqlite:///'+td+'/studio.db',storage_dir=td+'/artifacts',
                       identities=TOKENS,init_schema=True)
        with TestClient(app) as client, sync_playwright() as pw:
            browser=pw.chromium.launch(executable_path='/usr/bin/chromium',
                       headless=True,args=['--no-sandbox','--disable-dev-shm-usage'])
            try:
                page=browser.new_page(viewport={'width':1360,'height':960})
                errors=[]
                page.on('pageerror',lambda exc:errors.append(str(exc)))
                def bridge(route):
                    req=route.request
                    u=urlsplit(req.url)
                    headers={k:v for k,v in req.headers.items() if k.lower() not in
                             ('host','content-length','origin','referer')}
                    if req.method=='POST' and u.path.endswith('/models'):
                        headers.pop('content-type',None)
                        r=client.post(u.path,headers=headers,files={
                            'file':('demo.qea',model.read_bytes(),'application/octet-stream')})
                    else:
                        r=client.request(req.method,u.path+('?' + u.query if u.query else ''),
                                         headers=headers,content=req.post_data_buffer)
                    route.fulfill(status=r.status_code,body=r.content,
                                  headers={'content-type':r.headers.get('content-type','application/json')})
                page.route('**/api/v1/**',bridge)
                page.set_content('<html><head><base href="http://localhost/"/></head>'
                                 '<body><div id="root"></div></body></html>')
                page.add_style_tag(path=str(root/'styles.css'))
                for file in ('vendor/react.production.min.js',
                             'vendor/react-dom.production.min.js','app.js'):
                    page.add_script_tag(path=str(root/file))
                assert page.locator('h1').count()==1
                page.get_by_label('API token').fill('a'*32)
                page.get_by_text('연결',exact=True).click()
                page.get_by_label('New project name').fill('Studio Test Project')
                page.get_by_text('생성',exact=True).click()
                page.get_by_label('Upload QEA').set_input_files(str(model))
                page.get_by_text('Engine',exact=True).wait_for(timeout=8000)
                page.get_by_text('DOCX 작업 등록',exact=True).click()
                page.get_by_text('QUEUED',exact=True).wait_for(timeout=8000)
                assert run_one(app.state.session_factory,app.state.store)
                page.get_by_text('새로고침',exact=True).click()
                page.get_by_text('PASS',exact=True).wait_for(timeout=8000)
                assert page.get_by_text('inventory.docx').count()==1
                assert not errors,errors
                if args.screenshot:
                    args.screenshot.parent.mkdir(parents=True,exist_ok=True)
                    page.screenshot(path=str(args.screenshot),full_page=True)
                print('CHROMIUM UI SMOKE PASS: login, project, import, explorer, DOCX job, PASS, artifact listing')
            finally:
                browser.close()
    return 0
if __name__=='__main__':
    raise SystemExit(main())
