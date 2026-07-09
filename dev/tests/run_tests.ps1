# run_tests.ps1 - Torvik v1.1.x end-to-end test suite (Windows 10+).
# Usage: powershell -ExecutionPolicy Bypass -File run_tests.ps1 [torvc-path] [rune-path]
#   Defaults to `torvc` / `rune` on PATH.
# All work happens in .\tv-test-work (never %TEMP%). Exit code: 0 all pass, 1 any failure.
# Full log in .\tv-test-work\results.log

param(
    [string]$Torvc = "torvc",
    [string]$Rune  = "rune"
)

$ErrorActionPreference = "Continue"
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Work = Join-Path $Here "tv-test-work"
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory -Path $Work | Out-Null
$Log = Join-Path $Work "results.log"
"" | Set-Content $Log

$script:PASS = 0
$script:FAIL = 0
$script:FAILED = @()

function Note([string]$msg) { Write-Host $msg; Add-Content $Log $msg }

function Get-Cmd([string]$name) {
    $c = Get-Command $name -ErrorAction SilentlyContinue
    if ($null -eq $c) { Write-Host "$name not found"; exit 1 }
    return $c.Source
}
$TorvcExe = Get-Cmd $Torvc
$RuneExe  = Get-Cmd $Rune
Note "== torvc: $(& $TorvcExe --version 2>&1) =="
Note "== rune:  $(& $RuneExe --version 2>&1) =="
Note "== host:  $([System.Environment]::OSVersion.VersionString) =="

# Normalize output: CRLF -> LF, strip one trailing newline (matches .out goldens)
function Read-Norm([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    $t = [System.IO.File]::ReadAllText($path) -replace "`r`n", "`n"
    return $t.TrimEnd("`n")
}

# ---------- positive cases ----------
Note ""
Note "== POSITIVE CASES =="
Get-ChildItem (Join-Path $Here "cases\pos\*.tv") | ForEach-Object {
    $base = $_.BaseName
    if ($base -like "helpers*") { return }
    $exp     = Join-Path $Here "cases\pos\$base.out"
    $expcode = Join-Path $Here "cases\pos\$base.code"
    $stdinF  = Join-Path $Here "cases\pos\$base.in"
    $d = Join-Path $Work "pos_$base"
    New-Item -ItemType Directory -Path $d | Out-Null
    Copy-Item $_.FullName $d
    $srcDir = Join-Path $d "src"
    New-Item -ItemType Directory -Path $srcDir | Out-Null
    Get-ChildItem (Join-Path $Here "cases\pos\helpers*.tv") -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item $_.FullName $srcDir }

    Push-Location $d
    & $TorvcExe "$base.tv" -o prog -q *> compile.log
    $crc = $LASTEXITCODE
    if ($crc -ne 0) {
        Pop-Location
        $script:FAIL++; $script:FAILED += "pos/$base(compile)"
        Note "FAIL  pos/$base  (did not compile)"
        Get-Content (Join-Path $d "compile.log") | ForEach-Object { Add-Content $Log "      $_" }
        return
    }
    $exe = if (Test-Path ".\prog.exe") { ".\prog.exe" } else { ".\prog" }
    if (Test-Path $stdinF) { Get-Content $stdinF | & $exe *> actual.out }
    else                   { & $exe *> actual.out }
    $rcode = $LASTEXITCODE
    Pop-Location

    $want = Read-Norm $exp
    $got  = Read-Norm (Join-Path $d "actual.out")
    $wantCode = 0
    if (Test-Path $expcode) { $wantCode = [int](Get-Content $expcode | Select-Object -First 1) }
    $ok = ($got -eq $want) -and ($rcode -eq $wantCode)
    if ($ok) { $script:PASS++; Note "ok    pos/$base" }
    else {
        $script:FAIL++; $script:FAILED += "pos/$base"
        Note "FAIL  pos/$base  (exit=$rcode)"
        Add-Content $Log "      --- expected ---"; Add-Content $Log $want
        Add-Content $Log "      --- actual ---";   Add-Content $Log $got
    }
}

# ---------- negative cases ----------
Note ""
Note "== NEGATIVE CASES (expected clean compile errors) =="
Get-ChildItem (Join-Path $Here "cases\neg\*.tv") | ForEach-Object {
    $base = $_.BaseName
    $errf = Join-Path $Here "cases\neg\$base.err"
    $d = Join-Path $Work "neg_$base"
    New-Item -ItemType Directory -Path $d | Out-Null
    Copy-Item $_.FullName $d
    Push-Location $d
    & $TorvcExe "$base.tv" -o prog -q *> compile.log
    $code = $LASTEXITCODE
    Pop-Location
    if ($code -eq 0) {
        $script:FAIL++; $script:FAILED += "neg/$base(compiled!)"
        Note "FAIL  neg/$base  (COMPILED - expected a clean error)"
        return
    }
    if ($code -ne 1) {
        $script:FAIL++; $script:FAILED += "neg/$base(exit=$code)"
        Note "FAIL  neg/$base  (exit=$code, expected 1 - internal error?)"
        return
    }
    $needle = (Get-Content $errf -Raw).Trim()
    $logTxt = Get-Content (Join-Path $d "compile.log") -Raw
    if ($logTxt -match [regex]::Escape($needle)) { $script:PASS++; Note "ok    neg/$base" }
    else {
        $script:FAIL++; $script:FAILED += "neg/$base(msg)"
        Note "FAIL  neg/$base  (error text missing '$needle')"
        Add-Content $Log $logTxt
    }
}

# ---------- rune cases ----------
Note ""
Note "== RUNE CASES =="
function Rune-Case([string]$name, [scriptblock]$body) {
    $d = Join-Path $Work "rune_$name"
    New-Item -ItemType Directory -Path $d | Out-Null
    Push-Location $d
    $ok = $false
    try { $ok = & $body } catch { $ok = $false; Add-Content $Log "      exception: $_" }
    Pop-Location
    if ($ok) { $script:PASS++; Note "ok    rune/$name" }
    else     { $script:FAIL++; $script:FAILED += "rune/$name"; Note "FAIL  rune/$name" }
}

Rune-Case "new_build_run" {
    & $RuneExe new myapp *> $null; if ($LASTEXITCODE -ne 0) { return $false }
    if (-not (Test-Path "myapp\torvik.rune")) { return $false }
    if (-not (Test-Path "myapp\src\main.tv")) { return $false }
    Set-Location myapp
    & $RuneExe build *> $null; if ($LASTEXITCODE -ne 0) { return $false }
    if (-not ((Test-Path "build\myapp.exe") -or (Test-Path "build\myapp"))) { return $false }
    $out = & $RuneExe run 2>&1 | Out-String
    return ($out -match "(?i)hello")
}

Rune-Case "incremental" {
    & $RuneExe new capp *> $null; Set-Location capp
    & $RuneExe run *> first.log
    & $RuneExe run *> second.log
    $second = Get-Content second.log -Raw
    return (($second -match "(?i)cache|up.to.date|unchanged") -or ($second -notmatch "(?i)compil"))
}

Rune-Case "exit_propagation" {
    & $RuneExe new eapp *> $null; Set-Location eapp
    Set-Content "src\main.tv" "df main() -> void {`n    exit(4);`n}`n"
    & $RuneExe run *> $null
    return ($LASTEXITCODE -eq 4)
}

Rune-Case "clean_list_version" {
    & $RuneExe new lapp *> $null; Set-Location lapp
    & $RuneExe build *> $null; if ($LASTEXITCODE -ne 0) { return $false }
    $lst = & $RuneExe list 2>&1 | Out-String
    if ($lst -notmatch "(?i)lapp") { return $false }
    & $RuneExe clean *> $null
    if (Test-Path "build") { return $false }
    $ver = & $RuneExe version 2>&1 | Out-String
    return ($ver -match "1\.")
}

Rune-Case "min_version_gate" {
    & $RuneExe new vapp *> $null; Set-Location vapp
    $m = Get-Content torvik.rune -Raw
    if ($m -match "(?m)^torvik\s*=") { $m = $m -replace "(?m)^torvik\s*=.*$", 'torvik = "99.0.0"' }
    else { $m += "`ntorvik = `"99.0.0`"`n" }
    Set-Content torvik.rune $m
    & $RuneExe build *> b.log
    if ($LASTEXITCODE -eq 0) { return $false }
    return ((Get-Content b.log -Raw) -match "(?i)update|version")
}

Rune-Case "missing_entry" {
    New-Item -ItemType Directory -Path p1 | Out-Null; Set-Location p1
    Set-Content torvik.rune "[project]`nname = `"p1`"`n"
    & $RuneExe build *> b.log
    if ($LASTEXITCODE -eq 0) { return $false }
    return ((Get-Content b.log -Raw) -match "main\.tv")
}

Rune-Case "bad_name" {
    & $RuneExe new "bad/name" *> $null
    if ($LASTEXITCODE -eq 0) { return $false }
    & $RuneExe new "" *> $null
    if ($LASTEXITCODE -eq 0) { return $false }
    return $true
}

Rune-Case "final_build" {
    & $RuneExe new fapp *> $null; Set-Location fapp
    & $RuneExe build --final *> $null; if ($LASTEXITCODE -ne 0) { return $false }
    $exe = if (Test-Path "build\fapp.exe") { ".\build\fapp.exe" } else { ".\build\fapp" }
    $out = & $exe 2>&1 | Out-String
    return ($out -match "(?i)hello")
}

# ---------- torvc flag cases ----------
Note ""
Note "== TORVC FLAG CASES =="
$d = Join-Path $Work "flags"
New-Item -ItemType Directory -Path $d | Out-Null
Push-Location $d
Set-Content flag.tv "df main() -> void {`n    echo!(`"flagtest`");`n}`n"

function Flag-Case([string]$name, [scriptblock]$body) {
    $ok = $false
    try { $ok = & $body } catch { $ok = $false }
    if ($ok) { $script:PASS++; Note "ok    flags/$name" }
    else     { $script:FAIL++; $script:FAILED += "flags/$name"; Note "FAIL  flags/$name" }
}

Flag-Case "quiet" {
    & $TorvcExe flag.tv -o out1 -q *> q.log
    if ($LASTEXITCODE -ne 0) { return $false }
    if ((Get-Item q.log).Length -gt 0) { return $false }
    $exe = if (Test-Path ".\out1.exe") { ".\out1.exe" } else { ".\out1" }
    return ((& $exe) -eq "flagtest")
}
Flag-Case "success_message" {
    & $TorvcExe flag.tv -o out2 *> v.log
    if ($LASTEXITCODE -ne 0) { return $false }
    return ((Get-Content v.log -Raw) -match "(?i)compiled successfully")
}
Flag-Case "final" {
    & $TorvcExe flag.tv -o out3 --final -q *> f.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $exe = if (Test-Path ".\out3.exe") { ".\out3.exe" } else { ".\out3" }
    return ((& $exe) -eq "flagtest")
}
Flag-Case "version_help" {
    $v = & $TorvcExe --version 2>&1 | Out-String
    if ($v -notmatch "1\.") { return $false }
    & $TorvcExe -h *> h.log
    return $true
}
Flag-Case "derived_name" {
    Remove-Item flag.exe, flag -ErrorAction SilentlyContinue
    & $TorvcExe flag.tv -q *> d.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $exe = if (Test-Path ".\flag.exe") { ".\flag.exe" } else { ".\flag" }
    if (-not (Test-Path $exe)) { return $false }
    return ((& $exe) -eq "flagtest")
}
Flag-Case "missing_source" {
    & $TorvcExe no_such_file.tv -o x -q *> m.log
    if ($LASTEXITCODE -ne 1) { return $false }
    return ((Get-Content m.log -Raw) -notmatch "TVC-")
}
Pop-Location

# ---------- summary ----------
Note ""
Note "== SUMMARY: $($script:PASS) passed, $($script:FAIL) failed =="
if ($script:FAILED.Count -gt 0) { Note ("failed: " + ($script:FAILED -join " ")) }
Note "(full log: $Log)"
if ($script:FAIL -eq 0) { exit 0 } else { exit 1 }
