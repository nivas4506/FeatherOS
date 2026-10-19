use std::fs;
use std::path::Path;

pub struct DiffEngine;

impl DiffEngine {
    pub fn preview_change(path: &Path, new_content: &str) -> String {
        if !path.exists() {
            let mut out = format!("--- /dev/null\n+++ {}\n", path.display());
            for line in new_content.lines().take(30) {
                out.push_str(&format!("+ {}\n", line));
            }
            if new_content.lines().count() > 30 {
                out.push_str(&format!("... ({} more lines)\n", new_content.lines().count() - 30));
            }
            return out;
        }

        let old_content = fs::read_to_string(path).unwrap_or_default();
        let old_lines: Vec<&str> = old_content.lines().collect();
        let new_lines: Vec<&str> = new_content.lines().collect();

        let mut out = format!("--- {}\n+++ {}\n", path.display(), path.display());
        let mut changes = 0;

        let max_len = old_lines.len().max(new_lines.len());
        for i in 0..max_len {
            match (old_lines.get(i), new_lines.get(i)) {
                (Some(o), Some(n)) if o != n => {
                    out.push_str(&format!("- {}\n", o));
                    out.push_str(&format!("+ {}\n", n));
                    changes += 1;
                }
                (Some(o), None) => {
                    out.push_str(&format!("- {}\n", o));
                    changes += 1;
                }
                (None, Some(n)) => {
                    out.push_str(&format!("+ {}\n", n));
                    changes += 1;
                }
                _ => {}
            }
            if changes >= 25 {
                out.push_str("... (diff truncated)\n");
                break;
            }
        }

        if changes == 0 {
            out.push_str("(No textual content differences detected)\n");
        }

        out
    }
}
