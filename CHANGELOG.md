# Changelog

## [1.0.1] — 2026

### Fixes

- **`readenv` of an unset environment variable no longer crashes.** The runtime returned a
  null pointer when the variable was not set, which segfaulted on first use of the result.
  `readenv` now returns `""` when the variable is unset. Existing installations pick up the
  fix by re-running the installer; all builds after that are fixed automatically.

## [1.0.0] — 2026

First stable release of Torvik: a self-hosting, compiled, general-purpose language.

### Language

- **Native compilation** through an LLVM backend (LLVM IR linked with `clang`).
- **Functions** (`df`) with typed parameters, return types, and full recursion.
- **Variables**: `set` (mutable) and `fixed` (immutable), with required type annotations.
- **Types**: `i8`–`i64`, `u8`–`u64`, the wide `i128`/`u128`, `f64`, `bool`, `str`.
- **Character literals** (`'A'`) as one-character strings, with `byte_at` for raw byte values.
- **Collections**: `list<T>`, `table<K, V>`, and `bag<T>`.
- **Control flow**: `check` / `fallback` (if/else) and `guard` / `fallback` (early-exit).
- **Loops**: `whilst` (condition loop) and `each i in START..END` (range loop), with
  `break` and `continue`.
- **Ternary expression**: `cond ?> a !> b`.
- **Assertions and aborts**: `vow(cond, message)` and `halt(message)`.
- **String interpolation**: `fmt("Hello {name}")` and positional `fmt("{} {}", a, b)`.
- **`unsafe` blocks** and the **`apply`** module mechanism.
- **Automatic reference counting** for heap values — leak-free for supported patterns, with
  clean out-of-memory behavior and no reference cycles by construction.

### Toolchain

- **`torvc`** — the compiler, with `-o`, `-q`/`--quiet`, `-v`/`--verbose`, `--final`
  (production build), and `--version`. Reports compile time on success.
- **`rune`** — the project tool: `new`, `init`, `build`, `run` (incremental), `list`,
  `clean`, `version`, and `uninstall`. Faithfully propagates program exit codes.
- **Self-hosting** — `torvc` and `rune` are written in Torvik and reproduce bit-for-bit on a
  clean self-rebuild.

### Standard library

A broad set of built-in functions covering strings, conversions, math, lists/tables/bags,
console input, files, system access, and time. See [docs/STDLIB.md](docs/STDLIB.md).

### Licensing

- Released under the **GNU AGPL-3.0** with a runtime-library exception (programs written in
  Torvik carry no license obligation from the compiler).

### Notes & known limitations

- Ternary branches are both evaluated (no short-circuit) — keep them to simple values.
- A value taken directly from a function call or an index must be bound to a variable before
  it is used in a larger arithmetic or comparison expression (reported as a clean compile
  error). 
- Planned for upcoming releases (v1.1.0 onward): full Windows support, structs (`shape`),
  pattern matching (`when`), async/concurrent tasks (`task`), result types, enums, `f32`, a
  dedicated `char` type, fixed-size arrays, inclusive ranges (`..=`), direct collection
  iteration (`each x in xs`), and block comments.
