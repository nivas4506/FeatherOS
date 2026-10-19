# Application Layer: User Space Applications (Layer 4)

The **Application Layer** sits on top of the FeatherOS OS Layer and provides human and agent interfaces to the system. Applications communicate with the background OS supervisor (`featherd`) via JSON-RPC 2.0 over standard IPC channels.

```
┌────────────────────────────────────────────────────────────────────────┐
│                      APPLICATION LAYER (Layer 4)                       │
│                                                                        │
│  ┌─────────────────────────┐  ┌─────────────────────────────────────┐  │
│  │   COGNITIVE SHELL       │  │       TERMINAL DASHBOARD (TUI)      │  │
│  │   apps/shell/aish       │  │       apps/tui/feather-tui          │  │
│  │   • Dual-Mode REPL      │  │       • Dual-Pane Visual Dashboard  │  │
│  │   • Instant POSIX Shell │  │       • Real-time Agent Inspector   │  │
│  │   • Natural Language AI │  │       • Telemetry & Memory Charts   │  │
│  └─────────────────────────┘  └─────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │   SYSTEM TOOLS & EXTENSIONS (apps/tools)                         │  │
│  │   • CLI Tools • Scripting Bridge • MCP Extensions                │  │
│  └──────────────────────────────────────────────────────────────────┘  │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ JSON-RPC 2.0 / IPC
┌───────────────────────────────────▼────────────────────────────────────┐
│                       OS LAYER (FeatherOS Substrate)                   │
└────────────────────────────────────────────────────────────────────────┘
```

## Applications

### 1. The Cognitive Shell (`apps/shell/aish`)
- **Binary**: `feather` (or `aish`)
- **Key Features**:
  - Dual-mode command input: Type standard commands (`ls`, `git diff`) or natural language intents (`"find all log files older than 7 days"`).
  - Proactive step verification and rollback preview.
  - Interactive auto-confirmation and execution control.

### 2. Dual-Pane Terminal Dashboard (`apps/tui/feather-tui`)
- **Binary**: `feather-tui`
- **Key Features**:
  - Built with `ratatui` and `crossterm`.
  - Left Pane: Interactive FeatherOS terminal session.
  - Right Pane: Cognitive inspector showing active plan steps, vector memory search, system telemetry gauges, and sandbox capability indicators.

### 3. Application Tools (`apps/tools`)
- User-space CLI helpers, scripts, and Model Context Protocol (MCP) tool plugins for specialized domain workflows.
