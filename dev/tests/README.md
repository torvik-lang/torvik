# Torvik end-to-end test suite

**166 cases**, all of which must pass before a release:

| Group | Count | What it covers |
| --- | --- | --- |
| `cases/pos` | 76 | The language surface: values, operators, control flow, functions (including optional and variadic parameters), collections, weave, membership, files, system, stdin, concurrency (`raven` tasks and `bridge` channels), table iteration, interpolation scoping, argument type-checking — and, new in v1.5.0, raw pointers, fixed arrays, `shape` structs, volatile MMIO, inline assembly, freestanding builds and the `f16`/`f32`/`f128` float widths. |
| `cases/neg` | 74 | Programs that must **fail to compile**, each with the error text it has to produce. |
| warnings | 9 | The warning system: unused variables, unreachable code, `!@` directives. |
| torvc flags | 7 | `-o`, `-q`, `--final`, `--no-warn`, run mode, argument forwarding. |

Every case is end-to-end: compile a real program with a real `torvc`, run it, and
compare what came out.

> **rune is tested in its own repository** as of v1.5.0
> (`rune/dev/tests/run_tests.sh`). rune ships on its own version line, so a rune
> regression should be catchable without building the compiler — and this suite runs
> with only `torvc` installed.

## Run

Linux:

    sh run_tests.sh [path-to-torvc]

Windows (PowerShell):

    powershell -ExecutionPolicy Bypass -File run_tests.ps1 [torvc-path]

Both default to `torvc` on PATH. All work happens in `./tv-test-work` (never `/tmp`
or `%TEMP%`); the full log is `tv-test-work/results.log`. Exit code 0 = all green.

## Layout

- `cases/pos/NAME.tv` + `NAME.out` — compile, run, diff stdout. Optional
  `NAME.code` (expected exit code, default 0) and `NAME.in` (stdin).
  `helpers*.tv` are support modules copied into `./src` for `apply` tests.
- `cases/neg/NAME.tv` + `NAME.err` — must FAIL to compile with exit 1 and an
  error message containing the `.err` substring (case-insensitive).

## Adding a case

Write the program, run it once, and save what it printed:

    torvc cases/pos/NN_thing.tv -o /tmp/t && /tmp/t > cases/pos/NN_thing.out

For a negative case, put a distinctive fragment of the expected error in the `.err`
file — enough to be specific, short enough to survive rewording:

    printf "index must be a whole number" > cases/neg/NN_thing.err

A negative case is worth adding whenever a mistake produced a *confusing* error, not
only when it produced none. Several cases here exist because a user error was once
reported as an internal compiler error, and the test is what keeps that from
coming back.

## Freestanding cases

Bare-metal builds are exercised through `examples/kernel/`, which is checked as part
of release preparation rather than from this suite — the output is a kernel image, so
verifying it means inspecting the ELF (multiboot header, entry symbol, section
addresses) rather than running it. See that directory's README.
