# check_cmdline.ps1 — refuse source that builds a command line from unvalidated data.
#
# Windows companion to check_cmdline.sh. Keep the two in step: they enforce the
# same rule, and a rule enforced on one platform only is a rule that drifts.
#
# WHY THIS EXISTS. Seven command-injection issues were found in Torvik between
# 1.5.1 and 1.5.4. Every one had the same shape: a value reached a command line
# for clang or the linker without being checked, and every fix closed the site
# that had been found while leaving the others open. The last one was in `shq`
# itself — the helper the earlier fixes had been relying on.
#
# A runtime check cannot end that pattern, because the mistake is made while
# WRITING the compiler, not while running it. This reads the compiler's own
# source and fails if a value reaches a command line without a validator.
#
# HOW IT DECIDES. Per SITE, not per file. "Validated somewhere in the file" is too
# weak a rule and was tried first: `out` is wrapped in shq() at two sites, so
# using it raw at a third still looked validated, and a deliberately reintroduced
# bug went undetected. Validated calls are deleted from each command-building line
# and anything still standing is a finding.
#
# Usage: powershell -ExecutionPolicy Bypass -File check_cmdline.ps1 [source.tv]

param([string]$Src = "")

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrEmpty($Src)) {
    $Src = Join-Path $Here (Join-Path ".." (Join-Path ".." (Join-Path "src" "torvc_main.tv")))
}

if (-not (Test-Path $Src)) {
    Write-Host "check_cmdline: cannot find $Src"
    exit 1
}

# Values the COMPILER constructs, never taken from a user. Safe because of where
# they come from, not because of any check. Keep in step with check_cmdline.sh.
$Allow = @(
    'tgt','lld_ent','lld_ld','bare_rt','bare_extra','bare_entry_flag',
    'bare_ld','bare_opt','libflags','extra_objs','opt_flags','final_flags',
    'g_build_arch','g_build_entry'
)

# Names that are language furniture or results coming back, not inputs going in.
$Ignore = @(
    'str','i64','void','bool','str_concat','set','fixed','check','fallback',
    'return','int_to_str','fmt','len','probe_cmd','line_eq','sys_os_name',
    'torvik_path_join','torvik_build_dir','home_dir','temp_dir','cmd','run',
    'echo','exit','df','oc'
)

$lines = Get-Content $Src | Select-String -Pattern 'run\(|_cmd: str|_cmd =|cmd = ' |
    Where-Object { $_.Line -notmatch '^\s*//' -and $_.Line -notmatch 'df run|df probe_cmd' }

$found = 0
foreach ($l in $lines) {
    $text = $l.Line

    # Strip the assignment target: `fixed rc: i64 = run(cmd)` names rc, but rc is
    # the exit status coming back, not an input going in.
    $text = $text -replace '^\s*(set|fixed)\s+[a-z_0-9]+(\s*:\s*[a-z_0-9<>]+)?\s*=', '='

    # Strip validated calls and string literals.
    $text = $text -replace 'shq\([a-z_0-9]*\)', ''
    $text = $text -replace 'run_arg_ok\([a-z_0-9]*\)', ''
    $text = $text -replace 'path_arg_ok\([a-z_0-9]*\)', ''
    $text = $text -replace 'link_lib_ok\([a-z_0-9]*\)', ''
    $text = $text -replace 'build_triple_ok\([a-z_0-9]*\)', ''
    $text = $text -replace '"[^"]*"', ''

    foreach ($m in ([regex]'\b[a-z_][a-z_0-9]{2,}\b').Matches($text)) {
        $n = $m.Value
        if ($Ignore -contains $n) { continue }
        if ($Allow -contains $n) { continue }
        # Assembled commands and results are not inputs.
        if ($n -match '(_cmd$|cmd$|_rc$|^rc$|_result$|_fresh$|_compile$|^have_)') { continue }
        Write-Host "  $n  (line $($l.LineNumber)) reaches a command line with no validator"
        $found++
    }
}

if ($found -ne 0) {
    Write-Host ""
    Write-Host "check_cmdline: $found value(s) reach a command line unvalidated."
    Write-Host ""
    Write-Host "  Wrap paths in shq(); use the validator matching the value's kind for"
    Write-Host "  anything else. If a value is genuinely compiler-constructed, add it to"
    Write-Host "  the allow-list in this script with a note saying why - do not silence"
    Write-Host "  it by reformatting the code."
    exit 1
}

Write-Host "ok    cmdline: every value reaching a command line is validated"
exit 0
