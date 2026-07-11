<#
  install.ps1 - Torvik installer for Windows (x86-64).

  One-line install (from PowerShell):
    iwr -useb https://raw.githubusercontent.com/torvik-lang/torvik/main/windows/install.ps1 | iex

  Installs torvc.exe and rune.exe under %USERPROFILE%\.torvik, downloads the
  bundled runtime + standard library, writes a default config, and adds
  %USERPROFILE%\.torvik\bin to your user PATH. Mirrors the Linux install.sh.

  Requires: LLVM/clang on PATH (torvc calls clang to link the final .exe).
    winget install LLVM.LLVM     - or grab it from https://releases.llvm.org
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
# with the GNU target - the same target torvc uses - to catch a broken toolchain
# now, with a clear fix, rather than at first build.
$clang = Get-Command clang -ErrorAction SilentlyContinue
if (-not $clang) {
    Write-Host ''
    Write-Host 'error: clang is required (torvc uses it to compile and link).' -ForegroundColor Red
    Write-Host '  Install a clang that includes the MinGW-w64 headers/libraries - pick one:'
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
# TORVIK_VERSION environment variable to pin a release - a full tag (v1.0.1) or a
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
# Raw-content base for VERSION / runtime / stdlib. When a specific version is
# installed we pull these from that version's TAG (not main), so the VERSION
# marker, runtime, and stdlib all match the binaries we just downloaded. A bare
# (latest) install tracks main.
$RawRef = "https://raw.githubusercontent.com/torvik-lang/torvik/v$V"
if (-not $env:TORVIK_VERSION) { $RawRef = $Raw }

# If an older install is present and this is a new MAJOR version, print a
# non-blocking heads-up. We don't prompt here: piping this script into `iex`
# consumes stdin, so an interactive prompt can't be answered reliably. The
# `rune update` command does the real confirm-before-major-bump; anyone who
# wants to stay on their current major line can install a pinned version
# (e.g. set TORVIK_VERSION=v1, or run `rune update v1`).
$ExistingVer = ''
$existingVerFile = Join-Path $LibDir 'VERSION'
if (Test-Path $existingVerFile) {
    foreach ($line in (Get-Content $existingVerFile)) {
        if ($line -match '^\s*torvik\s*=\s*(.+?)\s*$') { $ExistingVer = $Matches[1].Trim(); break }
    }
}
if ($ExistingVer) {
    $curMajor = ($ExistingVer -split '\.')[0]
    $newMajor = ($V -split '\.')[0]
    if ($newMajor -ne $curMajor) {
        Write-Host ''
        Write-Host "note: this replaces Torvik v$ExistingVer with a new MAJOR version (v$V)." -ForegroundColor Yellow
        Write-Host '      Major versions may include breaking changes - see the changelog:'
        Write-Host '      https://github.com/torvik-lang/torvik/blob/main/CHANGELOG.md'
        Write-Host "      To stay on v$curMajor.x instead, install a pinned version (e.g. rune update v$curMajor)."
        Write-Host ''
    }
}

# --- create layout ----------------------------------------------------------
New-Item -ItemType Directory -Force -Path $BinDir,$LibDir,
    (Join-Path $InstallDir 'cache'),(Join-Path $InstallDir 'runes') | Out-Null

# --- download the binaries --------------------------------------------------
# rune.exe may be the very process running this update (via `rune update`), and
# Windows won't let you overwrite a running executable - but it DOES allow
# renaming one out of the way. So we download to temp files first, then for each
# binary move any locked original aside (.old) before putting the new one in
# place. torvc is downloaded the same way for consistency. A genuinely missing
# build (e.g. a pinned version with no Windows asset) still fails clearly.
$torvcTmp = Join-Path $BinDir 'torvc.exe.new'
$runeTmp  = Join-Path $BinDir 'rune.exe.new'
try {
    Download "$Rel/torvc-$OS-$Arch.exe" $torvcTmp
    Download "$Rel/rune-$OS-$Arch.exe"  $runeTmp
} catch {
    Write-Host "error: could not download the Torvik v$V binaries for $OS/$Arch." -ForegroundColor Red
    Write-Host '  If you pinned a version, check it has a Windows build at'
    Write-Host '  https://github.com/torvik-lang/torvik/releases'
    Remove-Item -Force -ErrorAction SilentlyContinue $torvcTmp, $runeTmp
    exit 1
}

function Install-Binary([string]$New, [string]$Final) {
    $old = "$Final.old"
    Remove-Item -Force -ErrorAction SilentlyContinue $old
    if (Test-Path $Final) {
        try {
            # Overwrite in place if we can (not running)...
            Move-Item -Force $New $Final -ErrorAction Stop
            return
        } catch {
            # ...otherwise the target is locked (running): rename it aside, which
            # Windows permits even for a running exe, then drop the new one in.
            Rename-Item -Path $Final -NewName ([System.IO.Path]::GetFileName($old)) -Force
            Move-Item -Force $New $Final
        }
    } else {
        Move-Item -Force $New $Final
    }
}

Install-Binary $torvcTmp (Join-Path $BinDir 'torvc.exe')
Install-Binary $runeTmp  (Join-Path $BinDir 'rune.exe')

# --- runtime + standard library --------------------------------------------
Download "$RawRef/runtime/torvik_runtime.c" (Join-Path $LibDir 'torvik_runtime.c')
Download "$RawRef/VERSION"                   (Join-Path $LibDir 'VERSION')
# v1.1.3: also install the compiler library sources and the std umbrella module,
# matching the Linux installer. std.tv in particular was missing, which broke
# `apply std;` on every Windows install.
foreach ($a in @('torvik_lexer.tv','torvik_parser.tv','torvik_codegen.tv','diag.tv','std.tv')) {
    try { Download "$RawRef/src/$a" (Join-Path $LibDir $a) } catch { }
}
$stdDir = Join-Path $LibDir 'std'
New-Item -ItemType Directory -Force -Path $stdDir | Out-Null
foreach ($a in @('math','strings','list','path')) {
    try { Download "$RawRef/src/std/$a.tv" (Join-Path $stdDir "$a.tv") } catch { }
}

# --- .tv file type + icon (v1.2.0) -------------------------------------------
# Register the .tv extension with a friendly type name and the Torvik file icon
# (per-user HKCU keys - no admin needed). No open-command is registered: source
# files belong to your editor, not to the toolchain.
try { Download "$RawRef/assets/torvik-file.ico" (Join-Path $InstallDir 'torvik-file.ico') } catch { }
$icoPath = Join-Path $InstallDir 'torvik-file.ico'
if (Test-Path $icoPath) {
    $clsRoot = 'HKCU:\Software\Classes'
    New-Item -Path "$clsRoot\.tv" -Force | Out-Null
    Set-ItemProperty -Path "$clsRoot\.tv" -Name '(Default)' -Value 'Torvik.Source'
    New-Item -Path "$clsRoot\Torvik.Source" -Force | Out-Null
    Set-ItemProperty -Path "$clsRoot\Torvik.Source" -Name '(Default)' -Value 'Torvik Source File'
    New-Item -Path "$clsRoot\Torvik.Source\DefaultIcon" -Force | Out-Null
    Set-ItemProperty -Path "$clsRoot\Torvik.Source\DefaultIcon" -Name '(Default)' -Value "$icoPath,0"
    # Keep the type connected to editors in the Open-with list without hijacking open.
    New-Item -Path "$clsRoot\.tv\OpenWithProgids" -Force | Out-Null
    Set-ItemProperty -Path "$clsRoot\.tv\OpenWithProgids" -Name 'Torvik.Source' -Value '' -Force
    # Belt and braces: some shell paths consult SystemFileAssociations for the icon.
    New-Item -Path "$clsRoot\SystemFileAssociations\.tv\DefaultIcon" -Force | Out-Null
    Set-ItemProperty -Path "$clsRoot\SystemFileAssociations\.tv\DefaultIcon" -Name '(Default)' -Value "$icoPath,0"
    # THE common reason the icon does not show: Explorer's per-user UserChoice
    # (written when you ever picked "always open with <app>" for .tv) OVERRIDES
    # the ProgID - Explorer then shows that app's icon and never consults ours.
    # Writing UserChoice is hash-protected, but deleting it is allowed; after
    # deletion .tv falls back to Torvik.Source and the icon shows. Choosing
    # "always use this app" later re-creates UserChoice and the editor's icon
    # returns - that is Windows association design; re-run this script to
    # reclaim the Torvik icon.
    $uc = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExts\.tv\UserChoice'
    if (Test-Path $uc) {
        try {
            $cur = (Get-ItemProperty -Path $uc -ErrorAction SilentlyContinue).ProgId
            if ($cur -ne 'Torvik.Source') {
                Remove-Item -Path $uc -Force -ErrorAction SilentlyContinue
                Write-Host "  Cleared a previous per-user .tv association ($cur) so the Torvik icon can show."
            }
        } catch { }
    }
    # Tell the shell associations changed so Explorer refreshes without a reboot.
    try {
        Add-Type -Namespace TorvikShell -Name Notify -MemberDefinition '[System.Runtime.InteropServices.DllImport("shell32.dll")] public static extern void SHChangeNotify(int wEventId, int uFlags, System.IntPtr dwItem1, System.IntPtr dwItem2);' -ErrorAction SilentlyContinue
        [TorvikShell.Notify]::SHChangeNotify(0x08000000, 0x0000, [System.IntPtr]::Zero, [System.IntPtr]::Zero)
    } catch { }
    try { Start-Process -FilePath "$env:WINDIR\System32\ie4uinit.exe" -ArgumentList '-show' -WindowStyle Hidden -ErrorAction SilentlyContinue } catch { }
    Write-Host "Registered the .tv file type and icon (may need an Explorer restart to show)."
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
# Best-effort cleanup of a binary we had to rename aside because it was running
# (a self-update via `rune update`). It's just the previous rune.exe; if it's
# still locked this pass, it'll be removable next run.
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $BinDir 'rune.exe.old'), (Join-Path $BinDir 'torvc.exe.old')

Write-Host "Torvik v$V installed."
Write-Host '>>> Open a NEW terminal (so PATH refreshes), then:  rune --version'
Write-Host '    From here on, `rune update` upgrades Torvik and `rune uninstall` removes it.'
