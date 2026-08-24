# fuzz.ps1 — feed the compiler malformed input and assert it never loses control.
#
# Windows companion to fuzz.sh. Keep the two in step.
#
# WHAT THIS TESTS, and it is one property only:
#
#   For ANY input, torvc must either compile it or refuse it cleanly.
#
# "Cleanly" means exit 0 (compiled) or exit 1 (a located user error). Anything
# else is a bug in the compiler: exit 70 is an internal error asking the user to
# report a toolchain fault for their own typo, and a crash means the compiler
# fell over on text somebody typed.
#
# WHY IT MATTERS. Torvik reports user errors through a located-error channel and
# reserves TVC-xxxx codes for faults it could not handle. That split is only
# meaningful if the second set is unreachable from user input — and the only way
# to find out is to try. The hand-written negative cases test the mistakes we
# thought of; this tests the ones we did not.
#
# The generator is deliberately dumb. Mutating real programs reaches deep into the
# compiler and needs no model of the language; random bytes mostly bounce off the
# lexer.
#
# Usage: powershell -ExecutionPolicy Bypass -File fuzz.ps1 [torvc.exe] [iterations]

param([string]$Torvc = "torvc", [int]$N = 200)

$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Corpus = Join-Path $Here (Join-Path "cases" "pos")
$Work = Join-Path ([System.IO.Path]::GetTempPath()) ("tvfuzz_" + [System.Guid]::NewGuid().ToString("N").Substring(0,8))

if (-not (Test-Path $Corpus)) { Write-Host "fuzz: no corpus at $Corpus"; exit 1 }
New-Item -ItemType Directory -Force -Path $Work | Out-Null

# Fragments that have historically confused compilers: unbalanced delimiters,
# deep nesting, boundary numbers, unterminated text.
$Frags = @(
    '{', '}', '(', ')', '[', ']', '"', "'", ';', '::', '->', '~>', '!@',
    'df', 'set', 'fixed', 'check', 'whilst', 'return', 'unsafe', 'extern',
    'skil(', 'shape', 'varda<', 'result<', 'list<', '0x', '1e', '1e999',
    '-9223372036854775808', '18446744073709551615',
    '340282366920938463463374607431768211455', '\x', '\',
    '{{{{{{{{{{', '((((((((((', '""""""""""'
)

$files = @(Get-ChildItem (Join-Path $Corpus "*.tv") | Where-Object { $_.Name -notlike "helpers*" })
if ($files.Count -eq 0) { Write-Host "fuzz: corpus is empty"; exit 1 }

$fails = 0
$done = 0
for ($i = 1; $i -le $N; $i++) {
    $done = $i
    $victim = $files[$i % $files.Count]
    $src = Get-Content $victim.FullName
    if ($src.Count -lt 2) { continue }

    $at = ($i * 7919) % $src.Count
    $frag = $Frags[($i * 31) % $Frags.Count]

    switch ($i % 4) {
        0 { $out = @($src[0..$at]) + @($frag) + @($src[($at+1)..($src.Count-1)]) }   # insert
        1 { $out = @($src | Where-Object { $_ -ne $src[$at] }) }                      # delete
        2 { $out = @($src[0..$at]) }                                                  # truncate
        3 { $tmp = @($src); $tmp[$at] = $frag; $out = $tmp }                           # replace
    }

    $f = Join-Path $Work "g.tv"
    Set-Content -Path $f -Value $out

    Push-Location $Work
    & $Torvc "g.tv" -o out -q *> "log.txt"
    $code = $LASTEXITCODE
    Pop-Location

    # 0 and 1 are the two correct answers. Everything else is a finding.
    if ($code -ne 0 -and $code -ne 1) {
        $fails++
        Write-Host "FAIL  fuzz: exit $code on mutation $i (from $($victim.Name))"
        if ($code -eq 70) {
            Write-Host "      TVC internal error - a user mistake reported as a toolchain bug"
        } elseif ($code -lt 0 -or $code -gt 128) {
            Write-Host "      the compiler crashed, rather than refusing"
        }
        Copy-Item $f (Join-Path $Here "fuzz-fail-$i.tv") -Force
        Write-Host "      saved: dev/tests/fuzz-fail-$i.tv"
        Get-Content (Join-Path $Work "log.txt") -TotalCount 3 | ForEach-Object { Write-Host "      $_" }
        if ($fails -ge 5) { break }
    }
}

Remove-Item -Recurse -Force $Work -ErrorAction SilentlyContinue

if ($fails -ne 0) {
    Write-Host "fuzz: $fails input(s) made the compiler lose control over $done mutations."
    exit 1
}

Write-Host "ok    fuzz: $done mutations, every one compiled or refused cleanly"
exit 0
