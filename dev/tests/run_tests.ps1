# run_tests.ps1 - Torvik v1.5.x end-to-end test suite (Windows 10+).
# Usage: powershell -ExecutionPolicy Bypass -File run_tests.ps1 [torvc-path]
#   Defaults to `torvc` on PATH.
#
# v1.5.0: rune ships from its own repository and is tested there, so this suite
# needs only the compiler.
# All work happens in .\tv-test-work (never %TEMP%). Exit code: 0 all pass, 1 any failure.
# Full log in .\tv-test-work\results.log

param(
    [string]$Torvc = "torvc"
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
Note "== torvc: $(& $TorvcExe --version 2>&1) =="
Note "== host:  $([System.Environment]::OSVersion.VersionString) =="

# Normalize output: CRLF -> LF, strip one trailing newline (matches .out goldens)
function Read-Norm([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    $t = [System.IO.File]::ReadAllText($path) -replace "`r`n", "`n"
    return $t.TrimEnd("`n")
}

# ---------- positive cases ----------
Note ""
# The corpus has to be here before anything else is worth trying. Without this
# check, a missing cases directory produced a wall of Get-ChildItem exceptions -
# one per loop - and then a SUMMARY line, which reads as though the suite ran.
# It is the same shape as a skip reported as a pass: the run looks like a result
# and is not.
foreach ($needed in @("pos", "neg")) {
    $dir = Join-Path (Join-Path $Here "cases") $needed
    if (-not (Test-Path $dir)) {
        Write-Host ""
        Write-Host "error: the test corpus is missing." -ForegroundColor Red
        Write-Host "       expected: $dir"
        Write-Host ""
        Write-Host "       run_tests.ps1 needs the whole dev/tests directory, not just the"
        Write-Host "       script. If you copied the script on its own, unpack the full"
        Write-Host "       release folder instead."
        Write-Host ""
        exit 1
    }
}

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
    # Optional NAME.flags: extra torvc flags for this case, one line.
    $flagsFile = Join-Path $Here (Join-Path "cases" (Join-Path "pos" ($base + ".flags")))
    $extra = @()
    if (Test-Path $flagsFile) {
        $extra = ((Get-Content $flagsFile -Raw).Trim() -split '\s+')
    }
    & $TorvcExe "$base.tv" -o prog -q @extra *> compile.log
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
    # Optional NAME.flags - needed when what is refused is a FLAG rather than the
    # source. A bad OUTPUT PATH has no representation inside a .tv file, so the
    # case supplies it here. Keep in step with run_tests.sh.
    $negFlagsFile = Join-Path $Here (Join-Path "cases" (Join-Path "neg" ($base + ".flags")))
    if (Test-Path $negFlagsFile) {
        $negArgs = ((Get-Content $negFlagsFile -Raw).Trim() -split '\s+', 2)
        & $TorvcExe "$base.tv" -q @negArgs *> compile.log
    } else {
        & $TorvcExe "$base.tv" -o prog -q *> compile.log
    }
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

# ---------- torvc flag cases ----------
Note ""
Note "== TORVC FLAG CASES =="
$d = Join-Path $Work "flags"
New-Item -ItemType Directory -Path $d | Out-Null
Push-Location $d
Set-Content flag.tv "df main() -> void {`n    echo!(`"flagtest`");`n}`n"

function Flag-Case2([string]$name, [scriptblock]$body) {
    $ok = $false
    try { $ok = & $body } catch { $ok = $false }
    if ($ok) { $script:PASS++; Note "ok    $name" }
    else     { $script:FAIL++; $script:FAILED += $name; Note "FAIL  $name" }
}

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
# v1.5.0: no -o => RUN mode (python3-style). Executes the file, forwards extra
# args to the program, and leaves no binary behind.
Flag-Case "run_mode" {
    Remove-Item flag.exe, flag -ErrorAction SilentlyContinue
    $out = (& $TorvcExe flag.tv 2>&1 | Out-String)
    if ($out -notmatch "flagtest") { return $false }
    if ((Test-Path ".\flag.exe") -or (Test-Path ".\flag")) { return $false }
    return $true
}
Flag-Case "run_args" {
    @'
df main() -> void {
    fixed n: i64 = args();
    check n >= 2 { echo!(args_get(1)); } fallback { echo!("noargs"); }
}
'@ | Set-Content -Encoding ASCII argfwd.tv
    $out = (& $TorvcExe argfwd.tv HELLO 2>&1 | Out-String)
    return ($out -match "HELLO")
}
Flag-Case "missing_source" {
    & $TorvcExe no_such_file.tv -o x -q *> m.log
    if ($LASTEXITCODE -ne 1) { return $false }
    return ((Get-Content m.log -Raw) -notmatch "TVC-")
}
Pop-Location

# ---------- warning cases ----------
Note ""
Note "== WARNING CASES =="
$d = Join-Path $Work "warns"
New-Item -ItemType Directory -Path $d | Out-Null
Push-Location $d
Set-Content warny.tv "df main() -> void {`n    set unused: i64 = 1;`n    echo!(`"ran`");`n    return;`n    echo!(`"dead`");`n}`n"

Flag-Case2 "warns/emitted_nonfatal" {
    & $TorvcExe warny.tv -o wy *> w1.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $l = Get-Content w1.log -Raw
    if ($l -notmatch "warning:") { return $false }
    if ($l -notmatch "unused variable") { return $false }
    if ($l -notmatch "unreachable code") { return $false }
    $exe = if (Test-Path ".\wy.exe") { ".\wy.exe" } else { ".\wy" }
    return ((& $exe) -eq "ran")
}
Flag-Case2 "warns/no_warn_flag" {
    & $TorvcExe warny.tv -o wy2 --no-warn *> w2.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $l2 = [string](Get-Content w2.log -Raw)
    return ($l2 -notmatch "warning:")
}
Flag-Case2 "warns/quiet_keeps_warnings" {
    & $TorvcExe warny.tv -o wy3 -q *> w3.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $l3 = [string](Get-Content w3.log -Raw)
    return (($l3 -match "warning:") -and ($l3 -notmatch "Compiled successfully"))
}
Flag-Case2 "warns/underscore_exempt" {
    Set-Content uscore.tv "df main() -> void {`n    fixed _ignored: i64 = 5;`n    echo!(`"clean`");`n}`n"
    & $TorvcExe uscore.tv -o us *> w4.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $l4 = [string](Get-Content w4.log -Raw)
    return ($l4 -notmatch "warning:")
}
Flag-Case2 "warns/allow_category" {
    Set-Content direc.tv "!@ALLOW[unused_variable];`ndf main() -> void {`n    set unused: i64 = 1;`n    echo!(`"ran`");`n    return;`n    echo!(`"dead`");`n}`n"
    & $TorvcExe direc.tv -o dr *> w5.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $l = Get-Content w5.log -Raw
    return (($l -match "unreachable code") -and ($l -notmatch "unused variable"))
}
Flag-Case2 "warns/no_warn_directive" {
    Set-Content direc2.tv "!@NO_WARN;`ndf main() -> void {`n    set unused: i64 = 1;`n    echo!(`"ran`");`n    return;`n    echo!(`"dead`");`n}`n"
    & $TorvcExe direc2.tv -o dr2 *> w6.log
    if ($LASTEXITCODE -ne 0) { return $false }
    $l6 = [string](Get-Content w6.log -Raw)
    return ($l6 -notmatch "warning:")
}
Flag-Case2 "warns/typo_directive_errors" {
    Set-Content direc3.tv "!@NO_WRN;`ndf main() -> void { echo!(`"x`"); }`n"
    & $TorvcExe direc3.tv -o dr3 *> w7.log
    if ($LASTEXITCODE -ne 1) { return $false }
    $l7 = [string](Get-Content w7.log -Raw)
    return ($l7 -match "unknown warning directive")
}
Flag-Case2 "warns/unused_result" {
    Set-Content ur.tv "df init(code: i64) -> i64 { return code; }`ndf main() -> void { init(1); echo!(`"ran`"); }`n"
    & $TorvcExe ur.tv -o ur *> w8.log
    if ($LASTEXITCODE -ne 0) { return $false }
    return ([string](Get-Content w8.log -Raw) -match "unused")
}
Flag-Case2 "warns/apply_line_numbers" {
    Set-Content lineoff.tv "apply std;`ndf main() -> void {`n    nosuchfn();`n}`n"
    & $TorvcExe lineoff.tv -o lo *> w9.log
    if ($LASTEXITCODE -ne 1) { return $false }
    return ([string](Get-Content w9.log -Raw) -match "lineoff.tv:3:")
}
Pop-Location

# ---------- summary ----------
# v1.5.4: the security gate and the fuzzer, wired here as well as in
# run_tests.sh. A gate that runs on one platform only is a gate that drifts.
$scriptDir = $Here
foreach ($g in @(
    @{ name = "security/cmdline_validated"; file = "check_cmdline.ps1"; args = @() },
    @{ name = "fuzz/crash_freedom";         file = "fuzz.ps1";          args = @($TorvcExe, 150) }
)) {
    $gp = Join-Path $scriptDir $g.file
    if (Test-Path $gp) {
        # Whichever PowerShell is actually running this. Hard-coding "powershell"
        # means Windows PowerShell 5.1, which is not present on PowerShell 7 for
        # Linux or macOS - so the gates failed there with "term not recognized"
        # while the compiler itself was fine.
        $psExe = if ($PSVersionTable.PSEdition -eq 'Core') { 'pwsh' } else { 'powershell' }
        $glog = Join-Path $Work "$($g.file).log"
        & $psExe -ExecutionPolicy Bypass -File $gp @($g.args) *> $glog
        if ($LASTEXITCODE -eq 0) {
            $script:Pass++; Note "ok    $($g.name)"
        } else {
            $script:Fail++; $script:Failed += $g.name
            Note "FAIL  $($g.name)"
            # Guarded: if the gate could not start there is no log to read, and an
            # unguarded Get-Content buries the real error under its own.
            if (Test-Path $glog) {
                Get-Content $glog -TotalCount 12 | ForEach-Object { Note "      $_" }
            }
        }
    }
}

Note ""
Note "== SUMMARY: $($script:PASS) passed, $($script:FAIL) failed =="
if ($script:FAILED.Count -gt 0) { Note ("failed: " + ($script:FAILED -join " ")) }
Note "(full log: $Log)"
if ($script:FAIL -eq 0) { exit 0 } else { exit 1 }
