"use strict";
/* UMAA Studio V6: read-only EA model explorer and draft artifact workflow.
 * React class API is used for offline smoke compatibility. Not production UI.
 */
class StudioApp extends React.Component {
    constructor(props) {
        super(props);
        this.state = {
            token: '', connected: false, projects: [], project: '', models: [], model: '',
            kind: 'elements', query: '', rows: [], total: 0, jobs: [], artifacts: [], name: '',
            docTitle: 'Model Inventory', idlModule: 'ProjectTypes', selected: [],
            loading: false, message: '', error: ''
        };
    }
    async request(path, init = {}) {
        const response = await fetch('/api/v1' + path, {
            ...init,
            headers: { Authorization: 'Bearer ' + this.state.token,
                ...(init.headers || {}) }
        });
        if (!response.ok) {
            const body = await response.json().catch(() => ({ detail: 'HTTP ' + response.status }));
            throw new Error(typeof body.detail === 'string' ? body.detail : JSON.stringify(body.detail));
        }
        return response.json();
    }
    async perform(fn) {
        this.setState({ loading: true, error: '', message: '' });
        try {
            await fn();
        }
        catch (error) {
            this.setState({ error: error instanceof Error ? error.message : String(error) });
        }
        finally {
            this.setState({ loading: false });
        }
    }
    async loadProjects() {
        const projects = await this.request('/projects');
        this.setState({ connected: true, projects });
        if (!projects.find(p => p.id === this.state.project)) {
            await this.chooseProject(projects.length ? projects[0].id : '');
        }
    }
    async chooseProject(project) {
        this.setState({ project, model: '', models: [], rows: [], jobs: [], artifacts: [], selected: [] });
        if (!project)
            return;
        const [models, jobs, artifacts] = await Promise.all([
            this.request('/projects/' + project + '/models'),
            this.request('/projects/' + project + '/jobs'),
            this.request('/projects/' + project + '/artifacts')
        ]);
        this.setState({ models, jobs, artifacts, model: models.length ? models[0].id : '' });
        if (models.length)
            await this.loadRows(project, models[0].id, this.state.kind, '');
    }
    async loadRows(project = this.state.project, model = this.state.model, kind = this.state.kind, query = this.state.query) {
        if (!project || !model) {
            this.setState({ rows: [], total: 0 });
            return;
        }
        const result = await this.request('/projects/' + project + '/models/' + model +
            '/elements?kind=' + encodeURIComponent(kind) + '&q=' + encodeURIComponent(query) + '&limit=200');
        this.setState({ rows: result.items, total: result.total, kind, query, selected: [] });
    }
    async createProject() {
        if (!this.state.name.trim())
            throw new Error('프로젝트 이름을 입력하세요.');
        const result = await this.request('/projects', { method: 'POST',
            headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ name: this.state.name }) });
        this.setState({ name: '' });
        await this.loadProjects();
        await this.chooseProject(result.id);
    }
    async upload(file) {
        if (!file || !this.state.project)
            return;
        const form = new FormData();
        form.append('file', file);
        const result = await this.request('/projects/' + this.state.project + '/models', { method: 'POST', body: form });
        await this.chooseProject(this.state.project);
        this.setState({ model: result.id, message: '읽기 전용 QEA 스냅샷을 등록했습니다.' });
        await this.loadRows(this.state.project, result.id, 'elements', '');
    }
    async generate(kind) {
        if (!this.state.project || !this.state.model)
            return;
        const body = kind === 'docx' ? { modelImportId: this.state.model, kind, title: this.state.docTitle } :
            { modelImportId: this.state.model, kind, module: this.state.idlModule,
                elementGuids: this.state.selected };
        await this.request('/projects/' + this.state.project + '/jobs', {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body)
        });
        await this.chooseProject(this.state.project);
        this.setState({ message: '생성 작업이 등록되었습니다. 별도 Worker 실행 후 새로고침으로 결과를 확인하세요.' });
    }
    async download(a) {
        const res = await fetch('/api/v1/projects/' + this.state.project + '/artifacts/' + a.id + '/download', { headers: { Authorization: 'Bearer ' + this.state.token } });
        if (!res.ok)
            throw new Error('다운로드/무결성 검사 실패 (' + res.status + ')');
        const url = URL.createObjectURL(await res.blob());
        const anchor = document.createElement('a');
        anchor.href = url;
        anchor.download = a.filename;
        document.body.appendChild(anchor);
        anchor.click();
        anchor.remove();
        setTimeout(() => URL.revokeObjectURL(url), 5000);
    }
    render() {
        var _a;
        const s = this.state;
        const chosen = s.models.find(m => m.id === s.model);
        const canEdit = s.projects.some(p => p.id === s.project && p.role !== 'viewer');
        const visible = s.rows;
        return React.createElement("div", { className: "shell" },
            React.createElement("aside", { className: "sidebar" },
                React.createElement("div", { className: "logo" },
                    React.createElement("span", { className: "logo-mark" }, "U"),
                    React.createElement("div", null,
                        React.createElement("strong", null, "UMAA Studio"),
                        React.createElement("small", null, "Model-driven development"))),
                React.createElement("div", { className: "sidebar-label" }, "WORKSPACE"),
                React.createElement("div", { className: "nav-item active" }, "\u25A6 \u00A0 Model Explorer"),
                React.createElement("div", { className: "nav-item" }, "\u25C7 \u00A0 Generation Center"),
                React.createElement("div", { className: "nav-item" }, "\u25F7 \u00A0 Execution History"),
                React.createElement("div", { className: "sidebar-note" }, "\uAC1C\uBC1C \uB3C4\uAD6C PoC \u00B7 \uACF5\uC2DD UMAA \uC801\uD569\uC131 \uAC80\uC99D \uACB0\uACFC\uAC00 \uC544\uB2D9\uB2C8\uB2E4.")),
            React.createElement("main", { className: "main" },
                React.createElement("header", { className: "header" },
                    React.createElement("div", null,
                        React.createElement("div", { className: "eyebrow" }, "DEVELOPMENT PLATFORM"),
                        React.createElement("h1", null, "EA \uBAA8\uB378 \uAE30\uBC18 \uAC1C\uBC1C \uC791\uC5C5\uACF5\uAC04"),
                        React.createElement("p", null, "QEA \uBAA8\uB378\uC744 \uAC00\uC838\uC640 \uBD84\uC11D\uD558\uACE0 IDL\u00B7DOCX \uCD08\uC548\uC744 \uC0DD\uC131\uD569\uB2C8\uB2E4.")),
                    React.createElement("span", { className: "environment" }, "LOCAL DEVELOPMENT")),
                React.createElement("section", { className: "auth-panel" },
                    React.createElement("label", null,
                        "API Access Token ",
                        React.createElement("input", { "aria-label": "API token", type: "password", placeholder: "\uC11C\uBC84 \uC124\uC815\uC5D0 \uB4F1\uB85D\uB41C Token", value: s.token, onChange: (e) => this.setState({ token: e.target.value, connected: false }) })),
                    React.createElement("button", { disabled: s.loading || !s.token, onClick: () => this.perform(() => this.loadProjects()) }, "\uC5F0\uACB0"),
                    React.createElement("span", { className: s.connected ? 'connection ok' : 'connection' }, s.connected ? '● 연결됨' : '○ 미연결')),
                s.error && React.createElement("div", { role: "alert", className: "alert error" }, s.error),
                s.message && React.createElement("div", { role: "status", className: "alert success" }, s.message),
                React.createElement("div", { className: "cards" },
                    React.createElement("section", { className: "panel project-panel" },
                        React.createElement("div", { className: "section-heading" },
                            React.createElement("h2", null, "\uD504\uB85C\uC81D\uD2B8"),
                            React.createElement("span", null, "01 / WORKSPACE")),
                        React.createElement("select", { "aria-label": "Project", value: s.project, onChange: (e) => this.perform(() => this.chooseProject(e.target.value)) },
                            React.createElement("option", { value: "" }, "\uD504\uB85C\uC81D\uD2B8 \uC120\uD0DD"),
                            s.projects.map(p => React.createElement("option", { key: p.id, value: p.id },
                                p.name,
                                " (",
                                p.role,
                                ")"))),
                        React.createElement("div", { className: "field-row" },
                            React.createElement("input", { "aria-label": "New project name", placeholder: "\uC0C8 \uD504\uB85C\uC81D\uD2B8 \uC774\uB984", value: s.name, onChange: (e) => this.setState({ name: e.target.value }) }),
                            React.createElement("button", { disabled: s.loading || !s.connected, onClick: () => this.perform(() => this.createProject()) }, "\uC0DD\uC131"))),
                    React.createElement("section", { className: "panel upload-panel" },
                        React.createElement("div", { className: "section-heading" },
                            React.createElement("h2", null, "QEA \uAC00\uC838\uC624\uAE30"),
                            React.createElement("span", null, "02 / IMPORT")),
                        React.createElement("p", null, "EA\uC5D0\uC11C \uB2EB\uC740 QEA \uBCF5\uC0AC\uBCF8\uC744 \uC120\uD0DD\uD558\uC138\uC694. \uC6D0\uBCF8\uC740 \uC218\uC815\uB418\uC9C0 \uC54A\uC2B5\uB2C8\uB2E4."),
                        React.createElement("input", { "aria-label": "Upload QEA", type: "file", accept: ".qea", disabled: !canEdit || s.loading, onChange: (e) => {
                                var _a;
                                const file = (_a = e.target.files) === null || _a === void 0 ? void 0 : _a[0];
                                this.perform(() => this.upload(file));
                                e.target.value = '';
                            } }),
                        React.createElement("small", null, "\uD604\uC7AC Import \uC0C1\uD55C 32MiB \u00B7 \uD30C\uC77C \uD615\uC2DD/\uCC38\uC870 \uAC80\uC0AC \uC801\uC6A9"))),
                React.createElement("section", { className: "panel explorer" },
                    React.createElement("div", { className: "section-heading" },
                        React.createElement("h2", null, "\uBAA8\uB378 \uD0D0\uC0C9"),
                        React.createElement("span", null, "03 / MODEL DATA")),
                    React.createElement("div", { className: "toolbar" },
                        React.createElement("select", { "aria-label": "Model revision", value: s.model, onChange: (e) => this.perform(async () => {
                                this.setState({ model: e.target.value });
                                await this.loadRows(s.project, e.target.value, s.kind, s.query);
                            }) },
                            React.createElement("option", { value: "" }, "\uBAA8\uB378\uC744 \uC120\uD0DD\uD558\uC138\uC694"),
                            s.models.map(m => React.createElement("option", { key: m.id, value: m.id },
                                m.createdAt.slice(0, 19),
                                " \u00B7 ",
                                m.sourceSha256.slice(0, 10)))),
                        React.createElement("select", { "aria-label": "Model type", value: s.kind, onChange: (e) => this.perform(() => this.loadRows(s.project, s.model, e.target.value, s.query)) }, ['elements', 'packages', 'attributes', 'connectors', 'diagrams'].map(k => React.createElement("option", { key: k, value: k }, k))),
                        React.createElement("input", { "aria-label": "Search model", placeholder: "\uC774\uB984 \uB610\uB294 GUID \uAC80\uC0C9", value: s.query, onChange: (e) => this.setState({ query: e.target.value }) }),
                        React.createElement("button", { className: "secondary", disabled: !s.model || s.loading, onClick: () => this.perform(() => this.loadRows()) }, "\uAC80\uC0C9")),
                    React.createElement("div", { className: "statrow" },
                        React.createElement("span", null,
                            "\uD56D\uBAA9 ",
                            React.createElement("b", null, s.total),
                            "\uAC1C"),
                        React.createElement("span", null,
                            " ",
                            chosen ? chosen.validation : '모델 미선택'),
                        chosen && React.createElement("span", { className: "mono" },
                            "SHA ",
                            chosen.sourceSha256.slice(0, 16),
                            "\u2026")),
                    ((_a = chosen === null || chosen === void 0 ? void 0 : chosen.changes) === null || _a === void 0 ? void 0 : _a.requiresArtifactReview) && React.createElement("div", { className: "alert warn" }, "\uC774\uC804 \uBAA8\uB378 \uB300\uBE44 \uBCC0\uACBD\uC774 \uBC1C\uACAC\uB418\uC5C8\uC2B5\uB2C8\uB2E4. \uC0DD\uC131\uBB3C \uC7AC\uAC80\uD1A0\uAC00 \uD544\uC694\uD569\uB2C8\uB2E4."),
                    React.createElement("div", { className: "table-scroll" },
                        React.createElement("table", null,
                            React.createElement("thead", null,
                                React.createElement("tr", null,
                                    React.createElement("th", null, "\uC120\uD0DD"),
                                    React.createElement("th", null, "EA GUID"),
                                    React.createElement("th", null, "\uC774\uB984"),
                                    React.createElement("th", null, "\uC720\uD615"))),
                            React.createElement("tbody", null, visible.map((row, i) => React.createElement("tr", { key: row.ea_guid || i },
                                React.createElement("td", null,
                                    React.createElement("input", { type: "checkbox", "aria-label": 'Select ' + (row.Name || row.ea_guid || i), disabled: s.kind !== 'elements', checked: s.selected.indexOf(row.ea_guid) >= 0, onChange: (e) => this.setState({ selected: e.target.checked ?
                                                [...s.selected, row.ea_guid] : s.selected.filter(g => g !== row.ea_guid) }) })),
                                React.createElement("td", { className: "mono" }, row.ea_guid || '—'),
                                React.createElement("td", null, row.Name || '—'),
                                React.createElement("td", null, row.Object_Type || row.Connector_Type || row.Type || '—'))))),
                        visible.length === 0 && React.createElement("p", { className: "empty" }, "\uBAA8\uB378\uC744 \uAC00\uC838\uC624\uAC70\uB098 \uC870\uD68C \uC870\uAC74\uC744 \uBCC0\uACBD\uD558\uC138\uC694.")),
                    s.total > 200 && React.createElement("p", { className: "hint" }, "\uCC98\uC74C 200\uAC1C\uB9CC \uD45C\uC2DC\uB429\uB2C8\uB2E4. \uAC80\uC0C9\uC744 \uC774\uC6A9\uD558\uC138\uC694.")),
                React.createElement("section", { className: "panel generation" },
                    React.createElement("div", { className: "section-heading" },
                        React.createElement("h2", null, "\uC0B0\uCD9C\uBB3C \uC0DD\uC131"),
                        React.createElement("span", null, "04 / GENERATION")),
                    React.createElement("div", { className: "cards" },
                        React.createElement("div", { className: "action-card" },
                            React.createElement("h3", null, "DOCX \uBAA8\uB378 \uC778\uBCA4\uD1A0\uB9AC"),
                            React.createElement("p", null, "\uBBF8\uC2B9\uC778 \uBAA8\uB378 \uBAA9\uB85D \uCD08\uC548 \u00B7 Manifest \uC0DD\uC131"),
                            React.createElement("input", { "aria-label": "Document title", value: s.docTitle, onChange: (e) => this.setState({ docTitle: e.target.value }) }),
                            React.createElement("button", { disabled: !canEdit || !s.model || s.loading, onClick: () => this.perform(() => this.generate('docx')) }, "DOCX \uC791\uC5C5 \uB4F1\uB85D")),
                        React.createElement("div", { className: "action-card" },
                            React.createElement("h3", null, "\uD504\uB85C\uC81D\uD2B8 \uD655\uC7A5 DDS IDL"),
                            React.createElement("p", null, "\uC120\uD0DD\uD55C Class/DataType\uB9CC \uC0DD\uC131 \u00B7 \uACF5\uC2DD UMAA IDL \uC544\uB2D8"),
                            React.createElement("input", { "aria-label": "IDL module", value: s.idlModule, onChange: (e) => this.setState({ idlModule: e.target.value }) }),
                            React.createElement("button", { disabled: !canEdit || !s.model || !s.selected.length || s.loading, onClick: () => this.perform(() => this.generate('idl')) },
                                "IDL \uC791\uC5C5 \uB4F1\uB85D (",
                                s.selected.length,
                                ")")))),
                React.createElement("div", { className: "cards bottom" },
                    React.createElement("section", { className: "panel" },
                        React.createElement("div", { className: "section-heading" },
                            React.createElement("h2", null, "\uC0DD\uC131 \uC791\uC5C5"),
                            React.createElement("button", { className: "secondary", disabled: !s.project || s.loading, onClick: () => this.perform(() => this.chooseProject(s.project)) }, "\uC0C8\uB85C\uACE0\uCE68")),
                        s.jobs.length ? s.jobs.map(j => React.createElement("div", { className: "history", key: j.id },
                            React.createElement("span", null, j.kind.toUpperCase()),
                            React.createElement("span", { className: 'job-status ' + j.status.toLowerCase() }, j.status),
                            React.createElement("small", { title: j.error || '' }, j.error || j.id.slice(0, 8)))) : React.createElement("p", { className: "empty" }, "\uB4F1\uB85D\uB41C \uC791\uC5C5\uC774 \uC5C6\uC2B5\uB2C8\uB2E4.")),
                    React.createElement("section", { className: "panel" },
                        React.createElement("div", { className: "section-heading" },
                            React.createElement("h2", null, "\uB2E4\uC6B4\uB85C\uB4DC"),
                            React.createElement("span", null, "05 / ARTIFACTS")),
                        s.artifacts.length ? s.artifacts.map(a => React.createElement("div", { className: "history", key: a.id },
                            React.createElement("span", null, a.filename),
                            React.createElement("button", { className: "secondary", onClick: () => this.perform(() => this.download(a)) }, "\uB2E4\uC6B4\uB85C\uB4DC"))) : React.createElement("p", { className: "empty" }, "\uC644\uB8CC\uB41C \uC0B0\uCD9C\uBB3C\uC774 \uC5C6\uC2B5\uB2C8\uB2E4."))),
                React.createElement("footer", null, "UMAA Studio V6 \u00B7 Development-only \u00B7 \uC6D0\uBCF8 \uBAA8\uB378\uACFC \uB3C5\uB9BD \u00B7 \uACF5\uC2DD \uC801\uD569\uC131 \uBBF8\uC2B9\uC778")));
    }
}
ReactDOM.render(React.createElement(StudioApp, {}), document.getElementById('root'));
