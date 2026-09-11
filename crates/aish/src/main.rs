use aegis_exec::{ExecutionConfig, RiskLevel, SafeExecutor, SafetyEngine};
use agent_bus::ToolBus;
use clap::Parser;
use colored::*;
use cortex_core::{
    HeuristicProvider, IntentClassifier, IntentType, LlmProvider, OllamaProvider,
    SystemContext,
};
use rustyline::error::ReadlineError;
use rustyline::DefaultEditor;
use std::io::{self, Write};
use std::path::PathBuf;
use std::sync::Arc;
use synapsed::{FileSystemIndexer, SemanticJournal};

#[derive(Parser, Debug)]
#[command(name = "feather")]
#[command(about = "FeatherOS - Cross-Platform Cognitive Shell", long_about = None)]
struct CliArgs {
    /// Execute a single command or natural language intent and exit
    #[arg(short = 'c', long)]
    command: Option<String>,

    /// Provider type (heuristic, ollama)
    #[arg(long, default_value = "heuristic")]
    provider: String,

    /// Ollama endpoint (if using ollama provider)
    #[arg(long, default_value = "http://localhost:11434")]
    ollama_endpoint: String,

    /// Model name
    #[arg(long, default_value = "llama3.2:3b")]
    model: String,
}

struct ShellState {
    provider: Arc<dyn LlmProvider>,
    tool_bus: ToolBus,
    journal: SemanticJournal,
    classifier: IntentClassifier,
    safety: SafetyEngine,
    current_dir: PathBuf,
}

impl ShellState {
    fn new(args: &CliArgs) -> Self {
        let provider: Arc<dyn LlmProvider> = if args.provider == "ollama" {
            Arc::new(OllamaProvider::new(
                args.ollama_endpoint.clone(),
                args.model.clone(),
            ))
        } else {
            Arc::new(HeuristicProvider)
        };

        let mut journal = SemanticJournal::load_or_new();
        let current_dir = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        let _ = FileSystemIndexer::index_directory(&mut journal, &current_dir, 30);

        Self {
            provider,
            tool_bus: ToolBus::with_defaults(),
            journal,
            classifier: IntentClassifier::new(),
            safety: SafetyEngine::new(),
            current_dir,
        }
    }
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let args = CliArgs::parse();
    let mut state = ShellState::new(&args);

    if let Some(cmd) = args.command {
        handle_input(&mut state, &cmd).await;
        return Ok(());
    }

    print_welcome_banner(&state);

    let mut rl = DefaultEditor::new()?;
    let history_file = dirs_next().join(".feather_history");
    let _ = rl.load_history(&history_file);

    loop {
        let context = SystemContext::capture();
        let prompt_str = format_prompt(&context);

        match rl.readline(&prompt_str) {
            Ok(line) => {
                let trimmed = line.trim();
                if trimmed.is_empty() {
                    continue;
                }
                let _ = rl.add_history_entry(trimmed);

                if trimmed == ":exit" || trimmed == ":quit" || trimmed == "exit" {
                    println!("{}", "Exiting FeatherOS AI Shell. Goodbye!".green());
                    break;
                }

                handle_input(&mut state, trimmed).await;
            }
            Err(ReadlineError::Interrupted) => {
                println!("{}", "^C".yellow());
                continue;
            }
            Err(ReadlineError::Eof) => {
                println!("{}", "Exiting...".green());
                break;
            }
            Err(err) => {
                eprintln!("Shell error: {:?}", err);
                break;
            }
        }
    }

    let _ = rl.save_history(&history_file);
    Ok(())
}

fn dirs_next() -> PathBuf {
    std::env::var("HOME")
        .or_else(|_| std::env::var("USERPROFILE"))
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from("."))
}

fn print_welcome_banner(state: &ShellState) {
    let ctx = SystemContext::capture();
    let platform = std::env::consts::OS.to_uppercase();
    println!("{}", "================================================================".cyan());
    println!(
        "{} {}",
        "  FEATHER OS - Cross-Platform Cognitive Shell (v0.1.0)".bold().bright_cyan(),
        format!("| {}", platform).bright_black()
    );
    println!("{}", "================================================================".cyan());
    println!(
        "  OS: {} | Kernel/Arch: {}",
        ctx.os_name.green(),
        ctx.kernel.yellow()
    );
    println!(
        "  Indexed Files: {} | Available RAM: {} MB | Free Disk: {} GB",
        state.journal.total_entries().to_string().cyan(),
        ctx.memory_available_mb.unwrap_or(0).to_string().cyan(),
        ctx.disk_available_gb.unwrap_or(0).to_string().cyan()
    );
    println!(
        "  Mode: {}",
        "Hybrid Native Shell + Cross-Platform Intent Planner".bright_magenta()
    );
    println!(
        "  Type {} for help, or type any command or English request.",
        ":help".bold().yellow()
    );
    println!("{}", "================================================================\n".cyan());
}

fn format_prompt(ctx: &SystemContext) -> String {
    let cwd_display = ctx
        .current_dir
        .file_name()
        .map(|n| n.to_string_lossy().to_string())
        .unwrap_or_else(|| "/".to_string());

    let git_part = match &ctx.git_branch {
        Some(b) => {
            let status_mark = if ctx.git_dirty { "*" } else { "" };
            format!(" ({}{})", b.magenta(), status_mark.yellow())
        }
        None => String::new(),
    };

    format!(
        "{}:{}{}> ",
        "feather".bright_cyan().bold(),
        cwd_display.blue().bold(),
        git_part
    )
}

async fn handle_input(state: &mut ShellState, input: &str) {
    let intent = state.classifier.classify(input);

    match intent {
        IntentType::BuiltinShell(sub) => {
            handle_builtin(state, &sub).await;
        }
        IntentType::DirectCommand(cmd) => {
            handle_direct_command(state, &cmd).await;
        }
        IntentType::NaturalLanguage(prompt) => {
            handle_natural_language(state, &prompt).await;
        }
    }
}

async fn handle_builtin(state: &mut ShellState, sub: &str) {
    let parts: Vec<&str> = sub.split_whitespace().collect();
    let command = parts.first().copied().unwrap_or("");

    match command {
        "help" => {
            println!("\n{}", "Available Commands & Shortcuts:".bold().yellow());
            println!("  {}             Show this help guide", ":help".cyan());
            println!("  {}           Display system telemetry and sandbox status", ":status".cyan());
            println!("  {}            List MCP tools available on the Agent Bus", ":tools".cyan());
            println!("  {}    Search semantic memory and journal", ":memory <query>".cyan());
            println!("  {}            Re-index the current directory files", ":index".cyan());
            println!("  {}             Exit the shell\n", ":quit / :exit".cyan());
            println!("{}", "Usage Tips:".bold().yellow());
            println!("  • Direct execution: Just type normal commands (e.g. `ls -la` on Unix, `dir` on Windows)");
            println!("  • Natural language: Type English questions or tasks (e.g. `check disk space and find large files`)");
            println!("  • Cross-platform: Runs natively on Linux, macOS, and Windows!\n");
        }
        "status" => {
            let ctx = SystemContext::capture();
            let sandbox = aegis_exec::SandboxCapability::detect();
            println!("\n{}", "System Telemetry:".bold().yellow());
            println!("  Platform:   {}", sandbox.platform);
            println!("  User:       {}", ctx.username);
            println!("  Host:       {}", ctx.hostname);
            println!("  OS:         {}", ctx.os_name);
            println!("  Kernel/Arch:{}", ctx.kernel);
            println!("  Directory:  {}", ctx.current_dir.display());
            println!("  Memory:     {} MB available", ctx.memory_available_mb.unwrap_or(0));
            println!("  Disk:       {} GB free", ctx.disk_available_gb.unwrap_or(0));
            println!("\n{}", "Sandbox & Security Capabilities:".bold().yellow());
            println!("  Bubblewrap: {}", if sandbox.has_bubblewrap { "Available".green() } else { "Not installed (fallback enabled)".yellow() });
            println!("  User Namespaces: {}", if sandbox.has_user_namespaces { "Supported".green() } else { "Not available on this platform/config".yellow() });
            println!("  Active Tech: {:?}\n", sandbox.active_tech);
        }
        "tools" => {
            let tools = state.tool_bus.list_tools();
            println!("\n{}", "Agent Bus Registered Tools:".bold().yellow());
            for t in tools {
                println!("  • {} - {}", t["name"].as_str().unwrap_or("").bold().cyan(), t["description"].as_str().unwrap_or(""));
            }
            println!();
        }
        "memory" => {
            let query = parts.get(1..).map(|p| p.join(" ")).unwrap_or_default();
            if query.is_empty() {
                println!("{}", "Please provide a search query: :memory <term>".yellow());
                return;
            }
            let hits = state.journal.search(&query, 5);
            println!("\n{} '{}':", "Semantic Journal Search for".bold().yellow(), query);
            if hits.is_empty() {
                println!("  (No matches found in memory journal)\n");
            } else {
                for (i, hit) in hits.iter().enumerate() {
                    println!(
                        "  [{}] ({}) {} (relevance: {:.1})",
                        i + 1,
                        hit.entry.category.cyan(),
                        hit.entry.title.bold(),
                        hit.relevance_score
                    );
                    let snippet: String = hit.entry.content.lines().take(2).collect::<Vec<&str>>().join(" ");
                    println!("      {}\n", snippet.bright_black());
                }
            }
        }
        "index" => {
            let count = FileSystemIndexer::index_directory(&mut state.journal, &state.current_dir, 50).unwrap_or(0);
            println!("Indexed {} files into semantic memory.\n", count.to_string().green());
        }
        _ => {
            println!("Unknown builtin command '{}'. Type :help for commands.", command);
        }
    }
}

async fn handle_direct_command(state: &mut ShellState, cmd: &str) {
    if cmd.is_empty() {
        return;
    }

    // Special handling for `cd`
    if cmd.starts_with("cd ") || cmd == "cd" {
        let target = cmd.strip_prefix("cd").unwrap().trim();
        let new_path = if target.is_empty() || target == "~" {
            dirs_next()
        } else {
            state.current_dir.join(target)
        };

        match std::fs::canonicalize(&new_path) {
            Ok(canonical) => {
                let _ = std::env::set_current_dir(&canonical);
                state.current_dir = canonical;
            }
            Err(e) => {
                eprintln!("cd: {}: {}", target, e);
            }
        }
        return;
    }

    // Assess safety
    let assessment = state.safety.assess(cmd);
    if !assessment.allowed {
        println!("{}", "Execution blocked by Aegis Safety Guard:".bold().red());
        for w in &assessment.warnings {
            println!("  • {}", w.red());
        }
        return;
    }

    if assessment.requires_confirmation {
        println!("{}", "Warning: Potentially hazardous command detected:".yellow());
        for w in &assessment.warnings {
            println!("  • {}", w.yellow());
        }
        print!("Proceed with execution? [y/N]: ");
        let _ = io::stdout().flush();
        let mut answer = String::new();
        if io::stdin().read_line(&mut answer).is_err() || !answer.trim().eq_ignore_ascii_case("y") {
            println!("{}", "Aborted.".bright_black());
            return;
        }
    }

    let config = ExecutionConfig {
        working_dir: state.current_dir.clone(),
        ..Default::default()
    };
    let executor = SafeExecutor::new(config);

    match executor.execute(cmd).await {
        Ok(result) => {
            if !result.stdout.is_empty() {
                print!("{}", result.stdout);
            }
            if !result.stderr.is_empty() {
                eprint!("{}", result.stderr.red());
            }

            state.journal.record(
                "shell_command",
                cmd,
                &result.stdout,
                vec!["command".to_string(), format!("exit:{}", result.exit_code.unwrap_or(-1))],
            );
        }
        Err(e) => {
            eprintln!("Execution error: {}", e);
        }
    }
}

async fn handle_natural_language(state: &mut ShellState, prompt: &str) {
    println!("{}", "Thinking... (Generating cognitive plan)".bright_cyan());

    let ctx = SystemContext::capture();
    match state.provider.generate_plan(prompt, &ctx).await {
        Ok(plan) => {
            println!("\n{}", "Proposed Execution Plan:".bold().yellow());
            println!("  Summary: {}\n", plan.summary.bright_white());

            for step in &plan.steps {
                let badge = match step.safety.risk_level {
                    RiskLevel::Safe => "[SAFE]".green(),
                    RiskLevel::Moderate => "[MODERATE]".yellow(),
                    RiskLevel::High => "[HIGH RISK]".red(),
                    RiskLevel::Critical => "[CRITICAL]".bold().red(),
                };

                println!("  Step {}: {} {}", step.step_number, badge, step.description.bold());
                println!("    $ {}", step.command.cyan());
                for w in &step.safety.warnings {
                    println!("    ⚠️  {}", w.yellow());
                }
            }

            if plan.highest_risk == RiskLevel::Critical {
                println!("\n{}", "Execution prohibited: Plan contains critical destructive actions.".bold().red());
                return;
            }

            print!("\nExecute this plan? [Y/n]: ");
            let _ = io::stdout().flush();
            let mut answer = String::new();
            if io::stdin().read_line(&mut answer).is_err() {
                println!("{}", "Aborted.".bright_black());
                return;
            }

            let trimmed = answer.trim();
            if !trimmed.is_empty() && !trimmed.eq_ignore_ascii_case("y") {
                println!("{}", "Plan cancelled.".bright_black());
                return;
            }

            println!("{}", "Executing plan...".green());
            let config = ExecutionConfig {
                working_dir: state.current_dir.clone(),
                ..Default::default()
            };
            let executor = SafeExecutor::new(config);

            for step in &plan.steps {
                println!("\n▶ [{}/{}] {}", step.step_number, plan.steps.len(), step.description.bold());
                match executor.execute(&step.command).await {
                    Ok(res) => {
                        if !res.stdout.is_empty() {
                            print!("{}", res.stdout);
                        }
                        if !res.stderr.is_empty() {
                            eprint!("{}", res.stderr.red());
                        }
                        if let Some(code) = res.exit_code {
                            if code != 0 {
                                println!("{}", format!("Command exited with status {}", code).yellow());
                            }
                        }
                    }
                    Err(e) => {
                        eprintln!("Error executing step: {}", e);
                        break;
                    }
                }
            }

            println!("\n{}", "Plan execution complete.".green());
            state.journal.record(
                "agent_plan",
                prompt,
                &plan.summary,
                vec!["plan".to_string(), "autonomous".to_string()],
            );
        }
        Err(e) => {
            eprintln!("Failed to generate plan: {}", e);
        }
    }
}
