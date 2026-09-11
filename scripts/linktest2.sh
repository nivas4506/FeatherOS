#!/usr/bin/env bash
# Force a real link of #[link(name="windows.0.52.0")] via zig-link-win (no -Bdynamic).
set -u
cd "$(dirname "$0")"
LIBDIR="$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/windows_x86_64_gnu-0.52.6/lib"
cat > linkbin.rs <<'EOF'
#[link(name = "windows.0.52.0")]
extern "C" {
    fn GetTickCount() -> u32;
}
fn main() {
    let t = unsafe { GetTickCount() };
    std::process::exit(t as i32);
}
EOF
rustc --target x86_64-pc-windows-gnu --crate-type bin linkbin.rs \
  -C linker=zig-link-win -L "native=$LIBDIR" -o linkbin.exe 2>&1 | head -n 12
echo "rustc-bin exit=$?"
ls -la linkbin.exe 2>/dev/null
