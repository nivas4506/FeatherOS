use crate::tools::{FsFindTool, FsReadTool, ProcListTool, SystemTool};
use anyhow::{bail, Result};
use serde_json::{json, Value};
use std::collections::HashMap;
use std::sync::Arc;

#[derive(Clone)]
pub struct ToolBus {
    tools: HashMap<String, Arc<dyn SystemTool>>,
}

impl Default for ToolBus {
    fn default() -> Self {
        Self::with_defaults()
    }
}

impl ToolBus {
    pub fn new() -> Self {
        Self {
            tools: HashMap::new(),
        }
    }

    pub fn with_defaults() -> Self {
        let mut bus = Self::new();
        bus.register(Arc::new(FsFindTool));
        bus.register(Arc::new(FsReadTool));
        bus.register(Arc::new(ProcListTool));
        bus
    }

    pub fn register(&mut self, tool: Arc<dyn SystemTool>) {
        self.tools.insert(tool.name().to_string(), tool);
    }

    pub fn list_tools(&self) -> Vec<Value> {
        self.tools
            .values()
            .map(|t| {
                json!({
                    "name": t.name(),
                    "description": t.description(),
                    "inputSchema": t.parameters_schema(),
                })
            })
            .collect()
    }

    pub async fn dispatch(&self, name: &str, args: Value) -> Result<Value> {
        if let Some(tool) = self.tools.get(name) {
            tool.execute(args).await
        } else {
            bail!("Tool '{}' not found in Agent Bus registry", name);
        }
    }
}
