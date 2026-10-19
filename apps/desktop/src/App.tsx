import { useEffect, useMemo, useState } from "react";
import { activity, files, tools } from "./mockData";
import type { IconName, View } from "./types";

const navItems: { label: View; icon: IconName; badge?: string }[] = [
  { label: "Overview", icon: "grid" },
  { label: "Workspace", icon: "folder" },
  { label: "Memory", icon: "brain", badge: "12" },
  { label: "Tools", icon: "spark" },
  { label: "Activity", icon: "pulse" },
];

function Icon({ name, size = 18 }: { name: IconName; size?: number }) {
  const paths: Record<IconName, string> = {
    grid: "M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 14h6v6h-6z",
    folder: "M3 6.5A2.5 2.5 0 0 1 5.5 4H10l2 2h6.5A2.5 2.5 0 0 1 21 8.5v7a2.5 2.5 0 0 1-2.5 2.5h-13A2.5 2.5 0 0 1 3 16z",
    brain: "M9.5 4.5a3 3 0 0 0-5.5 1.7A3 3 0 0 0 5 12a3 3 0 0 0 2 5.5A3 3 0 0 0 13 16V8a3.5 3.5 0 0 0-3.5-3.5ZM14.5 4.5a3 3 0 0 1 5.5 1.7A3 3 0 0 1 19 12a3 3 0 0 1-2 5.5A3 3 0 0 1 11 16V8a3.5 3.5 0 0 1 3.5-3.5Z",
    spark: "m12 3 1.6 5.4L19 10l-5.4 1.6L12 17l-1.6-5.4L5 10l5.4-1.6zM19 16l.7 2.3L22 19l-2.3.7L19 22l-.7-2.3L16 19l2.3-.7z",
    pulse: "M3 12h4l2-6 4 12 2-6h6",
    settings: "M12 15.5a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7Zm8-3.5 2-1.1-2-3.4-2.1.8a8 8 0 0 0-1.7-1L16 5h-4l-.3 2.3a8 8 0 0 0-1.7 1L8 7.5l-2 3.4L8 12a7 7 0 0 0 0 2l-2 1.1 2 3.4 2.1-.8a8 8 0 0 0 1.7 1L12 21h4l.3-2.3a8 8 0 0 0 1.7-1l2.1.8 2-3.4-2-1.1a7 7 0 0 0 0-2Z",
    search: "m20 20-4.3-4.3M10.8 18a7.2 7.2 0 1 1 0-14.4 7.2 7.2 0 0 1 0 14.4Z",
    bell: "M18 9a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9ZM10 22h4",
    plus: "M12 5v14M5 12h14",
    arrow: "M5 12h13m-5-5 5 5-5 5",
    terminal: "m5 7 5 5-5 5m8 0h6",
    check: "m5 12 4 4L19 6",
    clock: "M12 7v5l3 2m6-2a9 9 0 1 1-18 0 9 9 0 0 1 18 0Z",
    shield: "M12 3 20 6v5c0 5-3.4 8.2-8 10-4.6-1.8-8-5-8-10V6z",
    cpu: "M9 9h6v6H9zM9 3v3m6-3v3m-6 12v3m6-3v3M3 9h3m-3 6h3m12-6h3m-3 6h3",
    memory: "M5 5h14v14H5zM8 8h8v8H8z",
    disk: "M4 5h16v14H4zM8 15h8M8 9h.01",
    chevron: "m7 10 5 5 5-5",
    command: "M5 4h14v16H5zM9 8h6M9 12h6M9 16h3",
    close: "m6 6 12 12M18 6 6 18",
    minus: "M5 12h14",
    maximize: "M6 6h12v12H6z",
  };

  return (
    <svg aria-hidden="true" className="icon" width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
      <path d={paths[name]} />
    </svg>
  );
}

function Metric({ icon, label, value, note, color }: { icon: IconName; label: string; value: string; note: string; color: string }) {
  return (
    <article className="metric-card">
      <div className={`metric-icon ${color}`}><Icon name={icon} /></div>
      <div className="metric-copy"><span>{label}</span><strong>{value}</strong><small>{note}</small></div>
      <span className={`metric-dot ${color}`} />
    </article>
  );
}

function App() {
  const [view, setView] = useState<View>("Overview");
  const [command, setCommand] = useState("");
  const [history, setHistory] = useState(["Connected to featherd", "Workspace session opened at ~/FeatherOS"]);
  const [paletteOpen, setPaletteOpen] = useState(false);
  const [notice, setNotice] = useState("System healthy");

  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "k") {
        event.preventDefault();
        setPaletteOpen((open) => !open);
      }
      if (event.key === "Escape") setPaletteOpen(false);
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, []);

  const greeting = useMemo(() => {
    const hour = new Date().getHours();
    return hour < 12 ? "Good morning" : hour < 18 ? "Good afternoon" : "Good evening";
  }, []);

  const runCommand = () => {
    const value = command.trim();
    if (!value) return;
    setHistory((items) => [...items, `$ ${value}`, "Queued through Aegis safety guard"]);
    setCommand("");
    setNotice("Command queued");
    window.setTimeout(() => setNotice("System healthy"), 2500);
  };

  const renderContent = () => {
    if (view === "Workspace") {
      return <section className="page-grid workspace-page"><div className="section-heading"><div><p className="eyebrow">Workspace explorer</p><h2>Your FeatherOS files</h2></div><button className="primary-button"><Icon name="plus" size={16} /> New workspace</button></div><div className="file-grid">{files.map((file) => <article className="file-card" key={file.name}><div className={`file-icon ${file.color}`}><Icon name={file.type === "folder" ? "folder" : "command"} size={20} /></div><strong>{file.name}</strong><span>{file.meta}</span><Icon name="chevron" size={16} /></article>)}</div></section>;
    }
    if (view === "Memory") {
      return <section className="page-grid"><div className="section-heading"><div><p className="eyebrow">Synapse journal</p><h2>Memory at a glance</h2></div><button className="ghost-button"><Icon name="search" size={16} /> Search memory</button></div><div className="memory-hero"><div className="memory-orb"><Icon name="brain" size={42} /></div><div><span className="status-label mint-text">INDEXED AND READY</span><h3>1,284 workspace memories</h3><p>Semantic context is available to Feather agents across this workspace.</p></div><strong className="memory-score">94<span>%</span></strong></div><div className="activity-list">{activity.map((item) => <div className="activity-row" key={item.time}><span>{item.time}</span><div className={`activity-line ${item.tone}`} /><div><strong>{item.title}</strong><small>{item.detail}</small></div></div>)}</div></section>;
    }
    if (view === "Tools") {
      return <section className="page-grid"><div className="section-heading"><div><p className="eyebrow">Agent bus</p><h2>Tools & capabilities</h2></div><span className="pill"><span className="live-dot" /> 4 online</span></div><div className="tool-table">{tools.map((tool) => <div className="tool-row" key={tool.name}><div className="tool-symbol"><Icon name={tool.icon} size={17} /></div><div><strong>{tool.name}</strong><small>{tool.group}</small></div><span className="tool-status">{tool.status}</span><button className="icon-button"><Icon name="arrow" size={16} /></button></div>)}</div></section>;
    }
    if (view === "Activity") {
      return <section className="page-grid"><div className="section-heading"><div><p className="eyebrow">Event stream</p><h2>Recent activity</h2></div><button className="ghost-button"><Icon name="pulse" size={16} /> Live stream</button></div><div className="activity-list activity-list-large">{activity.concat(activity).map((item, index) => <div className="activity-row" key={`${item.time}-${index}`}><span>{item.time}</span><div className={`activity-line ${item.tone}`} /><div><strong>{item.title}</strong><small>{item.detail}</small></div><span className="row-action"><Icon name="check" size={15} /></span></div>)}</div></section>;
    }
    return <Overview command={command} setCommand={setCommand} runCommand={runCommand} history={history} greeting={greeting} />;
  };

  return (
    <div className="app-shell">
      <header className="window-bar"><div className="brand"><div className="brand-mark"><span /></div><strong>FeatherOS</strong><span className="brand-divider" /><span className="brand-context">Desktop</span></div><div className="window-actions"><button onClick={() => void window.featherDesktop?.minimize()}><Icon name="minus" size={15} /></button><button onClick={() => void window.featherDesktop?.toggleMaximize()}><Icon name="maximize" size={14} /></button><button className="window-close" onClick={() => void window.featherDesktop?.close()}><Icon name="close" size={15} /></button></div></header>
      <div className="app-body">
        <aside className="sidebar"><div className="sidebar-top"><div className="workspace-switcher"><div className="workspace-avatar">F</div><div><strong>FeatherOS</strong><span>Local workspace</span></div><Icon name="chevron" size={15} /></div><button className="new-button"><Icon name="plus" size={17} /> New session <kbd>⌘ N</kbd></button></div><nav>{navItems.map((item) => <button className={view === item.label ? "nav-item active" : "nav-item"} onClick={() => setView(item.label)} key={item.label}><Icon name={item.icon} /><span>{item.label}</span>{item.badge && <em>{item.badge}</em>}</button>)}</nav><div className="sidebar-bottom"><div className="daemon-card"><span className="live-dot" /><div><strong>featherd online</strong><small>v0.2.0 · 24ms</small></div><Icon name="pulse" size={16} /></div><button className="nav-item"><Icon name="settings" /><span>Settings</span></button><div className="profile"><div className="profile-avatar">NA</div><div><strong>Nivas</strong><span>Personal account</span></div><Icon name="chevron" size={15} /></div></div></aside>
        <main className="main-content"><div className="topbar"><div className="breadcrumbs"><span>FeatherOS</span><Icon name="chevron" size={14} /><strong>{view}</strong></div><div className="topbar-actions"><button className="search-trigger" onClick={() => setPaletteOpen(true)}><Icon name="search" size={16} /><span>Search anything</span><kbd>⌘ K</kbd></button><button className="round-button"><Icon name="bell" size={17} /><i /></button><div className="top-avatar">NA</div></div></div><div className="content-scroll">{renderContent()}</div></main>
      </div>
      <div className={`toast ${notice === "System healthy" ? "" : "toast-visible"}`}><span className="live-dot" />{notice}</div>
      {paletteOpen && <div className="palette-backdrop" onClick={() => setPaletteOpen(false)}><div className="command-palette" onClick={(event) => event.stopPropagation()}><div className="palette-input"><Icon name="search" /><input autoFocus placeholder="Search commands, files, memory..." onKeyDown={(event) => { if (event.key === "Escape") setPaletteOpen(false); }} /></div><div className="palette-section"><span>Quick actions</span><button onClick={() => { setView("Overview"); setPaletteOpen(false); }}><Icon name="grid" /> Open overview <kbd>Enter</kbd></button><button onClick={() => { setView("Memory"); setPaletteOpen(false); }}><Icon name="brain" /> Search memory <kbd>M</kbd></button><button onClick={() => { setView("Tools"); setPaletteOpen(false); }}><Icon name="spark" /> Browse tools <kbd>T</kbd></button></div></div></div>}
    </div>
  );
}

function Overview({ command, setCommand, runCommand, history, greeting }: { command: string; setCommand: (value: string) => void; runCommand: () => void; history: string[]; greeting: string }) {
  return <section className="page-grid overview-page"><div className="hero-heading"><div><p className="eyebrow">Monday, October 19, 2026</p><h1>{greeting}, Nivas <span className="wave">✦</span></h1><p className="hero-subtitle">Your system is calm, connected, and ready to build.</p></div><button className="status-pill"><span className="live-dot" /> All systems operational <Icon name="chevron" size={14} /></button></div><div className="metrics-grid"><Metric icon="cpu" label="CPU load" value="18.4%" note="↓ 4.2% from yesterday" color="violet" /><Metric icon="memory" label="Memory" value="6.8 GB" note="of 16 GB available" color="blue" /><Metric icon="disk" label="Disk space" value="72%" note="184 GB remaining" color="amber" /><Metric icon="shield" label="Safety guard" value="Active" note="0 blocked actions" color="mint" /></div><div className="dashboard-grid"><div className="panel shell-panel"><div className="panel-heading"><div><span className="panel-kicker"><Icon name="terminal" size={14} /> Cognitive shell</span><h3>What would you like to do?</h3></div><span className="connection-label"><span className="live-dot" /> Connected</span></div><div className="command-box"><span className="command-prompt">›</span><input value={command} onChange={(event) => setCommand(event.target.value)} onKeyDown={(event) => { if (event.key === "Enter") runCommand(); }} placeholder="Ask Feather or run a command..." /><button onClick={runCommand}><Icon name="arrow" size={18} /></button></div><div className="suggestion-row"><button onClick={() => setCommand("check disk space and find largest folders")}><Icon name="disk" size={14} /> Check disk</button><button onClick={() => setCommand("show active tools")}><Icon name="spark" size={14} /> Show tools</button><button onClick={() => setCommand(":status")}><Icon name="pulse" size={14} /> System status</button></div><div className="shell-history">{history.slice(-3).map((line, index) => <div key={`${line}-${index}`}><span className={line.startsWith("$") ? "history-command" : "history-muted"}>{line}</span></div>)}</div></div><div className="panel plan-panel"><div className="panel-heading"><div><span className="panel-kicker"><Icon name="spark" size={14} /> Active intelligence</span><h3>Workspace pulse</h3></div><button className="icon-button"><Icon name="arrow" size={16} /></button></div><div className="pulse-chart"><div className="chart-glow" /><svg viewBox="0 0 420 150" preserveAspectRatio="none"><defs><linearGradient id="chart" x1="0" x2="0" y1="0" y2="1"><stop offset="0" stopColor="#a78bfa" stopOpacity=".3" /><stop offset="1" stopColor="#a78bfa" stopOpacity="0" /></linearGradient></defs><path d="M0 120 C32 106 42 112 69 88 S111 103 139 79 S170 34 199 69 S238 104 269 63 S302 81 326 48 S363 60 420 18 V150 H0Z" fill="url(#chart)" /><path d="M0 120 C32 106 42 112 69 88 S111 103 139 79 S170 34 199 69 S238 104 269 63 S302 81 326 48 S363 60 420 18" fill="none" stroke="#bba7ff" strokeWidth="2.5" /></svg><div className="chart-labels"><span>10:00</span><span>12:00</span><span>14:00</span><span>Now</span></div></div><div className="pulse-summary"><div><strong>12</strong><span>memory events</span></div><div><strong>8</strong><span>tool calls</span></div><div><strong>0</strong><span>blocked</span></div></div></div></div><div className="lower-grid"><div className="panel"><div className="panel-heading"><div><span className="panel-kicker"><Icon name="clock" size={14} /> Timeline</span><h3>Recent activity</h3></div><button className="text-button">View all <Icon name="arrow" size={14} /></button></div><div className="activity-list">{activity.map((item) => <div className="activity-row" key={item.time}><span>{item.time}</span><div className={`activity-line ${item.tone}`} /><div><strong>{item.title}</strong><small>{item.detail}</small></div></div>)}</div></div><div className="panel snapshot-card"><div className="snapshot-art"><div className="snapshot-ring" /><Icon name="shield" size={25} /></div><div><span className="panel-kicker">Latest snapshot</span><h3>Safe to experiment</h3><p>Workspace checkpoint created 4 minutes ago.</p><button className="text-button">Open snapshots <Icon name="arrow" size={14} /></button></div></div></div></section>;
}

export default App;
