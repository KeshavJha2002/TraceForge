import {
  useEffect,
  useId,
  useRef,
  useState,
  type ChangeEvent,
  type FormEvent,
} from 'react';
import Editor from '@monaco-editor/react';
import {
  Group,
  Panel,
  Separator,
} from 'react-resizable-panels';
import type { PanelImperativeHandle } from 'react-resizable-panels';
import './App.css';

type EntryMode = 'standalone' | 'targeted';
type RouteView = 'home' | 'tool';

type NamedValue = {
  name: string;
  dataType: string;
  value: string;
};

type EditorTab = {
  id: string;
  title: string;
  code: string;
};

type OutputTab = {
  id: string;
  title: string;
  body: string;
};

type JobResponse = {
  jobId: string;
  status: string;
  createdAt: string;
  updatedAt: string;
  progress: {
    stage: string;
    message: string;
  };
  request: {
    code: string;
    entryMode: EntryMode;
    sourceName: string;
    targetFunction: string;
    parameters: NamedValue[];
    globals: NamedValue[];
  };
  result: {
    graph: unknown;
    flatTrace: unknown;
    preview: {
      title: string;
      body: string;
    };
  };
};

const sampleCode = `int total = 4;

int solve(int x, int y) {
  return total + x * y;
}
`;

const sampleInjection = `{
  "locals": {
    "x": 3,
    "y": 7
  },
  "globals": {
    "total": 4
  }
}`;

function App() {
  const [routeView, setRouteView] = useState<RouteView>(() => readRoute());
  const [editorTabs, setEditorTabs] = useState<EditorTab[]>([
    { id: createClientId(), title: 'Snippet 1', code: sampleCode },
  ]);
  const [activeEditorTabId, setActiveEditorTabId] = useState(editorTabs[0].id);
  const [outputTabs, setOutputTabs] = useState<OutputTab[]>([
    {
      id: 'trace',
      title: 'Trace Graph',
      body: 'The graph output window will render here once the trace job starts returning artifacts.',
    },
    {
      id: 'status',
      title: 'Status',
      body: 'Waiting for a trace request.',
    },
    {
      id: 'request',
      title: 'Request',
      body: 'The latest accepted request payload will appear here.',
    },
  ]);
  const [activeOutputTabId, setActiveOutputTabId] = useState('trace');
  const [targetFunction, setTargetFunction] = useState('solve');
  const [injectionJson, setInjectionJson] = useState(sampleInjection);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [job, setJob] = useState<JobResponse | null>(null);
  const [error, setError] = useState('');
  const [isInjectionPanelOpen, setIsInjectionPanelOpen] = useState(false);
  const [isLeftCollapsed, setIsLeftCollapsed] = useState(false);
  const [isRightCollapsed, setIsRightCollapsed] = useState(false);
  const leftPanelRef = useRef<PanelImperativeHandle>(null);
  const rightPanelRef = useRef<PanelImperativeHandle>(null);
  const fileInputId = useId();
  const isGenerating = isSubmitting || job?.status === 'generating';
  const activeEditorTab = editorTabs.find((tab) => tab.id === activeEditorTabId) ?? editorTabs[0];
  const activeOutputTab = outputTabs.find((tab) => tab.id === activeOutputTabId) ?? outputTabs[0];

  useEffect(() => {
    function syncRoute() {
      setRouteView(readRoute());
    }

    window.addEventListener('hashchange', syncRoute);
    return () => {
      window.removeEventListener('hashchange', syncRoute);
    };
  }, []);

  useEffect(() => {
    if (!job) {
      return;
    }

    setOutputTabs((current) =>
      current.map((tab) => {
        if (tab.id === 'status') {
          return {
            ...tab,
            body: `${job.progress.message}\n\nJob ID: ${job.jobId}\nStage: ${job.progress.stage}`,
          };
        }

        if (tab.id === 'request') {
          return {
            ...tab,
            body: JSON.stringify(job.request, null, 2),
          };
        }

        if (tab.id === 'trace' && !isGenerating) {
          return {
            ...tab,
            body: `${job.result.preview.title}\n\n${job.result.preview.body}`,
          };
        }

        return tab;
      }),
    );
  }, [job, isGenerating]);

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setError('');
    setIsSubmitting(true);
    setJob(null);
    setActiveOutputTabId('status');
    setOutputTabs((current) =>
      current.map((tab) =>
        tab.id === 'status'
          ? { ...tab, body: 'Generating the trace graph - please wait.' }
          : tab,
      ),
    );

    try {
      const injection = parseInjectionJson(injectionJson);
      const response = await fetch(`${resolveApiBaseUrl()}/api/jobs`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          sourceName: `${activeEditorTab.title || 'submission'}.cpp`,
          code: activeEditorTab.code,
          entryMode: targetFunction.trim() ? 'targeted' : 'standalone',
          targetFunction,
          parameters: mapNamedValues(injection.locals),
          globals: mapNamedValues(injection.globals),
        }),
      });

      const payload = (await response.json()) as JobResponse | { message?: string };
      if (!response.ok) {
        throw new Error('message' in payload && payload.message ? payload.message : 'Request failed.');
      }

      setJob(payload as JobResponse);
    } catch (submissionError) {
      const message = submissionError instanceof Error ? submissionError.message : 'Request failed.';
      setError(message);
      setOutputTabs((current) =>
        current.map((tab) => (tab.id === 'status' ? { ...tab, body: message } : tab)),
      );
    } finally {
      setIsSubmitting(false);
    }
  }

  function handleFileChange(event: ChangeEvent<HTMLInputElement>) {
    const file = event.target.files?.[0];
    if (!file) {
      return;
    }

    file.text().then((text) => {
      setEditorTabs((current) =>
        current.map((tab) =>
          tab.id === activeEditorTabId
            ? {
                ...tab,
                title: stripExtension(file.name) || tab.title,
                code: text,
              }
            : tab,
        ),
      );
    });
  }

  function updateActiveTabCode(nextCode: string) {
    setEditorTabs((current) =>
      current.map((tab) => (tab.id === activeEditorTabId ? { ...tab, code: nextCode } : tab)),
    );
  }

  function updateActiveTabTitle(nextTitle: string) {
    setEditorTabs((current) =>
      current.map((tab) => (tab.id === activeEditorTabId ? { ...tab, title: nextTitle } : tab)),
    );
  }

  function addEditorTab() {
    const nextTab = {
      id: createClientId(),
      title: `Snippet ${editorTabs.length + 1}`,
      code: '',
    };
    setEditorTabs((current) => [...current, nextTab]);
    setActiveEditorTabId(nextTab.id);
  }

  function closeEditorTab(tabId: string) {
    if (editorTabs.length === 1) {
      return;
    }

    const nextTabs = editorTabs.filter((tab) => tab.id !== tabId);
    setEditorTabs(nextTabs);
    if (activeEditorTabId === tabId) {
      setActiveEditorTabId(nextTabs[0].id);
    }
  }

  function toggleLeftPanel() {
    const panel = leftPanelRef.current;
    if (!panel) {
      return;
    }

    if (panel.isCollapsed()) {
      panel.expand();
      return;
    }

    panel.collapse();
  }

  function toggleRightPanel() {
    const panel = rightPanelRef.current;
    if (!panel) {
      return;
    }

    if (panel.isCollapsed()) {
      panel.expand();
      return;
    }

    panel.collapse();
  }

  if (routeView === 'home') {
    return (
      <main className="landing-shell">
        <section className="landing-panel">
          <p className="eyebrow">TraceForge · Step 1</p>
          <h1>Turn raw C++ into a staged trace request.</h1>
          <p className="hero-copy">
            Submit a full program or target a function directly, inject initial values, and prepare
            the replay workspace that will later render the graph.
          </p>
          <div className="landing-actions">
            <button className="submit-button landing-button" onClick={() => navigateTo('tool')} type="button">
              Try it out
            </button>
            <p className="landing-note">The tool opens in a dedicated full-height workspace.</p>
          </div>
        </section>
      </main>
    );
  }

  return (
    <main className="tool-shell">
      <form className="ide-shell" onSubmit={handleSubmit}>
        <Group className="ide-panels" id="traceforge-tool-layout" orientation="horizontal">
          <Panel
            className="ide-panel"
            collapsible
            collapsedSize="15px"
            defaultSize={58}
            minSize={30}
            onResize={(panelSize) => {
              setIsLeftCollapsed(panelSize.inPixels <= 18);
            }}
            order={1}
            panelRef={leftPanelRef}
          >
            {isLeftCollapsed ? (
              <button className="collapsed-ribbon" onClick={toggleLeftPanel} type="button">
                Code
              </button>
            ) : (
            <section className="editor-surface">
              <div className="workspace-toolbar">
                <div className="toolbar-group">
                  <button className="run-button" disabled={isSubmitting} type="submit">
                    {isSubmitting ? 'Generating...' : 'Generate Trace'}
                  </button>
                  <input
                    className="tab-title-input"
                    onChange={(event) => updateActiveTabTitle(event.target.value)}
                    value={activeEditorTab.title}
                  />
                </div>

                <div className="toolbar-group">
                  <label className="toolbar-button toolbar-file" htmlFor={fileInputId}>
                    Upload File
                  </label>
                  <input
                    accept=".cpp,.cc,.cxx,.hpp,.h"
                    className="hidden-file-input"
                    id={fileInputId}
                    onChange={handleFileChange}
                    type="file"
                  />
                  <button className="toolbar-icon-button" onClick={toggleLeftPanel} type="button">
                    Collapse
                  </button>
                </div>
              </div>

              <div className="editor-tabbar">
                <div className="editor-tabstrip">
                  {editorTabs.map((tab) => (
                    <button
                      key={tab.id}
                      className={`editor-tab ${tab.id === activeEditorTabId ? 'editor-tab-active' : ''}`}
                      onClick={() => setActiveEditorTabId(tab.id)}
                      type="button"
                    >
                      <span>{tab.title}</span>
                      {editorTabs.length > 1 ? (
                        <span
                          className="editor-tab-close"
                          onClick={(event) => {
                            event.stopPropagation();
                            closeEditorTab(tab.id);
                          }}
                        >
                          x
                        </span>
                      ) : null}
                    </button>
                  ))}
                  <button className="editor-tab-add" onClick={addEditorTab} type="button">
                    +
                  </button>
                </div>
              </div>

              {isInjectionPanelOpen ? (
                <Group className="editor-vertical-group" orientation="vertical">
                  <Panel defaultSize={76} minSize={35}>
                    <div className="editor-main-panel">
                      <div className="editor-content">
                        <Editor
                          defaultLanguage="cpp"
                          language="cpp"
                          onChange={(value) => updateActiveTabCode(value ?? '')}
                          options={{
                            automaticLayout: true,
                            fontFamily: 'IBM Plex Mono, SFMono-Regular, Consolas, monospace',
                            fontSize: 15,
                            lineNumbersMinChars: 3,
                            minimap: { enabled: false },
                            padding: { top: 14, bottom: 14 },
                            roundedSelection: false,
                            scrollBeyondLastLine: false,
                            wordWrap: 'off',
                          }}
                          theme="vs"
                          value={activeEditorTab.code}
                        />
                      </div>

                      <div className="bottom-toolbar">
                        <div className="toolbar-group">
                          <button
                            className="toolbar-button"
                            onClick={() => setIsInjectionPanelOpen(false)}
                            type="button"
                          >
                            Hide injection params
                          </button>
                        </div>
                        <div className="toolbar-group">
                          <span className="editor-meta">{activeEditorTab.code.length} chars</span>
                        </div>
                      </div>
                    </div>
                  </Panel>

                  <Separator className="drawer-resize-handle">
                    <div className="drawer-resize-grip" />
                  </Separator>

                  <Panel defaultSize={24} minSize={14}>
                    <section className="injection-drawer">
                      <div className="drawer-header">
                        <div>
                          <p className="section-label">Injection setup</p>
                          <h3>Target function and values</h3>
                        </div>
                        <button
                          className="toolbar-icon-button"
                          onClick={() => setIsInjectionPanelOpen(false)}
                          type="button"
                        >
                          Close
                        </button>
                      </div>
                      <div className="drawer-grid">
                        <label className="field">
                          <span>Target function</span>
                          <input
                            onChange={(event) => setTargetFunction(event.target.value)}
                            placeholder="solve"
                            value={targetFunction}
                          />
                        </label>
                        <label className="field drawer-json">
                          <span>Injection JSON</span>
                          <textarea
                            className="json-editor"
                            onChange={(event) => setInjectionJson(event.target.value)}
                            spellCheck={false}
                            value={injectionJson}
                          />
                        </label>
                      </div>
                    </section>
                  </Panel>
                </Group>
              ) : (
                <>
                  <div className="editor-content">
                    <Editor
                      defaultLanguage="cpp"
                      language="cpp"
                      onChange={(value) => updateActiveTabCode(value ?? '')}
                      options={{
                        automaticLayout: true,
                        fontFamily: 'IBM Plex Mono, SFMono-Regular, Consolas, monospace',
                        fontSize: 15,
                        lineNumbersMinChars: 3,
                        minimap: { enabled: false },
                        padding: { top: 14, bottom: 14 },
                        roundedSelection: false,
                        scrollBeyondLastLine: false,
                        wordWrap: 'off',
                      }}
                      theme="vs"
                      value={activeEditorTab.code}
                    />
                  </div>

                  <div className="bottom-toolbar">
                    <div className="toolbar-group">
                      <button
                        className="toolbar-button"
                        onClick={() => setIsInjectionPanelOpen(true)}
                        type="button"
                      >
                        Add injection params
                      </button>
                    </div>
                    <div className="toolbar-group">
                      <span className="editor-meta">{activeEditorTab.code.length} chars</span>
                    </div>
                  </div>
                </>
              )}
            </section>
            )}
          </Panel>

          <Separator className="resize-handle">
            <div className="resize-handle-grip" />
          </Separator>

          <Panel
            className="ide-panel"
            collapsible
            collapsedSize={5}
            defaultSize={42}
            minSize={22}
            onResize={(panelSize) => {
              setIsRightCollapsed(panelSize.asPercentage <= 5.5);
            }}
            order={2}
            panelRef={rightPanelRef}
          >
            {isRightCollapsed ? (
              <button className="collapsed-ribbon collapsed-ribbon-output" onClick={toggleRightPanel} type="button">
                Output
              </button>
            ) : (
            <aside className="output-surface">
              <div className="output-toolbar">
                <div className="output-tabstrip">
                  {outputTabs.map((tab) => (
                    <button
                      key={tab.id}
                      className={`output-tab ${tab.id === activeOutputTabId ? 'output-tab-active' : ''}`}
                      onClick={() => setActiveOutputTabId(tab.id)}
                      type="button"
                    >
                      {tab.title}
                    </button>
                  ))}
                </div>
                <button className="toolbar-icon-button" onClick={toggleRightPanel} type="button">
                  Collapse
                </button>
              </div>

              <div className="output-header">
                <strong>{activeOutputTab.title}</strong>
                <span
                  className={`status-pill ${isGenerating ? 'status-pending' : job ? 'status-live' : 'status-idle'}`}
                >
                  {isGenerating ? 'Generating' : job ? job.status : 'Waiting'}
                </span>
              </div>

              <div className="output-body">
                {error ? <div className="error-banner">{error}</div> : null}
                <pre>{resolveOutputBody(activeOutputTab, isGenerating)}</pre>
              </div>
            </aside>
            )}
          </Panel>
        </Group>
      </form>
    </main>
  );
}

function resolveOutputBody(tab: OutputTab, isGenerating: boolean) {
  if (tab.id === 'trace' && isGenerating) {
    return 'Generating the trace graph - please wait.';
  }

  return tab.body;
}

function readRoute(): RouteView {
  return window.location.hash === '#/tool' ? 'tool' : 'home';
}

function navigateTo(next: RouteView) {
  window.location.hash = next === 'tool' ? '/tool' : '/';
}

function parseInjectionJson(value: string) {
  let parsed: unknown;

  try {
    parsed = JSON.parse(value);
  } catch {
    throw new Error('Injection JSON is invalid.');
  }

  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    throw new Error('Injection JSON must be an object with `locals` and `globals`.');
  }

  const locals = readRecord(parsed, 'locals');
  const globals = readRecord(parsed, 'globals');

  return { locals, globals };
}

function readRecord(value: object, key: 'locals' | 'globals') {
  const candidate = Reflect.get(value, key);

  if (candidate === undefined) {
    return {};
  }

  if (!candidate || typeof candidate !== 'object' || Array.isArray(candidate)) {
    throw new Error(`Injection field \`${key}\` must be a JSON object.`);
  }

  return candidate as Record<string, unknown>;
}

function mapNamedValues(record: Record<string, unknown>): NamedValue[] {
  return Object.entries(record).map(([name, value]) => ({
    name,
    dataType: '',
    value: stringifyValue(value),
  }));
}

function stringifyValue(value: unknown) {
  if (typeof value === 'string') {
    return value;
  }

  return JSON.stringify(value);
}

function resolveApiBaseUrl() {
  return import.meta.env.VITE_API_BASE_URL || 'http://localhost:3001';
}

function createClientId() {
  return Math.random().toString(36).slice(2, 10);
}

function stripExtension(value: string) {
  return value.replace(/\.[^.]+$/, '');
}

export default App;
