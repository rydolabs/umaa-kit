/* UMAA Studio V6: read-only EA model explorer and draft artifact workflow.
 * React class API is used for offline smoke compatibility. Not production UI.
 */

type Project = { id: string; name: string; role: string };
type Model = { id: string; projectId: string; sourceSha256: string;
  validation: string; generationApproved: boolean; counts: Record<string,number>;
  issues: string[]; changes: any; createdAt: string };
type Job = { id: string; kind: string; status: string; error: string|null };
type Artifact = { id: string; jobId: string; filename: string; kind: string; sha256:string };
type BrowserResult = { total:number; items:Record<string,any>[] };
type State = {
 token: string; connected: boolean; projects: Project[]; project: string;
 models: Model[]; model: string; kind:string; query:string; rows: Record<string,any>[];
 total: number; jobs: Job[]; artifacts: Artifact[]; name: string;
 docTitle: string; idlModule: string; selected: string[];
 loading: boolean; message: string; error: string;
};

class StudioApp extends React.Component<{},State> {
  constructor(props: {}) {
    super(props);
    this.state = {
      token:'',connected:false,projects:[],project:'',models:[],model:'',
      kind:'elements',query:'',rows:[],total:0,jobs:[],artifacts:[],name:'',
      docTitle:'Model Inventory',idlModule:'ProjectTypes',selected:[],
      loading:false,message:'',error:''
    };
  }
  private async request(path: string, init: RequestInit={}) {
    const response = await fetch('/api/v1'+path, {
      ...init,
      headers: { Authorization:'Bearer '+this.state.token,
        ...((init.headers||{}) as Record<string,string>) }
    });
    if(!response.ok) {
      const body = await response.json().catch(()=>({detail:'HTTP '+response.status}));
      throw new Error(typeof body.detail==='string' ? body.detail : JSON.stringify(body.detail));
    }
    return response.json();
  }
  private async perform(fn:()=>Promise<void>) {
    this.setState({loading:true,error:'',message:''});
    try { await fn(); } catch(error) {
      this.setState({error: error instanceof Error?error.message:String(error)});
    } finally { this.setState({loading:false}); }
  }
  private async loadProjects() {
    const projects:Project[]=await this.request('/projects');
    this.setState({connected:true,projects});
    if(!projects.find(p=>p.id===this.state.project)) {
      await this.chooseProject(projects.length?projects[0].id:'');
    }
  }
  private async chooseProject(project:string) {
    this.setState({project,model:'',models:[],rows:[],jobs:[],artifacts:[],selected:[]});
    if(!project)return;
    const [models,jobs,artifacts] = await Promise.all([
      this.request('/projects/'+project+'/models'),
      this.request('/projects/'+project+'/jobs'),
      this.request('/projects/'+project+'/artifacts')
    ]);
    this.setState({models,jobs,artifacts,model:models.length?models[0].id:''});
    if(models.length) await this.loadRows(project,models[0].id,this.state.kind,'');
  }
  private async loadRows(project=this.state.project,model=this.state.model,
                        kind=this.state.kind,query=this.state.query) {
    if(!project||!model){this.setState({rows:[],total:0});return;}
    const result:BrowserResult=await this.request('/projects/'+project+'/models/'+model+
      '/elements?kind='+encodeURIComponent(kind)+'&q='+encodeURIComponent(query)+'&limit=200');
    this.setState({rows:result.items,total:result.total,kind,query,selected:[]});
  }
  private async createProject() {
    if(!this.state.name.trim())throw new Error('프로젝트 이름을 입력하세요.');
    const result:Project=await this.request('/projects',{method:'POST',
      headers:{'Content-Type':'application/json'},body:JSON.stringify({name:this.state.name})});
    this.setState({name:''}); await this.loadProjects(); await this.chooseProject(result.id);
  }
  private async upload(file:File|undefined) {
    if(!file||!this.state.project)return;
    const form = new FormData();form.append('file',file);
    const result:Model=await this.request('/projects/'+this.state.project+'/models',
      {method:'POST',body:form});
    await this.chooseProject(this.state.project);
    this.setState({model:result.id,message:'읽기 전용 QEA 스냅샷을 등록했습니다.'});
    await this.loadRows(this.state.project,result.id,'elements','');
  }
  private async generate(kind:'docx'|'idl') {
    if(!this.state.project||!this.state.model)return;
    const body = kind==='docx'?{modelImportId:this.state.model,kind,title:this.state.docTitle}:
      {modelImportId:this.state.model,kind,module:this.state.idlModule,
       elementGuids:this.state.selected};
    await this.request('/projects/'+this.state.project+'/jobs',{
      method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
    await this.chooseProject(this.state.project);
    this.setState({message:'생성 작업이 등록되었습니다. 별도 Worker 실행 후 새로고침으로 결과를 확인하세요.'});
  }
  private async download(a:Artifact) {
    const res=await fetch('/api/v1/projects/'+this.state.project+'/artifacts/'+a.id+'/download',
      {headers:{Authorization:'Bearer '+this.state.token}});
    if(!res.ok)throw new Error('다운로드/무결성 검사 실패 ('+res.status+')');
    const url=URL.createObjectURL(await res.blob());
    const anchor=document.createElement('a');anchor.href=url;anchor.download=a.filename;
    document.body.appendChild(anchor);anchor.click();anchor.remove();
    setTimeout(()=>URL.revokeObjectURL(url),5000);
  }
  render() {
    const s=this.state;
    const chosen=s.models.find(m=>m.id===s.model);
    const canEdit=s.projects.some(p=>p.id===s.project&&p.role!=='viewer');
    const visible = s.rows;
    return <div className="shell">
      <aside className="sidebar">
        <div className="logo"><span className="logo-mark">U</span><div><strong>UMAA Studio</strong><small>Model-driven development</small></div></div>
        <div className="sidebar-label">WORKSPACE</div>
        <div className="nav-item active">▦ &nbsp; Model Explorer</div>
        <div className="nav-item">◇ &nbsp; Generation Center</div>
        <div className="nav-item">◷ &nbsp; Execution History</div>
        <div className="sidebar-note">개발 도구 PoC · 공식 UMAA 적합성 검증 결과가 아닙니다.</div>
      </aside>
      <main className="main">
        <header className="header"><div><div className="eyebrow">DEVELOPMENT PLATFORM</div>
          <h1>EA 모델 기반 개발 작업공간</h1><p>QEA 모델을 가져와 분석하고 IDL·DOCX 초안을 생성합니다.</p></div>
          <span className="environment">LOCAL DEVELOPMENT</span></header>
        <section className="auth-panel">
          <label>API Access Token <input aria-label="API token" type="password" placeholder="서버 설정에 등록된 Token" value={s.token}
            onChange={(e:any)=>this.setState({token:e.target.value,connected:false})}/></label>
          <button disabled={s.loading||!s.token} onClick={()=>this.perform(()=>this.loadProjects())}>연결</button>
          <span className={s.connected?'connection ok':'connection'}>{s.connected?'● 연결됨':'○ 미연결'}</span>
        </section>
        {s.error&&<div role="alert" className="alert error">{s.error}</div>}
        {s.message&&<div role="status" className="alert success">{s.message}</div>}
        <div className="cards">
          <section className="panel project-panel">
            <div className="section-heading"><h2>프로젝트</h2><span>01 / WORKSPACE</span></div>
            <select aria-label="Project" value={s.project} onChange={(e:any)=>this.perform(()=>this.chooseProject(e.target.value))}>
              <option value="">프로젝트 선택</option>{s.projects.map(p=><option key={p.id} value={p.id}>{p.name} ({p.role})</option>)}
            </select>
            <div className="field-row"><input aria-label="New project name" placeholder="새 프로젝트 이름" value={s.name}
              onChange={(e:any)=>this.setState({name:e.target.value})}/>
              <button disabled={s.loading||!s.connected} onClick={()=>this.perform(()=>this.createProject())}>생성</button></div>
          </section>
          <section className="panel upload-panel">
            <div className="section-heading"><h2>QEA 가져오기</h2><span>02 / IMPORT</span></div>
            <p>EA에서 닫은 QEA 복사본을 선택하세요. 원본은 수정되지 않습니다.</p>
            <input aria-label="Upload QEA" type="file" accept=".qea" disabled={!canEdit||s.loading} onChange={(e:any)=>{
              const file=e.target.files?.[0];this.perform(()=>this.upload(file)); e.target.value='';}}/>
            <small>현재 Import 상한 32MiB · 파일 형식/참조 검사 적용</small>
          </section>
        </div>
        <section className="panel explorer">
          <div className="section-heading"><h2>모델 탐색</h2><span>03 / MODEL DATA</span></div>
          <div className="toolbar">
            <select aria-label="Model revision" value={s.model} onChange={(e:any)=>this.perform(async()=>{
              this.setState({model:e.target.value});await this.loadRows(s.project,e.target.value,s.kind,s.query);})}>
              <option value="">모델을 선택하세요</option>{s.models.map(m=><option key={m.id} value={m.id}>
                {m.createdAt.slice(0,19)} · {m.sourceSha256.slice(0,10)}</option>)}
            </select>
            <select aria-label="Model type" value={s.kind} onChange={(e:any)=>this.perform(()=>this.loadRows(s.project,s.model,e.target.value,s.query))}>
              {['elements','packages','attributes','connectors','diagrams'].map(k=><option key={k} value={k}>{k}</option>)}
            </select>
            <input aria-label="Search model" placeholder="이름 또는 GUID 검색" value={s.query}
              onChange={(e:any)=>this.setState({query:e.target.value})}/>
            <button className="secondary" disabled={!s.model||s.loading} onClick={()=>this.perform(()=>this.loadRows())}>검색</button>
          </div>
          <div className="statrow"><span>항목 <b>{s.total}</b>개</span><span> {chosen?chosen.validation:'모델 미선택'}</span>
            {chosen&&<span className="mono">SHA {chosen.sourceSha256.slice(0,16)}…</span>}</div>
          {chosen?.changes?.requiresArtifactReview&&<div className="alert warn">이전 모델 대비 변경이 발견되었습니다. 생성물 재검토가 필요합니다.</div>}
          <div className="table-scroll"><table><thead><tr><th>선택</th><th>EA GUID</th><th>이름</th><th>유형</th></tr></thead>
            <tbody>{visible.map((row,i)=><tr key={row.ea_guid||i}>
              <td><input type="checkbox" aria-label={'Select '+(row.Name||row.ea_guid||i)} disabled={s.kind!=='elements'}
                checked={s.selected.indexOf(row.ea_guid)>=0} onChange={(e:any)=>this.setState({selected:e.target.checked?
                  [...s.selected,row.ea_guid]:s.selected.filter(g=>g!==row.ea_guid)})}/></td>
              <td className="mono">{row.ea_guid||'—'}</td><td>{row.Name||'—'}</td><td>{row.Object_Type||row.Connector_Type||row.Type||'—'}</td>
            </tr>)}</tbody></table>{visible.length===0&&<p className="empty">모델을 가져오거나 조회 조건을 변경하세요.</p>}</div>
          {s.total>200&&<p className="hint">처음 200개만 표시됩니다. 검색을 이용하세요.</p>}
        </section>
        <section className="panel generation">
          <div className="section-heading"><h2>산출물 생성</h2><span>04 / GENERATION</span></div>
          <div className="cards"><div className="action-card"><h3>DOCX 모델 인벤토리</h3><p>미승인 모델 목록 초안 · Manifest 생성</p>
            <input aria-label="Document title" value={s.docTitle} onChange={(e:any)=>this.setState({docTitle:e.target.value})}/>
            <button disabled={!canEdit||!s.model||s.loading} onClick={()=>this.perform(()=>this.generate('docx'))}>DOCX 작업 등록</button>
          </div><div className="action-card"><h3>프로젝트 확장 DDS IDL</h3><p>선택한 Class/DataType만 생성 · 공식 UMAA IDL 아님</p>
            <input aria-label="IDL module" value={s.idlModule} onChange={(e:any)=>this.setState({idlModule:e.target.value})}/>
            <button disabled={!canEdit||!s.model||!s.selected.length||s.loading} onClick={()=>this.perform(()=>this.generate('idl'))}>IDL 작업 등록 ({s.selected.length})</button>
          </div></div>
        </section>
        <div className="cards bottom">
          <section className="panel"><div className="section-heading"><h2>생성 작업</h2>
            <button className="secondary" disabled={!s.project||s.loading} onClick={()=>this.perform(()=>this.chooseProject(s.project))}>새로고침</button></div>
            {s.jobs.length?s.jobs.map(j=><div className="history" key={j.id}><span>{j.kind.toUpperCase()}</span>
              <span className={'job-status '+j.status.toLowerCase()}>{j.status}</span><small title={j.error||''}>{j.error||j.id.slice(0,8)}</small></div>):<p className="empty">등록된 작업이 없습니다.</p>}
          </section>
          <section className="panel"><div className="section-heading"><h2>다운로드</h2><span>05 / ARTIFACTS</span></div>
            {s.artifacts.length?s.artifacts.map(a=><div className="history" key={a.id}><span>{a.filename}</span>
              <button className="secondary" onClick={()=>this.perform(()=>this.download(a))}>다운로드</button></div>):<p className="empty">완료된 산출물이 없습니다.</p>}
          </section>
        </div>
        <footer>UMAA Studio V6 · Development-only · 원본 모델과 독립 · 공식 적합성 미승인</footer>
      </main></div>;
  }
}
ReactDOM.render(React.createElement(StudioApp, {}), document.getElementById('root'));
