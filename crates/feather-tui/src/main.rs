use anyhow::Result;
use clap::Parser;
use crossterm::{
    event::{self, Event, KeyCode, KeyModifiers},
    execute,
    terminal::{disable_raw_mode, enable_raw_mode, EnterAlternateScreen, LeaveAlternateScreen},
};
use feather_ipc::Client;
use ratatui::{
    backend::CrosstermBackend,
    layout::{Constraint, Direction, Layout},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, Borders, List, ListItem, Paragraph},
    Terminal,
};
use serde_json::{json, Value};
use std::{io, time::Duration};

#[derive(Parser)]
#[command(version, about = "FeatherOS dual-pane TUI (shell + cognitive inspector)")]
struct Args {
    #[arg(long, default_value_t = feather_ipc::default_endpoint())]
    endpoint: String,
}

struct App {
    input: String,
    history: Vec<String>,
    status: Value,
    logs: Vec<String>,
    pending_plan: Option<Value>,
    message: String,
}

impl App {
    fn new() -> Self {
        Self { input: String::new(), history: vec!["FeatherOS TUI · connected. Type a shell command or request, Enter to run.".into()], status: Value::Null, logs: vec![], pending_plan: None, message: ":status :tools :memory <q> :index :logs :snapshots :undo [id] · Ctrl-C quit".into() }
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();
    let mut client = Client::connect(&args.endpoint).await?;
    let cwd = std::env::current_dir()?;
    client.call("session.open", json!({"working_dir": cwd})).await?;

    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;
    let mut app = App::new();
    refresh(&mut client, &mut app).await;

    let result = run_loop(&mut terminal, &mut client, &mut app, &cwd).await;

    disable_raw_mode()?;
    execute!(terminal.backend_mut(), LeaveAlternateScreen)?;
    terminal.show_cursor()?;
    result
}

async fn refresh(client: &mut Client, app: &mut App) {
    if let Ok(s) = client.call("daemon.status", json!({})).await {
        app.status = s;
    }
    if let Ok(logs) = client.call("daemon.logs", json!({})).await {
        if let Some(arr) = logs.as_array() {
            app.logs = arr.iter().rev().take(12).rev()
                .map(|e| format!("{} {}", e["method"].as_str().unwrap_or("?"), e["params"]))
                .collect();
        }
    }
}

async fn run_loop(
    terminal: &mut Terminal<CrosstermBackend<io::Stdout>>,
    client: &mut Client,
    app: &mut App,
    cwd: &std::path::Path,
) -> Result<()> {
    let mut tick = tokio::time::interval(Duration::from_secs(3));
    loop {
        terminal.draw(|f| {
            let root = Layout::default().direction(Direction::Vertical)
                .constraints([Constraint::Min(0), Constraint::Length(3), Constraint::Length(1)]).split(f.area());
            let panes = Layout::default().direction(Direction::Horizontal)
                .constraints([Constraint::Percentage(58), Constraint::Percentage(42)]).split(root[0]);

            let history: Vec<ListItem> = app.history.iter().rev().take(40).rev()
                .map(|h| ListItem::new(Line::from(vec![Span::raw(h.clone())]))).collect();
            f.render_widget(List::new(history).block(Block::default().borders(Borders::ALL).title("FEATHER SHELL")), panes[0]);

            let model = app.status["model"].as_str().unwrap_or("?");
            let entries = app.status["entries"].to_string();
            let tele = &app.status["telemetry"];
            let mut right = vec![
                Line::from(vec![Span::styled(format!("Model: {model}"), Style::default().add_modifier(Modifier::BOLD))]),
                Line::from(vec![Span::raw(format!("Entries: {entries} · Sessions: {}", app.status["sessions"]))]),
                Line::from(vec![Span::raw(format!("CPU: {}% · RAM free: {} MB · Disk free: {} GB",
                    tele["cpu_percent"], tele["memory_available_mb"], tele["disk_available_gb"]))]),
                Line::from(vec![Span::styled("Active plan:".to_owned(), Style::default().fg(Color::Yellow))]),
            ];
            if let Some(plan) = app.pending_plan.as_ref().and_then(|p| p["plan"].as_object()).or_else(|| app.status["active_plan"].as_object()) {
                for (i, s) in plan["steps"].as_array().map(|v| v.iter()).unwrap_or([].iter()).enumerate().take(6) {
                    right.push(Line::from(vec![Span::raw(format!("  {}. {}", i + 1, s["description"].as_str().unwrap_or("?")))]));
                }
            } else {
                right.push(Line::from(vec![Span::raw("  (idle)")]));
            }
            right.push(Line::from(vec![Span::styled("Recent events:".to_owned(), Style::default().fg(Color::Yellow))]));
            for log in app.logs.iter().take(6) {
                right.push(Line::from(vec![Span::styled(log.chars().take(64).collect::<String>(), Style::default().fg(Color::DarkGray))]));
            }
            if !app.message.is_empty() {
                right.push(Line::from(vec![Span::styled(app.message.clone(), Style::default().fg(Color::Cyan))]));
            }
            f.render_widget(Paragraph::new(right).block(Block::default().borders(Borders::ALL).title("COGNITIVE INSPECTOR")), panes[1]);
            f.render_widget(Paragraph::new(app.input.clone()).block(Block::default().borders(Borders::ALL).title("Input (Enter=run, Esc=clear, Ctrl-C=quit)")), root[1]);
            f.render_widget(Paragraph::new("[F1] Help | [F2] Memory | [F3] Tools | [F4] Logs | [F12] Refresh"), root[2]);
        })?;

        tokio::select! {
            _ = tick.tick() => { refresh(client, app).await; }
            _ = async {
                if event::poll(Duration::from_millis(150))? {
                    handle_key(client, app, cwd).await?;
                }
                Ok::<_, anyhow::Error>(())
            } => {}
        }
        if app.message == "__quit__" { break; }
    }
    Ok(())
}

async fn handle_key(client: &mut Client, app: &mut App, cwd: &std::path::Path) -> Result<()> {
    if let Event::Key(key) = event::read()? {
        if key.modifiers.contains(KeyModifiers::CONTROL) && key.code == KeyCode::Char('c') {
            app.message = "__quit__".into();
            return Ok(());
        }
        match key.code {
            KeyCode::Esc => app.input.clear(),
            KeyCode::Backspace => { app.input.pop(); }
            KeyCode::Enter => {
                let line = app.input.trim().to_owned();
                app.input.clear();
                if line.is_empty() { return Ok(()); }
                if line == ":quit" || line == "exit" { app.message = "__quit__".into(); return Ok(()); }
                execute_line(client, app, cwd, &line).await;
                refresh(client, app).await;
            }
            KeyCode::Char(c) => app.input.push(c),
            KeyCode::F(12) => refresh(client, app).await,
            KeyCode::F(1) => app.message = ":status :tools :memory <q> :index :snapshots :undo [id]".into(),
            _ => {}
        }
    }
    Ok(())
}

async fn execute_line(client: &mut Client, app: &mut App, cwd: &std::path::Path, line: &str) {
    app.history.push(format!("> {line}"));
    let out = if let Some(b) = line.strip_prefix(':') {
        let (name, query) = b.split_once(' ').unwrap_or((b, ""));
        let req = match name {
            "status" => ("daemon.status", json!({})),
            "tools" => ("tools/list", json!({})),
            "memory" => ("memory.search", json!({"query": query})),
            "index" => ("memory.index", json!({"working_dir": cwd})),
            "logs" => ("daemon.logs", json!({})),
            "snapshots" => ("snapshot.list", json!({"working_dir": cwd})),
            "undo" => ("snapshot.rollback", json!({"working_dir": cwd, "snapshot_id": if query.is_empty() {None} else {Some(query)}})),
            _ => { app.history.push(format!("unknown builtin :{name}")); return; }
        };
        match client.call(req.0, req.1).await {
            Ok(v) => truncate(serde_json::to_string_pretty(&v).unwrap_or_default()),
            Err(e) => format!("error: {e:#}"),
        }
    } else if let Some(id) = line.strip_prefix("y:") {
        // Confirm a pending plan shown in the inspector.
        let id = if id.trim().is_empty() {
            app.pending_plan.as_ref().and_then(|p| p["plan_id"].as_str()).unwrap_or("").to_owned()
        } else { id.trim().to_owned() };
        match client.call("plan.execute", json!({"plan_id": id})).await {
            Ok(v) => { app.pending_plan = None; format_result(&v) }
            Err(e) => format!("error: {e:#}"),
        }
    } else {
        // Natural language goes through the planner; shell text executes directly.
        let is_nl = line.split_whitespace().count() > 3 || line.contains('?');
        let (method, params) = if is_nl {
            ("intent.plan_and_execute", json!({"intent": line, "working_dir": cwd, "auto_confirm": false}))
        } else {
            ("command.execute", json!({"command": line, "working_dir": cwd, "confirmed": true}))
        };
        match client.call(method, params).await {
            Ok(v) if v["status"] == "awaiting_confirmation" => {
                app.pending_plan = Some(v.clone());
                format!("plan awaiting confirmation — type y: to execute\n{}", truncate(serde_json::to_string_pretty(&v["plan"]).unwrap_or_default()))
            }
            Ok(v) => { app.pending_plan = None; format_result(&v) }
            Err(e) => format!("error: {e:#}"),
        }
    };
    for chunk in out.lines().take(20).map(str::to_owned) {
        app.history.push(chunk);
    }
    app.history.truncate(300);
}

fn format_result(v: &Value) -> String {
    let mut s = format!("status: {}", v["status"]);
    if let Some(id) = v["snapshot_id"].as_str() { s.push_str(&format!(" · snapshot {id} (:undo to restore)")); }
    if let Some(results) = v["results"].as_array() {
        for r in results.iter().take(3) {
            if let Some(o) = r["stdout"].as_str() { s.push_str("\n"); s.push_str(&truncate(o.to_owned())); }
            if let Some(e) = r["stderr"].as_str() { if !e.is_empty() { s.push_str("\nstderr: "); s.push_str(&truncate(e.to_owned())); } }
        }
    }
    s
}

fn truncate(s: String) -> String {
    s.chars().take(1200).collect()
}
