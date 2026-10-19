const { contextBridge, ipcRenderer } = require("electron");

contextBridge.exposeInMainWorld("featherDesktop", {
  minimize: () => ipcRenderer.invoke("window:minimize"),
  toggleMaximize: () => ipcRenderer.invoke("window:toggle-maximize"),
  close: () => ipcRenderer.invoke("window:close"),
  listDirectory: (directory) => ipcRenderer.invoke("system:list-directory", directory),
  makeDirectory: (directory) => ipcRenderer.invoke("system:make-directory", directory),
  removePath: (target) => ipcRenderer.invoke("system:remove-path", target),
  execute: (command, cwd) => ipcRenderer.invoke("system:execute", command, cwd),
  stats: () => ipcRenderer.invoke("system:stats"),
  power: (action) => ipcRenderer.invoke("system:power", action),
});
