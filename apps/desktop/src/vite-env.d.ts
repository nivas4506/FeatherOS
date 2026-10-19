interface Window {
  featherDesktop?: {
    minimize: () => Promise<void>;
    toggleMaximize: () => Promise<void>;
    close: () => Promise<void>;
    listDirectory: (directory?: string) => Promise<{ name: string; path: string; isDirectory: boolean }[]>;
    makeDirectory: (directory: string) => Promise<void>;
    removePath: (target: string) => Promise<void>;
    execute: (command: string, cwd?: string) => Promise<{ stdout: string; stderr: string }>;
    stats: () => Promise<{ cpuCount: number; memoryTotal: number; memoryFree: number; uptime: number; platform: string; hostname: string }>;
    power: (action: "shutdown" | "restart" | "logout") => Promise<void>;
  };
}
