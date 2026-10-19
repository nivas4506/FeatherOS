param(
    [string]$BuildDirectory = "kernel/build-wsl"
)

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$drive = $repoRoot.Substring(0, 1).ToLowerInvariant()
$wslRoot = "/mnt/$drive/" + ($repoRoot.Substring(3) -replace "\\", "/")
$linuxBuild = "$wslRoot/$($BuildDirectory -replace '\\', '/')"
& wsl.exe bash -lc "set -e; cd '$wslRoot'; cmake -S kernel -B '$linuxBuild' -G Ninja; cmake --build '$linuxBuild' --target iso"
if ($LASTEXITCODE -ne 0) {
    throw "Native FeatherOS ISO build failed."
}

$iso = Join-Path $repoRoot "$BuildDirectory/featheros.iso"
if (-not (Test-Path $iso)) {
    throw "Build completed without producing $iso"
}
Write-Host "Native ISO ready: $iso"
