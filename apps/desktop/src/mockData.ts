export const activity = [
  { time: "09:42", title: "Workspace indexed", detail: "1,284 files · 4.2s", tone: "mint" },
  { time: "09:38", title: "Snapshot created", detail: "before command.execute", tone: "violet" },
  { time: "09:31", title: "Memory journal synced", detail: "12 new entries", tone: "blue" },
  { time: "09:18", title: "Aegis policy updated", detail: "No action required", tone: "amber" },
];

export const files = [
  { name: "kernel", type: "folder", meta: "Modified 12m ago", color: "violet" },
  { name: "os", type: "folder", meta: "Modified 28m ago", color: "blue" },
  { name: "README.md", type: "markdown", meta: "18.4 KB", color: "mint" },
  { name: "Cargo.toml", type: "config", meta: "1.2 KB", color: "amber" },
];

export const tools = [
  { name: "command.execute", group: "System", status: "Ready", icon: "terminal" as const },
  { name: "memory.search", group: "Cognition", status: "Ready", icon: "brain" as const },
  { name: "snapshot.rollback", group: "Safety", status: "Guarded", icon: "shield" as const },
  { name: "workspace.index", group: "System", status: "Ready", icon: "folder" as const },
];
