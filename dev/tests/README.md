# Torvik end-to-end test suite

34 positive cases covering the language surface (values, operators, control flow,
functions, collections, weave, membership, std lib, files, system, stdin),
30 expected-clean-error cases, 8 rune project-tool cases, and 6 torvc flag
cases.

## Run

Linux:

    sh run_tests.sh [path-to-torvc] [path-to-rune]

Windows (PowerShell):

    powershell -ExecutionPolicy Bypass -File run_tests.ps1 [torvc-path] [rune-path]

Both default to `torvc`/`rune` on PATH. All work happens in `./tv-test-work`
(never `/tmp` or `%TEMP%`); the full log is `tv-test-work/results.log`.
Exit code 0 = all green.

## Layout

- `cases/pos/NAME.tv` + `NAME.out` — compile, run, diff stdout. Optional
  `NAME.code` (expected exit code, default 0) and `NAME.in` (stdin).
  `helpers*.tv` are support modules copied into `./src` for `apply` tests.
- `cases/neg/NAME.tv` + `NAME.err` — must FAIL to compile with exit 1 and an
  error message containing the `.err` substring (case-insensitive).
