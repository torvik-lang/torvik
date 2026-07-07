<#
  install.ps1 - Torvik installer for Windows (x86-64).

  One-line install (from PowerShell):
    iwr -useb https://raw.githubusercontent.com/torvik-lang/torvik/main/windows/install.ps1 | iex

  Installs torvc.exe and rune.exe under %USERPROFILE%\.torvik, downloads the
  bundled runtime + standard library, writes a default config, and adds
  %USERPROFILE%\.torvik\bin to your user PATH. Mirrors the Linux install.sh.

  Requires: LLVM/clang on PATH (torvc calls clang to link the final .exe).
    winget install LLVM.LLVM     — or grab it from https://releases.llvm.org
#>

$ErrorActionPreference = 'Stop'

$Org = 'https://github.com/torvik-lang/torvik'
$Raw = 'https://raw.githubusercontent.com/torvik-lang/torvik/main'

$InstallDir = Join-Path $env:USERPROFILE '.torvik'
$BinDir     = Join-Path $InstallDir 'bin'
$LibDir     = Join-Path $InstallDir 'lib'

function Fetch([string]$Url) { (Invoke-WebRequest -UseBasicParsing -Uri $Url).Content }
function Download([string]$Url, [string]$Dest) {
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Dest
}

Write-Host 'Torvik installer (Windows)'

# --- clang + toolchain check ------------------------------------------------
# torvc needs clang AND a MinGW-w64 sysroot (headers + libs). Plain LLVM alone
# does not include the C headers, so we do a real test compile of a tiny program
# with the GNU target — the same target torvc uses — to catch a broken toolchain
# now, with a clear fix, rather than at first build.
$clang = Get-Command clang -ErrorAction SilentlyContinue
if (-not $clang) {
    Write-Host ''
    Write-Host 'error: clang is required (torvc uses it to compile and link).' -ForegroundColor Red
    Write-Host '  Install a clang that includes the MinGW-w64 headers/libraries — pick one:'
    Write-Host '    - LLVM-MinGW:  https://github.com/mstorsjo/llvm-mingw/releases  (simplest, self-contained)'
    Write-Host '    - MSYS2:       install msys2, then: pacman -S mingw-w64-clang-x86_64-toolchain'
    Write-Host '    - WinLibs:     https://winlibs.com  (UCRT build)'
    Write-Host '  Add the toolchain''s bin directory to PATH, then re-run this installer.'
    exit 1
}
# Functional check: can clang find the C headers for the GNU target?
$probe = Join-Path $env:TEMP 'torvik_probe.c'
$probeObj = Join-Path $env:TEMP 'torvik_probe.o'
'#include <stdio.h>' | Set-Content -Encoding ASCII $probe
'int main(void){return 0;}' | Add-Content -Encoding ASCII $probe
$null = & clang -target x86_64-w64-windows-gnu -c $probe -o $probeObj 2>&1
$toolchainOk = ($LASTEXITCODE -eq 0)
Remove-Item -Force -ErrorAction SilentlyContinue $probe, $probeObj
if (-not $toolchainOk) {
    Write-Host ''
    Write-Host 'warning: clang is installed but cannot find the C headers (e.g. stdio.h) for the' -ForegroundColor Yellow
    Write-Host '         MinGW-w64 target. Torvik will install, but `rune build` will fail until you'
    Write-Host '         have a clang with the MinGW-w64 sysroot. Recommended:'
    Write-Host '           - LLVM-MinGW:  https://github.com/mstorsjo/llvm-mingw/releases'
    Write-Host '           - MSYS2:       pacman -S mingw-w64-clang-x86_64-toolchain'
    Write-Host '           - WinLibs:     https://winlibs.com  (UCRT build)'
    Write-Host '         Put its bin directory first on PATH so clang finds its own headers.'
    Write-Host ''
}

# --- resolve version --------------------------------------------------------
# Default: latest, from the repo's VERSION on main. Override: set the
# TORVIK_VERSION environment variable to pin a release — a full tag (v1.0.1) or a
# partial version (v1, v1.0) that resolves to the newest matching release via the
# GitHub API. `rune update vX` sets this for you.
$Arch = 'x86_64'
$OS   = 'windows'
$Api  = 'https://api.github.com/repos/torvik-lang/torvik/releases'

function Resolve-Tag([string]$req) {
    try { $rel = Invoke-RestMethod -UseBasicParsing -Uri $Api -Headers @{ 'User-Agent' = 'torvik-installer' } }
    catch { return '__UNREACHABLE__' }
    $tags = @($rel | ForEach-Object { $_.tag_name })
    # keep exact match or "req." prefix (so v1 matches v1.2.3, not v10.0.0)
    $match = @($tags | Where-Object { $_ -eq $req -or $_.StartsWith("$req.") })
    if ($match.Count -eq 0) { return $null }
    # highest by version sort (force array so single matches index correctly)
    $sorted = @($match | Sort-Object { [version]($_ -replace '^v','') })
    return $sorted[$sorted.Count - 1]
}

$V = ''
if ($env:TORVIK_VERSION) {
    $req = $env:TORVIK_VERSION
    if ($req -notmatch '^v') { $req = "v$req" }
    $tag = Resolve-Tag $req
    if ($tag -eq '__UNREACHABLE__') {
        Write-Host 'error: could not reach the GitHub release list (network issue or API rate limit).' -ForegroundColor Red
        Write-Host '  Check your connection and try again, or install the latest with a bare ''rune update''.'
        exit 1
    }
    if (-not $tag) {
        Write-Host "error: no Torvik release matches '$($env:TORVIK_VERSION)'." -ForegroundColor Red
        Write-Host '  See https://github.com/torvik-lang/torvik/releases'
        exit 1
    }
    $V = $tag -replace '^v',''
    Write-Host "Installing the requested Torvik v$V ($OS/$Arch)..."
} else {
    try {
        $verText = Fetch "$Raw/VERSION"
        foreach ($line in ($verText -split "`n")) {
            if ($line -match '^\s*torvik\s*=\s*(.+?)\s*$') { $V = $Matches[1].Trim(); break }
        }
    } catch { }
    if (-not $V) { $V = '1.1.0' }
    Write-Host "Installing Torvik v$V ($OS/$Arch)..."
}
$Rel = "$Org/releases/download/v$V"

# --- create layout ----------------------------------------------------------
New-Item -ItemType Directory -Force -Path $BinDir,$LibDir,
    (Join-Path $InstallDir 'cache'),(Join-Path $InstallDir 'runes') | Out-Null

# --- download the binaries (fail cleanly if this version has no Windows build) -
try {
    Download "$Rel/torvc-$OS-$Arch.exe" (Join-Path $BinDir 'torvc.exe')
    Download "$Rel/rune-$OS-$Arch.exe"  (Join-Path $BinDir 'rune.exe')
} catch {
    Write-Host "error: Torvik v$V has no $OS/$Arch build (download failed)." -ForegroundColor Red
    Write-Host '  Pick another version from https://github.com/torvik-lang/torvik/releases'
    exit 1
}

# --- runtime + standard library --------------------------------------------
Download "$Raw/runtime/torvik_runtime.c" (Join-Path $LibDir 'torvik_runtime.c')
Download "$Raw/VERSION"                   (Join-Path $LibDir 'VERSION')
$stdDir = Join-Path $LibDir 'std'
New-Item -ItemType Directory -Force -Path $stdDir | Out-Null
foreach ($a in @('math','strings','list')) {
    try { Download "$Raw/src/std/$a.tv" (Join-Path $stdDir "$a.tv") } catch { }
}

# --- default config ---------------------------------------------------------
$cfg = Join-Path $InstallDir 'config.rune'
if (-not (Test-Path $cfg)) {
@'
# %USERPROFILE%\.torvik\config.rune - Torvik user configuration
cache = true
diagnostics = false
'@ | Set-Content -Encoding UTF8 $cfg
}

# --- PATH (user scope) ------------------------------------------------------
$userPath = [Environment]::GetEnvironmentVariable('Path','User')
if ($userPath -notlike "*$BinDir*") {
    $newPath = if ($userPath) { "$userPath;$BinDir" } else { $BinDir }
    [Environment]::SetEnvironmentVariable('Path', $newPath, 'User')
    Write-Host "Added $BinDir to your user PATH."
} else {
    Write-Host "$BinDir already on PATH."
}

Write-Host ''
Write-Host "Torvik v$V installed."
Write-Host '>>> Open a NEW terminal (so PATH refreshes), then:  rune --version'
Write-Host '    From here on, `rune update` upgrades Torvik and `rune uninstall` removes it.'
