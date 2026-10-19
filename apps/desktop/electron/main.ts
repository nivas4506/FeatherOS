import { app, BrowserWindow, ipcMain } from "electron";
import { exec } from "node:child_process";
import { promises as fs } from "node:fs";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { promisify } from "node:util";

const currentFile = fileURLToPath(import.meta.url);
const currentDirectory = path.dirname(currentFile);
const execAsync = promisify(exec);

function resolveUserPath(value: string | undefined) {
  const home = os.homedir();
  const requested = value?.trim() || home;
  const resolved = path.resolve(requested.startsWith("~") ? path.join(home, requested.slice(1)) : requested);
  return resolved;
}

function createWindow() {
  const window = new BrowserWindow({
    width: 1440,
    height: 940,
    minWidth: 1100,
    minHeight: 720,
    backgroundColor: "#0b0d12",
    titleBarStyle: "hidden",
    trafficLightPosition: { x: 16, y: 16 },
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      preload: path.join(currentDirectory, "preload.cjs"),
    },
  });

  if (process.env.VITE_DEV_SERVER_URL) {
    void window.loadURL(process.env.VITE_DEV_SERVER_URL);
  } else {
    void window.loadFile(path.join(currentDirectory, "../dist/index.html"));
  }
}

app.whenReady().then(() => {
  ipcMain.handle("window:minimize", (event) => {
    BrowserWindow.fromWebContents(event.sender)?.minimize();
  });
  ipcMain.handle("window:toggle-maximize", (event) => {
    const window = BrowserWindow.fromWebContents(event.sender);
    if (window?.isMaximized()) window.unmaximize();
    else window?.maximize();
  });
  ipcMain.handle("window:close", (event) => {
    BrowserWindow.fromWebContents(event.sender)?.close();
  });
  ipcMain.handle("system:list-directory", async (_event, directory?: string) => {
    const resolved = resolveUserPath(directory);
    const entries = await fs.readdir(resolved, { withFileTypes: true });
    return entries
      .sort((a, b) => Number(b.isDirectory()) - Number(a.isDirectory()) || a.name.localeCompare(b.name))
      .map((entry) => ({ name: entry.name, path: path.join(resolved, entry.name), isDirectory: entry.isDirectory() }));
  });
  ipcMain.handle("system:make-directory", async (_event, directory: string) => {
    await fs.mkdir(resolveUserPath(directory), { recursive: true });
  });
  ipcMain.handle("system:remove-path", async (_event, target: string) => {
    const resolved = resolveUserPath(target);
    if (resolved === os.homedir() || resolved === path.parse(resolved).root) throw new Error("Refusing to remove a protected path");
    await fs.rm(resolved, { recursive: true, force: true });
  });
  ipcMain.handle("system:execute", async (_event, command: string, cwd?: string) => {
    const result = await execAsync(command, { cwd: resolveUserPath(cwd), windowsHide: true, timeout: 30_000, maxBuffer: 1024 * 1024 });
    return { stdout: result.stdout, stderr: result.stderr };
  });
  ipcMain.handle("system:stats", () => {
    const totalMemory = os.totalmem();
    return {
      cpuCount: os.cpus().length,
      memoryTotal: totalMemory,
      memoryFree: os.freemem(),
      uptime: os.uptime(),
      platform: process.platform,
      hostname: os.hostname(),
    };
  });
  ipcMain.handle("system:power", (_event, action: "shutdown" | "restart" | "logout") => {
    if (action === "logout") return app.quit();
    if (process.platform === "win32") {
      const command = action === "shutdown" ? "shutdown /s /t 0" : "shutdown /r /t 0";
      exec(command);
      return;
    }
    exec(action === "shutdown" ? "shutdown -h now" : "shutdown -r now");
  });

  createWindow();
  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});
