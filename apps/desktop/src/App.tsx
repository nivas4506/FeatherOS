import { useEffect, useMemo, useState } from "react";

type AppId = "files" | "terminal" | "monitor" | "settings";
type WindowPosition = { top: number; left: number };
type DirectoryEntry = { name: string; path: string; isDirectory: boolean };
type SystemStats = { cpuCount: number; memoryTotal: number; memoryFree: number; uptime: number; platform: string; hostname: string };

type IconName =
  | "folder"
  | "terminal"
  | "monitor"
  | "settings"
  | "search"
  | "wifi"
  | "volume"
  | "battery"
  | "power"
  | "grid"
  | "close"
  | "minus"
  | "maximize"
  | "chevron";

const apps: { id: AppId; label: string; icon: IconName; color: string }[] = [
  { id: "files", label: "Files", icon: "folder", color: "blue" },
  { id: "terminal", label: "Terminal", icon: "terminal", color: "violet" },
  { id: "monitor", label: "System Monitor", icon: "monitor", color: "mint" },
  { id: "settings", label: "Settings", icon: "settings", color: "slate" },
];

function Icon({ name, size = 18 }: { name: IconName; size?: number }) {
  const paths: Record<IconName, string> = {
    folder: "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z",
    terminal: "m5 7 5 5-5 5m8 0h6",
    monitor: "M4 5h16v11H4zM9 20h6m-8-4h10",
    settings: "M12 15.5a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7Zm8-3.5 2-1.1-2-3.4-2.1.8a8 8 0 0 0-1.7-1L16 5h-4l-.3 2.3a8 8 0 0 0-1.7 1L8 7.5l-2 3.4L8 12a7 7 0 0 0 0 2l-2 1.1 2 3.4 2.1-.8a8 8 0 0 0 1.7 1L12 21h4l.3-2.3a8 8 0 0 0 1.7-1l2.1.8 2-3.4-2-1.1a7 7 0 0 0 0-2Z",
    search: "m20 20-4.3-4.3M10.8 18a7.2 7.2 0 1 1 0-14.4 7.2 7.2 0 0 1 0 14.4Z",
    wifi: "M3 8.5a14 14 0 0 1 18 0M6.5 12a8.5 8.5 0 0 1 11 0M10 15.5a3.5 3.5 0 0 1 4 0M12 19h.01",
    volume: "M4 10v4h4l5 4V6l-5 4zm12-1a5 5 0 0 1 0 6m2-8a9 9 0 0 1 0 10",
    battery: "M4 8h15v8H4zM21 11v2",
    power: "M12 3v9m5.7-6.1a8 8 0 1 1-11.4 0",
    grid: "M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 14h6v6h-6z",
    close: "m6 6 12 12M18 6 6 18",
    minus: "M5 12h14",
    maximize: "M6 6h12v12H6z",
    chevron: "m7 10 5 5 5-5",
  };

  return (
    <svg className="icon" width={size} height={size} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
      <path d={paths[name]} />
    </svg>
  );
}

function AppWindow({ id, position, zIndex, onClose, onMinimize, onActivate, onMove }: { id: AppId; position: WindowPosition; zIndex: number; onClose: () => void; onMinimize: () => void; onActivate: () => void; onMove: (position: WindowPosition) => void }) {
  const meta = apps.find((app) => app.id === id)!;
  const [dragging, setDragging] = useState(false);
  const [dragStart, setDragStart] = useState({ x: 0, y: 0, top: 0, left: 0 });
  const beginDrag = (event: React.PointerEvent<HTMLElement>) => {
    if ((event.target as HTMLElement).closest("button")) return;
    onActivate();
    setDragging(true);
    setDragStart({ x: event.clientX, y: event.clientY, top: position.top, left: position.left });
    event.currentTarget.setPointerCapture(event.pointerId);
  };
  const drag = (event: React.PointerEvent<HTMLElement>) => {
    if (dragging) onMove({ top: Math.max(45, dragStart.top + event.clientY - dragStart.y), left: Math.max(8, dragStart.left + event.clientX - dragStart.x) });
  };
  return (
    <section className={`desktop-window window-${id}`} style={{ top: position.top, left: position.left, right: "auto", zIndex }} onMouseDown={onActivate}>
      <header className="window-titlebar" onPointerDown={beginDrag} onPointerMove={drag} onPointerUp={() => setDragging(false)}>
        <div className={`app-window-icon ${meta.color}`}><Icon name={meta.icon} size={15} /></div>
        <strong>{meta.label}</strong>
        <div className="window-controls">
          <button aria-label="Minimize" onClick={onMinimize}><Icon name="minus" size={14} /></button>
          <button aria-label="Maximize"><Icon name="maximize" size={13} /></button>
          <button aria-label="Close" onClick={onClose}><Icon name="close" size={14} /></button>
        </div>
      </header>
      {id === "files" && <FilesContent />}
      {id === "terminal" && <TerminalContent />}
      {id === "monitor" && <MonitorContent />}
      {id === "settings" && <SettingsContent />}
    </section>
  );
}

function FilesContent() {
  const [directory, setDirectory] = useState("~");
  const [entries, setEntries] = useState<DirectoryEntry[]>([]);
  const [error, setError] = useState("");
  const refresh = () => void window.featherDesktop?.listDirectory(directory).then(setEntries).catch((reason: Error) => setError(reason.message));
  useEffect(refresh, [directory]);
  const createFolder = async () => {
    const name = window.prompt("New folder name");
    if (!name?.trim()) return;
    await window.featherDesktop?.makeDirectory(`${directory}/${name.trim()}`);
    refresh();
  };
  const removeEntry = async (entry: DirectoryEntry) => {
    if (!window.confirm(`Move ${entry.name} to trash?`)) return;
    await window.featherDesktop?.removePath(entry.path);
    refresh();
  };
  return <div className="window-content files-content"><aside className="file-sidebar"><strong>Places</strong><span className="selected" onClick={() => setDirectory("~")}><Icon name="folder" size={15} /> Home</span><span><Icon name="folder" size={15} /> Desktop</span><span><Icon name="folder" size={15} /> Documents</span><span><Icon name="folder" size={15} /> Downloads</span><hr /><strong>Devices</strong><span><Icon name="battery" size={15} /> Feather Disk</span></aside><div className="file-main"><div className="file-toolbar"><button onClick={() => setDirectory("~")}><Icon name="chevron" size={15} /></button><span>{directory}</span><button onClick={createFolder}>+ Folder</button><button className="toolbar-search" onClick={refresh}><Icon name="search" size={14} /></button></div>{error ? <p className="window-error">{error}</p> : <div className="file-grid">{entries.map((entry) => <FileItem key={entry.path} icon={entry.isDirectory ? "folder" : "terminal"} label={entry.name} onOpen={() => entry.isDirectory ? setDirectory(entry.path) : undefined} onRemove={() => void removeEntry(entry)} />)}</div>}</div></div>;
}

function FileItem({ icon, label, onOpen, onRemove }: { icon: IconName; label: string; onOpen: () => void; onRemove: () => void }) {
  return <button className="file-item" onDoubleClick={onOpen} onContextMenu={(event) => { event.preventDefault(); void onRemove(); }}><div className="file-item-icon"><Icon name={icon} size={28} /></div><span>{label}</span></button>;
}

function TerminalContent() {
  const [command, setCommand] = useState("");
  const [output, setOutput] = useState("FeatherOS terminal connected. Type a command to execute it through the desktop bridge.");
  const run = async () => {
    if (!command.trim()) return;
    try {
      const result = await window.featherDesktop?.execute(command);
      setOutput(`${output}\n$ ${command}\n${result?.stdout || result?.stderr || "(completed)"}`);
    } catch (reason) {
      setOutput(`${output}\n$ ${command}\n${reason instanceof Error ? reason.message : String(reason)}`);
    }
    setCommand("");
  };
  return <div className="terminal-content"><pre>{output}</pre><div><span className="terminal-green">nivas@feather</span><span className="terminal-muted">:</span><span className="terminal-blue">~</span><span className="terminal-muted">$</span> <input className="terminal-input" autoFocus value={command} onChange={(event) => setCommand(event.target.value)} onKeyDown={(event) => { if (event.key === "Enter") void run(); }} /><span className="cursor" /></div></div>;
}

function MonitorContent() {
  const [stats, setStats] = useState<SystemStats>();
  useEffect(() => { const update = () => void window.featherDesktop?.stats().then(setStats); update(); const timer = window.setInterval(update, 3000); return () => window.clearInterval(timer); }, []);
  const memoryUsed = stats ? Math.round((1 - stats.memoryFree / stats.memoryTotal) * 100) : 0;
  return <div className="window-content monitor-content"><div className="monitor-summary"><div><span>CPU cores</span><strong>{stats?.cpuCount ?? "—"}</strong></div><div><span>Memory used</span><strong>{stats ? `${memoryUsed}%` : "—"}</strong></div><div><span>Uptime</span><strong>{stats ? `${Math.floor(stats.uptime / 3600)}h` : "—"}</strong></div></div><div className="usage-chart"><svg viewBox="0 0 600 170" preserveAspectRatio="none"><path d="M0 125 C60 110 80 140 130 90 S190 125 240 80 S300 110 350 70 S420 110 470 60 S540 95 600 38" fill="none" stroke="#72d8c0" strokeWidth="3" /></svg></div><div className="process-row"><span>featherd</span><span>{stats?.hostname ?? "System daemon"}</span><b>online</b></div><div className="process-row"><span>desktop-shell</span><span>{stats?.platform ?? "Electron UI"}</span><b>ready</b></div></div>;
}

function SettingsContent() {
  return <div className="window-content settings-content"><div className="settings-card"><div className="settings-symbol"><Icon name="settings" size={28} /></div><div><h3>FeatherOS Settings</h3><p>Personalize your desktop environment.</p></div></div><label className="setting-row"><span>Dark appearance</span><input type="checkbox" defaultChecked /></label><label className="setting-row"><span>Show system telemetry</span><input type="checkbox" defaultChecked /></label><label className="setting-row"><span>Launch Feather daemon on startup</span><input type="checkbox" defaultChecked /></label></div>;
}

function App() {
  const [openApps, setOpenApps] = useState<AppId[]>(["files"]);
  const [startOpen, setStartOpen] = useState(false);
  const [quickPanelOpen, setQuickPanelOpen] = useState(false);
  const [calendarOpen, setCalendarOpen] = useState(false);
  const [powerOpen, setPowerOpen] = useState(false);
  const [notifications, setNotifications] = useState<string[]>([]);
  const [minimizedApps, setMinimizedApps] = useState<AppId[]>([]);
  const [positions, setPositions] = useState<Record<AppId, WindowPosition>>({ files: { top: 92, left: 160 }, terminal: { top: 150, left: 420 }, monitor: { top: 110, left: 720 }, settings: { top: 135, left: 610 } });
  const [workspace, setWorkspace] = useState(1);
  const [time, setTime] = useState(new Date());
  const [search, setSearch] = useState("");
  const [activeApp, setActiveApp] = useState<AppId>("files");
  useEffect(() => {
    const timer = window.setInterval(() => setTime(new Date()), 1000);
    return () => window.clearInterval(timer);
  }, []);
  useEffect(() => {
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.altKey && event.key === "Tab") {
        event.preventDefault();
        const next = apps.find((app) => app.id !== activeApp && openApps.includes(app.id));
        if (next) setActiveApp(next.id);
      }
      if (event.key === "Escape") { setStartOpen(false); setQuickPanelOpen(false); setCalendarOpen(false); setPowerOpen(false); }
      if (event.ctrlKey && event.key >= "1" && event.key <= "4") setWorkspace(Number(event.key));
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [activeApp, openApps]);

  const dateLabel = useMemo(() => time.toLocaleDateString(undefined, { weekday: "short", month: "short", day: "numeric" }), [time]);
  const timeLabel = useMemo(() => time.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" }), [time]);
  const launch = (id: AppId) => {
    setOpenApps((current) => current.includes(id) ? current : [...current, id]);
    setMinimizedApps((current) => current.filter((app) => app !== id));
    setActiveApp(id);
    setStartOpen(false);
  };
  const close = (id: AppId) => { setOpenApps((current) => current.filter((app) => app !== id)); setMinimizedApps((current) => current.filter((app) => app !== id)); };
  const move = (id: AppId, position: WindowPosition) => setPositions((current) => ({ ...current, [id]: position }));
  const notify = (message: string) => { setNotifications((current) => [...current, message]); window.setTimeout(() => setNotifications((current) => current.filter((item) => item !== message)), 3500); };

  return <div className="os-shell" onClick={() => { setStartOpen(false); setQuickPanelOpen(false); }}>
    <header className="system-bar" onClick={(event) => event.stopPropagation()}>
      <button className="system-brand" onClick={() => setStartOpen((open) => !open)}><span className="feather-mark">✦</span><strong>FeatherOS</strong></button>
      <nav className="system-menu"><button>Activities</button><button onClick={() => launch("files")}>Places</button><button onClick={() => launch("terminal")}>Terminal</button></nav>
      <div className="system-status"><button className="date-button" onClick={() => setCalendarOpen((open) => !open)}>{dateLabel}</button><span>{timeLabel}</span><button className="status-button" onClick={() => setQuickPanelOpen((open) => !open)}><Icon name="wifi" size={15} /><Icon name="volume" size={15} /><Icon name="battery" size={16} /><Icon name="chevron" size={12} /></button><button className="power-button" onClick={() => setPowerOpen((open) => !open)}><Icon name="power" size={15} /></button></div>
    </header>
    <main className="desktop-area" onDoubleClick={() => launch("files")}>
      <div className="desktop-icons" onClick={(event) => event.stopPropagation()}>
        <DesktopIcon icon="folder" label="Home" color="blue" onOpen={() => launch("files")} />
        <DesktopIcon icon="folder" label="File System" color="amber" onOpen={() => launch("files")} />
        <DesktopIcon icon="monitor" label="System Monitor" color="mint" onOpen={() => launch("monitor")} />
        <DesktopIcon icon="terminal" label="Terminal" color="violet" onOpen={() => launch("terminal")} />
      </div>
      {openApps.filter((id) => !minimizedApps.includes(id) && workspace === 1).map((id) => <AppWindow key={id} id={id} position={positions[id]} zIndex={activeApp === id ? 20 : 10} onActivate={() => setActiveApp(id)} onMove={(position) => move(id, position)} onClose={() => close(id)} onMinimize={() => setMinimizedApps((current) => [...current, id])} />)}
    </main>
    <footer className="dock" onClick={(event) => event.stopPropagation()}>{apps.map((app) => <button className={`dock-item ${activeApp === app.id && openApps.includes(app.id) ? "active" : ""}`} key={app.id} onClick={() => launch(app.id)}><span className={`dock-icon ${app.color}`}><Icon name={app.icon} size={22} /></span><span className="dock-label">{app.label}</span></button>)}<span className="dock-divider" /><button className="dock-item" onClick={() => setStartOpen((open) => !open)}><span className="dock-icon power"><Icon name="grid" size={21} /></span><span className="dock-label">Applications</span></button></footer>
    {startOpen && <div className="start-menu" onClick={(event) => event.stopPropagation()}><div className="start-header"><div className="user-avatar">N</div><div><strong>Nivas</strong><span>FeatherOS desktop · Workspace {workspace}</span></div><button onClick={() => setPowerOpen(true)}><Icon name="power" size={17} /></button></div><div className="start-search"><Icon name="search" size={15} /><input value={search} onChange={(event) => setSearch(event.target.value)} placeholder="Search applications" autoFocus /></div><div className="start-grid">{apps.filter((app) => app.label.toLowerCase().includes(search.toLowerCase())).map((app) => <button key={app.id} onClick={() => launch(app.id)}><span className={`start-app-icon ${app.color}`}><Icon name={app.icon} size={21} /></span><span>{app.label}</span></button>)}</div><div className="start-footer"><button onClick={() => launch("settings")}><Icon name="settings" size={14} /> Settings</button><button onClick={() => setPowerOpen(true)}><Icon name="power" size={14} /> Power</button></div></div>}
    {quickPanelOpen && <aside className="quick-panel" onClick={(event) => event.stopPropagation()}><div className="quick-heading"><strong>Quick settings</strong><button onClick={() => launch("settings")}><Icon name="settings" size={16} /></button></div><div className="quick-tile active"><Icon name="wifi" /><span><strong>Wi-Fi</strong><small>FeatherNet</small></span></div><div className="quick-tile"><Icon name="volume" /><span><strong>Sound</strong><small>Built-in Audio</small></span></div><div className="brightness"><span>Brightness</span><input type="range" defaultValue="80" /></div><div className="quick-footer"><span>FeatherOS 0.2.0</span><span>Connected</span></div></aside>}
    {calendarOpen && <aside className="calendar-panel" onClick={(event) => event.stopPropagation()}><strong>{time.toLocaleDateString(undefined, { month: "long", year: "numeric" })}</strong><div className="calendar-date">{time.getDate()}</div><span>{time.toLocaleDateString(undefined, { weekday: "long" })}</span><small>No events today</small></aside>}
    {powerOpen && <div className="power-overlay" onClick={() => setPowerOpen(false)}><div className="power-menu" onClick={(event) => event.stopPropagation()}><strong>Power options</strong><button onClick={() => void window.featherDesktop?.power("logout")}><Icon name="power" /> Log out</button><button onClick={() => void window.featherDesktop?.power("restart")}><Icon name="monitor" /> Restart</button><button onClick={() => void window.featherDesktop?.power("shutdown")}><Icon name="close" /> Shut down</button></div></div>}
    <div className="workspace-switcher">{[1, 2, 3, 4].map((item) => <button key={item} className={workspace === item ? "selected" : ""} onClick={() => setWorkspace(item)}>{item}</button>)}</div>
    <div className="notifications">{notifications.map((message) => <div key={message}><span className="live-dot" />{message}</div>)}</div>
  </div>;
}

function DesktopIcon({ icon, label, color, onOpen }: { icon: IconName; label: string; color: string; onOpen: () => void }) {
  return <button className="desktop-icon" onDoubleClick={onOpen}><span className={`desktop-icon-art ${color}`}><Icon name={icon} size={28} /></span><span>{label}</span></button>;
}

export default App;
