mod shell;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    shell::run().await
}
