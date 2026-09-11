use anyhow::{bail, Result};
use clap::{Parser, Subcommand};
use colored::Colorize;
use cortex_core::{IntentClassifier, IntentType};
use feather_ipc::Client;
use rustyline::{error::ReadlineError, DefaultEditor};
use serde_json::{json, Value};
use std::{io::{self, Write}, path::PathBuf};

#[derive(Parser)]
#[command(name = "feather", version, about = "FeatherOS cognitive shell, connected to featherd")]
struct Args {
    #[arg(short = 'c', long)]
    command: Option<String>,
    #[arg(long, default_value_t = feather_ipc::default_endpoint())]
    endpoint: String,
    /// Explicitly approve a plan or a high-risk command (critical commands remain blocked).
    #[arg(short = 'y', long)]
    yes: bool,
    #[command(subcommand)]
    action: Option<Action>,
}

#[derive(Subcommand)]
enum Action {
    Status,
    /// Restore the latest completed snapshot in this workspace, or a specific snapshot.
    Rollback { snapshot_id: Option<String> },
}

pub async fn run() -> Result<()> {
    let args = Args::parse();
    let mut client = Client::connect(&args.endpoint).await?;
    let mut cwd = std::env::current_dir()?;
    let session = client.call("session.open", json!({"working_dir":cwd})).await?;
    if let Some(action) = args.action {
        let result = match action {
            Action::Status => client.call("daemon.status", json!({})).await?,
            Action::Rollback { snapshot_id } => client.call("snapshot.rollback", json!({"snapshot_id":snapshot_id,"working_dir":cwd})).await?,
        };
        println!("{}", serde_json::to_string_pretty(&result)?);
        return Ok(());
    }
    let classifier = IntentClassifier::new();
    if let Some(command) = args.command {
        return handle(&mut client, &classifier, &mut cwd, &command, args.yes).await;
    }
    println!("{}", "FeatherOS · connected to featherd".bold().cyan());
    println!("Model: {} · :help for commands", session["model"].as_str().unwrap_or("unknown"));
    let mut editor = DefaultEditor::new()?;
    let history = feather_ipc::data_dir().join("shell_history");
    let _ = editor.load_history(&history);
    loop {
        let prompt = format!("feather:{}> ", cwd.display());
        match editor.readline(&prompt) {
            Ok(line) => {
                let line = line.trim();
                if line.is_empty() { continue; }
                if matches!(line, "exit" | ":quit" | ":exit") { break; }
                let _ = editor.add_history_entry(line);
                if let Err(e) = handle(&mut client, &classifier, &mut cwd, line, args.yes).await {
                    eprintln!("{}", format!("{e:#}").red());
                }
            }
            Err(ReadlineError::Interrupted) => continue,
            Err(ReadlineError::Eof) => break,
            Err(e) => return Err(e.into()),
        }
    }
    std::fs::create_dir_all(feather_ipc::data_dir())?;
    editor.save_history(&history)?;
    Ok(())
}

fn confirm() -> Result<bool> {
    print!("Execute? [y/N]: ");
    io::stdout().flush()?;
    let mut answer = String::new();
    io::stdin().read_line(&mut answer)?;
    Ok(answer.trim().eq_ignore_ascii_case("y"))
}

async fn handle(client: &mut Client, classifier: &IntentClassifier, cwd: &mut PathBuf, input: &str, yes: bool) -> Result<()> {
    if input == "cd" || input.starts_with("cd ") {
        let target = input.strip_prefix("cd").unwrap().trim().trim_matches('"').trim_matches('\'');
        let target = if target.is_empty() || target == "~" {
            PathBuf::from(std::env::var("HOME").or_else(|_| std::env::var("USERPROFILE"))?)
        } else { cwd.join(target) };
        let target = target.canonicalize()?;
        anyhow::ensure!(target.is_dir(), "Not a directory");
        client.call("session.open", json!({"working_dir":target})).await?;
        *cwd = target;
        return Ok(());
    }
    if let Some(builtin) = input.strip_prefix(':') {
        let (name, query) = builtin.split_once(' ').unwrap_or((builtin, ""));
        let value = match name {
            "help" => {
                println!(":status · :tools · :memory <query> · :index · :logs · :undo [id] · :diff [id] · :snapshots · :quit\nCommands and natural-language plans execute through featherd. Start feather-tui for the dashboard.");
                return Ok(());
            }
            "status" => client.call("daemon.status", json!({})).await?,
            "tools" => client.call("tools/list", json!({})).await?,
            "memory" => client.call("memory.search", json!({"query":query})).await?,
            "index" => client.call("memory.index", json!({"working_dir":cwd})).await?,
            "logs" => client.call("daemon.logs", json!({})).await?,
            "snapshots" => client.call("snapshot.list", json!({"working_dir":cwd})).await?,
            "diff" => {
                let value = client.call("snapshot.diff", json!({"working_dir":cwd,"snapshot_id":if query.is_empty() {None} else {Some(query)}})).await?;
                for line in value["diff"].as_str().unwrap_or("").lines() {
                    if line.starts_with('+') { println!("{}", line.green()); }
                    else if line.starts_with('-') { println!("{}", line.red()); }
                    else { println!("{line}"); }
                }
                return Ok(());
            }
            "undo" => client.call("snapshot.rollback", json!({"working_dir":cwd,"snapshot_id":if query.is_empty() {None} else {Some(query)}})).await?,
            _ => bail!("Unknown builtin: {name}"),
        };
        println!("{}", serde_json::to_string_pretty(&value)?);
        return Ok(());
    }
    let (method, mut params, is_plan) = match classifier.classify(input) {
        IntentType::NaturalLanguage(intent) => ("intent.plan_and_execute", json!({"intent":intent,"working_dir":cwd,"auto_confirm":yes}), true),
        _ => ("command.execute", json!({"command":input,"working_dir":cwd,"confirmed":yes}), false),
    };
    let mut result = client.call_with_events(method, params.clone(), display_event).await?;
    if result["status"] == "awaiting_confirmation" {
        if is_plan {
            println!("{}", serde_json::to_string_pretty(&result["plan"])?);
        } else {
            println!("{}", serde_json::to_string_pretty(&result["assessment"])?);
        }
        if confirm()? {
            result = if is_plan {
                client.call_with_events("plan.execute", json!({"plan_id":result["plan_id"]}), display_event).await?
            } else {
                params["confirmed"] = json!(true);
                client.call_with_events(method, params, display_event).await?
            };
        } else {
            if is_plan { client.call("plan.cancel", json!({"plan_id":result["plan_id"]})).await?; }
            println!("Cancelled.");
            return Ok(());
        }
    }
    // Results are authoritative even if a progress event was lost or overtaken by the response.
    if let Some(results) = result["results"].as_array() {
        for step in results {
            print!("{}", step["stdout"].as_str().unwrap_or(""));
            eprint!("{}", step["stderr"].as_str().unwrap_or("").red());
            if let Some(error) = step["error"].as_str() { eprintln!("{error}"); }
        }
    }
    if let Some(id) = result["snapshot_id"].as_str() { println!("Snapshot: {id} · :undo to restore workspace changes"); }
    if let Some(error) = result["snapshot_error"].as_str() { eprintln!("{error}"); }
    if result["status"] != "completed" { bail!("Execution {}", result["status"]); }
    Ok(())
}

fn display_event(event: Value) {
    if event["method"] == "plan.step_progress" {
        let p = &event["params"];
        println!("[{} / {}] $ {}", p["step"], p["total"], p["command"].as_str().unwrap_or(""));
    } else if event["method"] == "perception.alert" || event["method"] == "command.diagnostic" {
        eprintln!("{}", event["params"]);
    }
}
