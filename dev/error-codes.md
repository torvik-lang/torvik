# Torvik Internal Error Codes

This registry documents the stable codes attached to **internal** faults in the
Torvik toolchain — bugs inside `torvc` or `rune`, or user input that drove the
toolchain into a state it should have handled. These are distinct from ordinary
user errors (a syntax mistake, a missing source file, a type error in user code),
and from environment problems (such as `clang` not being installed), all of which
carry their own plain messages, exit `1`, and are **never** logged.

When an internal fault fires, the toolchain:

1. prints a one-line message that includes the code, e.g.
   `torvc internal error TVC-5002 [ir-build]: clang rejected generated IR`;
2. appends a single entry to `~/.torvik/diagnostics.log` (if diagnostics are
   enabled — see below); and
3. exits with code **70** ("internal software error"), distinct from `1` used for ordinary user errors.

## The diagnostics log

- **Location:** `~/.torvik/diagnostics.log`
- **Format (tab-separated):**
  `epoch · datetime · tool version · CODE · phase · input-file-path · message`
- **Privacy:** the log records the input file *path* only — never the contents
  of a user's source.
- **Retention:** entries older than 30 days are pruned on write, with a hard cap
  of the 500 most recent entries as a size backstop.
- **Enable/disable (default ON):** controlled by the `diagnostics` key in
  `~/.torvik/config.rune`.

Manage the log through `rune`'s developer menu:

```
rune dev status                Show whether diagnostics are on, plus log stats
rune dev log                   Print the diagnostics log
rune dev log tail <n>          Print the last n entries
rune dev log clear             Clear the log
rune dev diagnostics on|off    Enable or disable diagnostics logging
```

## Code scheme

Codes are `TVC-NNNN` (the `torvc` compiler) or `RUNE-NNNN` (the `rune` package
manager). The leading digit of the number identifies the subsystem:

| Range      | Subsystem (torvc)                         |
|------------|-------------------------------------------|
| `TVC-1xxx` | Lexer                                     |
| `TVC-2xxx` | Parser                                    |
| `TVC-3xxx` | Codegen                                   |
| `TVC-4xxx` | Imports / module resolution               |
| `TVC-5xxx` | IR / runtime interface (clang, linking)   |
| `TVC-9xxx` | Internal invariant ("this cannot happen") |

| Range       | Subsystem (rune)              |
|-------------|-------------------------------|
| `RUNE-1xxx` | Manifest parsing              |
| `RUNE-2xxx` | Resolution / fetch            |
| `RUNE-3xxx` | Build orchestration           |
| `RUNE-9xxx` | Internal invariant            |

## Registry

| Code       | Phase         | Meaning                                                       | Likely cause |
|------------|---------------|---------------------------------------------------------------|--------------|
| `TVC-1001` | lex           | Token/offset table length mismatch (lexer scan drift).        | `extract_offsets` and `lex_source` scanned the source differently — an internal lexer bug, reproducible from the recorded input file. |
| `TVC-5001` | runtime-build | The bundled runtime C failed to compile (clang error).        | A broken or mismatched `torvik_runtime.c`, or a toolchain/`clang` problem in the environment. |
| `TVC-5002` | ir-build      | `clang` rejected the generated IR, or linking failed.         | A codegen bug emitting invalid LLVM IR for some construct — usually reproducible from the recorded input file. |
| `TVC-9001` | codegen       | Code generation produced no IR output.                        | Codegen returned without writing the IR file — an internal invariant violation distinct from `TVC-5002` (where IR exists but `clang` rejects it). |
| `RUNE-3001`| build         | `torvc` reported success but produced no output binary.       | An inconsistency between `torvc`'s exit status and its output — a `torvc` or `rune` orchestration bug. |

New codes are added here as they are introduced. Codes are stable once assigned:
a retired code is left documented rather than reused, so that an entry in an old
log always resolves to the same meaning.

## Located user-error channel (v1.1.0, maintainer note)

Expression- and statement-level codegen sites report user errors by emitting a
`  ; tv.uerr:POS:MESSAGE` marker into the function IR and CONTINUING codegen.
`collect_user_errors` gathers the markers after codegen and renders them as standard
located errors alongside parse/decl errors; the compile aborts before the IR reaches
clang, so anything emitted after a marker is never built. When adding a new user-error
site, call `emit_uerr(ir, TOKEN_POS, "message")` — do not use `echo!` + `exit(1)`, which
can report only one error and cannot point at a line.
