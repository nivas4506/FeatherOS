use anyhow::Result;
use clap::Parser;
use cortex_core::{HeuristicProvider, LlmProvider, OllamaProvider};
use std::{path::PathBuf, sync::Arc};

#[derive(Parser)]
#[command(version, about = "FeatherOS background cognitive service")]
struct Args {
    #[arg(long, default_value_t = feather_ipc::default_endpoint())]
    endpoint: String,
    #[arg(long, default_value_os_t = feather_ipc::data_dir())]
    data_dir: PathBuf,
    #[arg(long, default_value = ".")]
    workspace: PathBuf,
    #[arg(long, default_value = "heuristic", value_parser = ["heuristic", "ollama"])]
    provider: String,
    #[arg(long, default_value = "http://localhost:11434")]
    ollama_endpoint: String,
    #[arg(long, default_value = "llama3.2:3b")]
    model: String,
}

#[tokio::main]
async fn main() -> Result<()> {
    let args = Args::parse();
    let provider: Arc<dyn LlmProvider> = match args.provider.as_str() {
        "ollama" => Arc::new(OllamaProvider::new(args.ollama_endpoint, args.model.clone())),
        _ => Arc::new(HeuristicProvider),
    };
    let model = if args.provider == "ollama" { args.model } else { "offline heuristics".into() };
    featherd::serve(&args.endpoint, args.data_dir, args.workspace, provider, model).await
}
