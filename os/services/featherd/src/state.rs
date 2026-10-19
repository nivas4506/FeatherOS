use aegis_exec::{ExecutionConfig, SafeExecutor, SafetyEngine};
use agent_bus::ToolBus;
use anyhow::{bail, ensure, Context, Result};
use cortex_core::{ExecutionPlan, LlmProvider, SystemContext};
use serde_json::{json, Value};
use std::{collections::{HashMap, VecDeque}, path::PathBuf, sync::{atomic::{AtomicBool, Ordering}, Arc}, time::{Duration, Instant}};
use synapsed::{FileSystemIndexer, SemanticJournal};
use tokio::sync::{broadcast, Mutex};
use uuid::Uuid;

pub(crate) struct PendingPlan {
    pub session: String,
    plan: ExecutionPlan,
    cwd: PathBuf,
    created: Instant,
}

pub struct Daemon {
    pub workspace: PathBuf,
    pub journal: Mutex<SemanticJournal>,
    pub events: broadcast::Sender<Value>,
    pub actions: Mutex<()>,
    pub stopping: AtomicBool,
    pub(crate) watcher: Mutex<Option<feather_watch::WorkspaceWatcher>>,
    pub(crate) telemetry: Mutex<feather_watch::Telemetry>,
    pub(crate) data_dir: PathBuf,
    pub(crate) snapshots: aegis_snapshot::SnapshotStore,
    pub(crate) sessions: Mutex<HashMap<String, PathBuf>>,
    pub(crate) plans: Mutex<HashMap<String, PendingPlan>>,
    provider: Arc<dyn LlmProvider>,
    model: String,
    tools: ToolBus,
    safety: SafetyEngine,
    cache: Mutex<HashMap<(String, PathBuf), (Instant, ExecutionPlan)>>,
    logs: Mutex<VecDeque<Value>>,
    active: Mutex<Value>,
}

impl Daemon {
    pub fn new(data_dir: PathBuf, workspace: PathBuf, provider: Arc<dyn LlmProvider>, model: String) -> Result<Self> {
        std::fs::create_dir_all(&data_dir)?;
        let data_dir = data_dir.canonicalize()?;
        let workspace = workspace.canonicalize()?;
        ensure!(workspace.is_dir(), "workspace must be a directory");
        let journal = SemanticJournal::load(data_dir.join("journal.json"))?;
        let snapshots = aegis_snapshot::SnapshotStore::new(data_dir.join("snapshots"), vec![data_dir.clone()])?;
        let (events, _) = broadcast::channel(256);
        Ok(Self {
            workspace, journal: Mutex::new(journal), events, actions: Mutex::new(()),
            stopping: AtomicBool::new(false), sessions: Mutex::new(HashMap::new()),
            plans: Mutex::new(HashMap::new()), provider, model, tools: ToolBus::with_defaults(),
            safety: SafetyEngine::new(), cache: Mutex::new(HashMap::new()),
            watcher: Mutex::new(None), telemetry: Mutex::new(feather_watch::Telemetry::default()), data_dir, snapshots,
            logs: Mutex::new(VecDeque::new()), active: Mutex::new(Value::Null),
        })
    }

    pub async fn emit(&self, method: &str, params: Value) {
        let event = feather_ipc::notification(method, params);
        let mut logs = self.logs.lock().await;
        if logs.len() >= 200 { logs.pop_front(); }
        let mut summary = event.clone();
        if let Some(result) = summary["params"].get_mut("result") {
            if let Some(object) = result.as_object_mut() { object.remove("stdout"); object.remove("stderr"); }
        }
        if let Some(params) = summary["params"].as_object_mut() { params.remove("plan"); }
        logs.push_back(summary);
        let _ = self.events.send(event);
    }

    async fn cwd(&self, session: &str, params: &Value) -> Result<PathBuf> {
        let mut sessions = self.sessions.lock().await;
        let current = sessions.get(session).unwrap_or(&self.workspace);
        let path = match params["working_dir"].as_str() {
            Some(path) => current.join(path).canonicalize().context("Invalid working directory")?,
            None => current.clone(),
        };
        ensure!(path.is_dir(), "working_dir must be a directory");
        sessions.insert(session.to_owned(), path.clone());
        Ok(path)
    }

    pub async fn dispatch(&self, session: &str, method: &str, params: Value) -> Result<Value> {
        ensure!(!self.stopping.load(Ordering::Relaxed), "Daemon is shutting down");
        match method {
            "initialize" => Ok(json!({"protocolVersion":"2024-11-05", "capabilities":{"tools":{}},
                "serverInfo":{"name":"featherd","version":env!("CARGO_PKG_VERSION")}})),
            "ping" => Ok(json!({})),
            "session.open" => {
                let cwd = self.cwd(session, &params).await?;
                if let Some(watcher) = self.watcher.lock().await.as_mut() { watcher.watch(&cwd)?; }
                Ok(json!({"session_id":session,"working_dir":cwd,"model":self.model}))
            }
            "daemon.status" => Ok(json!({"model":self.model,"sessions":self.sessions.lock().await.len(),
                "entries":self.journal.lock().await.total_entries(),"active_plan":self.active.lock().await.clone(),
                "workspace":self.workspace,"telemetry":self.telemetry.lock().await.clone(),
                "embedding":{"dim":feather_embed::DIM,"offline":true,
                    "custom_model":feather_embed::local_model_status(&self.data_dir)},
                "sandbox":"Aegis command policy; host-user execution"})),
            "daemon.logs" => Ok(json!(self.logs.lock().await.clone())),
            "tools/list" => Ok(json!({"tools":self.tools.list_tools()})),
            "tools/call" => {
                let name = text(&params, "name")?;
                let cwd = self.cwd(session, &params).await?;
                let mut args = params.get("arguments").cloned().unwrap_or(json!({}));
                ensure!(args.is_object(), "arguments must be an object");
                let key = match name { "fs_find" => Some("directory"), "fs_read" => Some("path"), _ => None };
                if let Some(key) = key { args[key] = json!(cwd.join(args[key].as_str().unwrap_or("."))); }
                let result = self.tools.dispatch(name, args).await?;
                Ok(json!({"content":[{"type":"text","text":serde_json::to_string(&result)?}],"isError":false}))
            }
            "memory.search" => {
                let query = text(&params, "query")?.to_owned();
                Ok(json!(self.journal.lock().await.search(&query, 10)))
            }
            "snapshot.list" | "snapshot.rollback" | "snapshot.diff" | "snapshot.recover" => {
                let cwd = self.cwd(session, &params).await?;
                let _guard = self.actions.lock().await;
                let store = self.snapshots.clone();
                let method = method.to_owned();
                let value = tokio::task::spawn_blocking(move || -> Result<Value> {
                    let snapshots = store.list(&cwd)?;
                    if method == "snapshot.list" {
                        return Ok(json!(snapshots.iter().map(|s| json!({"id":s.id,"created":s.created,
                            "label":s.label,"status":s.status,"changed_paths":s.changed_paths(),
                            "excluded_directories":aegis_snapshot::EXCLUDED_DIRS})).collect::<Vec<_>>()));
                    }
                    let id = params["snapshot_id"].as_str().map(str::to_owned)
                        .or_else(|| snapshots.iter().find(|s| s.status == "complete" && !s.changed_paths().is_empty()).map(|s| s.id.clone()))
                        .context("No completed snapshot for this workspace")?;
                    ensure!(store.load(&id)?.root == cwd, "Snapshot belongs to a different workspace");
                    if method == "snapshot.diff" { return Ok(json!({"diff":store.diff(&id)?})); }
                    let snapshot = if method == "snapshot.recover" { store.finish(&id)? } else { store.rollback(&id)? };
                    Ok(json!({"snapshot_id":snapshot.id,"status":snapshot.status,"changed_paths":snapshot.changed_paths()}))
                }).await??;
                if let Some(watcher) = self.watcher.lock().await.as_ref() { watcher.rescan.store(true, Ordering::Relaxed); }
                Ok(value)
            }
            "memory.index" => {
                let cwd = self.cwd(session, &params).await?;
                let mut journal = self.journal.lock().await;
                let count = FileSystemIndexer::index_directory_excluding(&mut journal, &cwd, 10_000, std::slice::from_ref(&self.data_dir))?;
                journal.persist()?;
                Ok(json!({"indexed":count}))
            }
            "intent.plan_and_execute" => {
                let intent = text(&params, "intent")?;
                let cwd = self.cwd(session, &params).await?;
                let key = (intent.to_owned(), cwd.clone());
                let cached = self.cache.lock().await.get(&key).filter(|(t,_)| t.elapsed() < Duration::from_secs(30)).map(|(_,p)| p.clone());
                let plan = match cached {
                    Some(plan) => plan,
                    None => {
                        let dir = cwd.clone();
                        let context = tokio::task::spawn_blocking(move || SystemContext::capture_in(dir)).await?;
                        let plan = self.provider.generate_plan(intent, &context).await?;
                        ensure!(!plan.steps.is_empty() && plan.steps.len() <= 32, "Plan must contain 1 to 32 steps");
                        ensure!(plan.steps.iter().map(|s| s.command.len() + s.description.len()).sum::<usize>() <= 65_536,
                            "Plan exceeds 64 KiB");
                        let mut cache = self.cache.lock().await;
                        if cache.len() >= 64 { cache.clear(); }
                        cache.insert(key, (Instant::now(), plan.clone()));
                        plan
                    }
                };
                for step in &plan.steps { ensure!(self.safety.assess(&step.command).allowed, "Plan contains a critical command"); }
                if params["auto_confirm"] == true {
                    self.execute_plan(session, plan, cwd).await
                } else {
                    let id = Uuid::new_v4().to_string();
                    let mut plans = self.plans.lock().await;
                    plans.retain(|_, p| p.created.elapsed() < Duration::from_secs(600));
                    ensure!(plans.len() < 256, "Too many pending plans");
                    plans.insert(id.clone(), PendingPlan { session:session.into(), plan:plan.clone(), cwd, created:Instant::now() });
                    Ok(json!({"status":"awaiting_confirmation","plan_id":id,"plan":plan}))
                }
            }
            "plan.execute" | "plan.cancel" => {
                let id = text(&params,"plan_id")?;
                let mut plans = self.plans.lock().await;
                let pending = plans.get(id).context("Unknown or already consumed plan")?;
                ensure!(pending.session == session, "Plan belongs to another session");
                let pending = plans.remove(id).unwrap();
                drop(plans);
                if method == "plan.cancel" { return Ok(json!({"status":"cancelled"})); }
                ensure!(pending.created.elapsed() < Duration::from_secs(600), "Plan expired; request a fresh plan");
                self.execute_plan(session, pending.plan, pending.cwd).await
            }
            "command.execute" => {
                let command = text(&params,"command")?;
                let assessment = self.safety.assess(command);
                ensure!(assessment.allowed, "Command blocked: {}", assessment.warnings.join("; "));
                if assessment.requires_confirmation && params["confirmed"] != true {
                    return Ok(json!({"status":"awaiting_confirmation","assessment":assessment}));
                }
                let cwd = self.cwd(session, &params).await?;
                let plan = ExecutionPlan::new(command.into(), command.into(), vec![("Shell command".into(), command.into())]);
                self.execute_plan(session, plan, cwd).await
            }
            _ => bail!("Unknown method: {method}"),
        }
    }

    async fn execute_plan(&self, session: &str, plan: ExecutionPlan, cwd: PathBuf) -> Result<Value> {
        let task = Uuid::new_v4().to_string();
        self.emit("plan.queued", json!({"session_id":session,"task_id":task})).await;
        let _guard = self.actions.lock().await;
        ensure!(!self.stopping.load(Ordering::Relaxed), "Daemon is shutting down");
        let store = self.snapshots.clone();
        let root = cwd.clone();
        let label = plan.intent.clone();
        let snapshot = tokio::task::spawn_blocking(move || store.capture(&root, &label)).await??;
        let executor = SafeExecutor::new(ExecutionConfig { working_dir:cwd, ..Default::default() });
        let mut results = Vec::new();
        let mut status = "completed";
        for step in &plan.steps {
            let progress = json!({"session_id":session,"task_id":task,"step":step.step_number,"total":plan.steps.len(),
                "status":"executing","command":step.command,"plan":plan});
            *self.active.lock().await = progress.clone();
            self.emit("plan.step_progress", progress).await;
            if !self.safety.assess(&step.command).allowed { status = "blocked"; break; }
            match executor.execute(&step.command).await {
                Ok(result) => {
                    let failed = result.exit_code != Some(0) || result.timed_out;
                    if failed {
                        if let Some(diagnostic) = feather_watch::diagnose(&result.stderr) {
                            self.emit("command.diagnostic", json!({"session_id":session,"command":step.command,"diagnostic":diagnostic})).await;
                        }
                    }
                    self.journal.lock().await.record("shell_command", &step.command,
                        &format!("{}\n{}", result.stdout, result.stderr), vec![format!("session:{session}")]);
                    self.emit("plan.step_finished", json!({"session_id":session,"task_id":task,"step":step.step_number,"result":result})).await;
                    results.push(json!(result));
                    if failed { status = "failed"; break; }
                }
                Err(e) => { results.push(json!({"command":step.command,"error":e.to_string()})); status = "failed"; break; }
            }
        }
        *self.active.lock().await = Value::Null;
        self.cache.lock().await.clear();
        let store = self.snapshots.clone();
        let snapshot_id = snapshot.id.clone();
        let finished = tokio::task::spawn_blocking(move || -> Result<Option<String>> {
            let finished = store.finish(&snapshot_id)?;
            if finished.changed_paths().is_empty() { store.discard_unchanged(&finished)?; Ok(None) }
            else { Ok(Some(finished.id)) }
        }).await?;
        let (snapshot_id, snapshot_error) = match finished {
            Ok(id) => (id, None),
            Err(e) => (Some(snapshot.id), Some(format!("Snapshot finalization failed; baseline retained: {e:#}"))),
        };
        self.journal.lock().await.persist()?;
        let result = json!({"status":status,"task_id":task,"results":results,"snapshot_id":snapshot_id,
            "snapshot_error":snapshot_error,"rollback_scope":"workspace files; excludes .git, .feather, target, node_modules, __pycache__, and daemon data"});
        self.emit("plan.finished", json!({"session_id":session,"task_id":task,"status":status})).await;
        Ok(result)
    }
}

fn text<'a>(params: &'a Value, key: &str) -> Result<&'a str> {
    let value = params[key].as_str().with_context(|| format!("{key} must be a string"))?;
    ensure!(!value.trim().is_empty() && value.len() <= 32_768, "{key} must contain 1 to 32768 bytes");
    Ok(value)
}
