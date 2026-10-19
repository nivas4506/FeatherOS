import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("featherDesktop", {
  minimize: () => ipcRenderer.invoke("window:minimize"),
  toggleMaximize: () => ipcRenderer.invoke("window:toggle-maximize"),
  close: () => ipcRenderer.invoke("window:close"),
  listDirectory: (directory?: string) => ipcRenderer.invoke("system:list-directory", directory),
  makeDirectory: (directory: string) => ipcRenderer.invoke("system:make-directory", directory),
  removePath: (target: string) => ipcRenderer.invoke("system:remove-path", target),
  execute: (command: string, cwd?: string) => ipcRenderer.invoke("system:execute", command, cwd),
  stats: () => ipcRenderer.invoke("system:stats"),
  power: (action: "shutdown" | "restart" | "logout") => ipcRenderer.invoke("system:power", action),
});
