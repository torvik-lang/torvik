# Changelog

## [1.1.0] — 2026-07

### Language

- **Expression chaining, stage 2.** Comparisons now accept ANY operand shape on either side —
  variables, string literals, function-call results, list elements, `table_get` values, and
  parenthesized expressions. `whilst char_at(s, i) == " "`, `check substr(n, 0, 3) == "st_"`,
  `check "a" == x`, and `echo!(table_get(m, "k") * 2)` all just work; the "bind it to a
  variable first" rule is gone. Mixed arithmetic and comparison folds with correct precedence
  without parentheses (`a + f(x) > b`). The ternary's bind-first caveat for call branches is
  also gone (`c ?> str_concat(a, b) !> "other"` now types correctly).
- **Strings compare by content, always.** `==`/`!=` test string equality and `<` `<=` `>` `>=`
  order strings lexicographically, whatever produced the operands. This fixes silent
  wrong-answer bugs in v1.0: comparing a list element, a string-returning function result, or
  a parenthesized string against a literal compared POINTERS, so content-equal heap-built
  strings compared as different. Comparing a string with a number is now a clean compile error
  (previously: a pointer comparison, broken IR for ordering, or a runtime crash for
  `numvar == "text"`).
- **`u64` signedness is per-operand.** A `u64` variable, call result, or list element anywhere
  in an expression — not just the lead — selects unsigned division, modulo, shifts, and
  comparison predicates from that operand onward, and a `u64` on either side of a comparison
  orders unsigned. Previously `2 + big / x` and `check 5 < x` with a u64 `x` silently used
  signed operations. Signed-only expressions are unchanged.
- **Weave argument insertion.** `x ~> f(a, b)` calls `f(x, a, b)` — the woven value is
  inserted as the first argument. Works with user-defined functions (arity-checked, counting
  the insertion) plus `replace(from, to)` and `substr(start, end)`:
  `csv ~> trim ~> replace(",", "-") ~> upper`.
- **Operator edges.** A unary minus may prefix any chain operand — `a - -b`, `a + -5`,
  `a * -f(x)`, and the float equivalents — with correct precedence. `<|` (membership) and
  `~>` (weave) accept any left operand: literals, call results, and list elements, not just
  variables. Weave results are typed, so `s ~> trim == "x"` compares by content.
- **`<|` on string lists matches by CONTENT.** v1.0 matched by identity, so a freshly built
  string never matched an equal stored element. Membership now uses string comparison for
  `list<str>`, and an item/element type mismatch (`5 <| names`) is a clean compile error.
- **Ternary short-circuit.** `cond ?> a !> b` now lowers to branch + phi, so **only the
  taken branch is evaluated** (v1.0 lowered to a `select`, which evaluated both). Guarding
  patterns like `d != 0 ?> 100 / d !> 0` and recursion in a branch
  (`return n <= 1 ?> 1 !> n * fact(n - 1);`) now work as expected.
- **Ternary in more positions.** The ternary is now accepted inside parentheses
  (`(cond ?> a !> b)` — including in `check`/`guard` conditions and arithmetic chains like
  `(c ?> 10 !> 20) * 3`), as the right-hand side of plain reassignment (`x = c ?> a !> b;`)
  and compound assignment (`total += c ?> i !> 0;`), and as an indexed-store element
  (`xs[i] = c ?> a !> b;`).

- **`list<f64>` now works.** Float lists previously produced broken IR at the point of use
  (push truncated the value with an integer coercion; element reads misinterpreted the bits).
  Float elements are now stored as their exact bit pattern and recovered on read, so push,
  index read, arithmetic, comparison, `each`, `list_insert`, and `list_pop` all work on
  `list<f64>`, and its element type is inferable.
- **List element-type inference.** An unannotated `set xs = list_new();` now infers its
  element type from the first `push(xs, value)` — string, integer, or bool — so the annotation
  is optional in the common case. (`list<f64>` and `table` inference remain unsupported.)
- **`list_pop` is wired.** `list_pop(xs)` removes and returns the last element, typed as the
  list's element type, with correct reference-count ownership transfer for `list<str>`.
  Popping an empty list panics cleanly; `list<i128>` pop is a clean "not yet" error.
- **`rune update` with version pinning.** `rune update` updates the toolchain to the latest
  release by re-running the official installer for your platform (Linux/macOS shell or Windows
  PowerShell). You can now pin a version: `rune update v1.1.0` (exact), `rune update v1.0`
  (newest 1.0.x), or `rune update v1` (newest 1.x). Partial versions resolve against the
  GitHub release list; a nonexistent version fails cleanly without touching your current
  install, and the installer only replaces binaries once the download succeeds. `--yes` skips
  the prompt. A bare `rune update` first checks whether you're already on the latest release and
  says so instead of reinstalling (`--force` overrides); if the latest is a new **major**
  version it warns about possible breaking changes and asks you to confirm before updating.
- **Project minimum-version requirement.** A project can declare `torvik = "1.1.0"` in its
  `torvik.rune`; `rune build` and `rune run` refuse to build with an older toolchain and point
  the user at `rune update`. `rune new` records the current version in new projects.
- **Installer layout.** The Linux/macOS installer moved to `linux/install.sh` (the Windows
  installer is `windows/install.ps1`), keeping the repo root tidy. A compatibility shim at the
  old root path forwards to the new location, so existing one-liners keep working.
- **Windows installer script.** The `windows/` folder adds `install.ps1` (network install).
- **Windows support.** `torvc` and `rune` now build and run on Windows (x86-64). The C
  runtime carries a Win32 platform layer covering terminal input, process id, wall-clock time,
  OS/architecture/hostname/memory info, running commands, and recursive directory removal; the
  compiler computes a Windows-appropriate build directory (`%TEMP%\.torvik`), detects clang
  portably, targets `x86_64-w64-windows-gnu`, and emits `.exe` outputs. On Windows the back-end
  needs a clang that bundles the MinGW-w64 headers/libraries (LLVM-MinGW, MSYS2, or WinLibs);
  the installer checks for this and `torvc` gives an actionable message if it's missing. The
  generated code is unchanged — it is LLVM IR either way — so every language feature behaves
  identically across Linux and Windows.
- **Interpolation accepts expressions.** Inside `echo`/`echo!`/`fmt` interpolation, the braces
  now take a full expression, not just a variable name: `echo!("sum={a + b}")`,
  `echo!("y={f(x)}")`, `echo!("first={xs[0]}")`, and mixtures like `echo!("{n} of {n + 1}")`.
  The span is lexed and compiled like ordinary code (correct typing and precedence); a bare
  `{name}` remains the fast path, and undefined names / unclosed braces are still clean located
  errors.
- **Direct collection iteration.** `each x in xs { ... }` iterates a list directly, binding
  the loop variable to each element (typed as the list's element type). Works with
  `list<i64>` and `list<str>` (elements reference-counted correctly), supports `break` /
  `continue` and nesting, and a non-list target is a clean located error.
- **Block comments.** `#- ... -#`, nestable, usable inline mid-statement. An unterminated
  `#-` or a stray `-#` is a clean, located compile error.
- **Inclusive ranges.** `each i in START..+END` iterates through END itself; `START..END`
  stays exclusive.
- **Redeclaration protection.** Redeclaring a variable with a **different type** in the same
  function (including reusing a non-i64 name as an `each` loop variable) is now a clean,
  located compile error. Previously both declarations silently shared one storage slot —
  memory corruption at a distance. Same-type re-`set` (the loop-body idiom) remains legal.

### Standard library

- **System introspection, wired.** The runtime shipped these in v1.0 but the compiler never
  exposed them; they are now callable: `sys_os_name()`, `sys_os_version()`, `sys_arch()`,
  `sys_hostname()`, `sys_username()` (str), and `sys_cpu_count()`, `sys_mem_total()`,
  `sys_mem_free()`, `sys_pid()` (i64). All are fully chainable
  (`check sys_os_name() == "linux" { ... }`, `check sys_cpu_count() > 4 { ... }`).
- **`appendline(path, line)`** — append a line (newline added) to a file, argument order
  matching `writefile(path, content)`.
- **`typeof(x)`** — the type of a variable or literal as a string, resolved at **compile
  time** with zero runtime cost. Reports full annotations (`"list<str>"`, `"i64"`, `"str"`).
  A genuinely unknown type at the use site is a clean compile error.

### Toolchain

- **`rune uninstall` now cleans up completely.** It removes the Torvik PATH entry (the
  `# Torvik` marker and the `~/.torvik/bin` line) from `~/.bashrc`, `~/.profile`, `~/.zshrc`,
  and the fish config, and deregisters the `.tv` file type and icons — no manual step remains.
  Startup files without a Torvik entry are left byte-for-byte untouched.

### Errors & diagnostics

- **Every codegen error is now a located error.** The 128-bit type errors, ternary errors,
  string/number comparison errors, membership and weave errors, arity errors, and the
  leftover-operator diagnostic all now report with the standard file:line:column, source
  line, and caret — and a file with several problems reports them ALL in one compile instead
  of stopping at the first. (Previously these printed a message without a location and
  aborted immediately.)

### Fixes

- **128-bit integers now work on Windows.** `__int128` was passed and returned by value across
  the runtime boundary, which the Win64 ABI lowers incompatibly with the generated IR — every
  i128/u128 operation faulted on Windows. All 128-bit values now travel by pointer (box, load,
  to-string, divide, modulo), which is correct on every target. i128/u128 is fully working on
  Linux and Windows alike.
- **Negative 128-bit literals now work.** `set a: i128 = -5;` (and any negative i128 literal,
  down to the true minimum -2^127) previously fell through to the i64-only unary-minus path and
  crashed; negative literals now box correctly, including when pushed or inserted into a
  `list<i128>`. A negative literal for an unsigned `u128` is now a clean range error instead of
  a crash.
- **128-bit list elements now work in expressions.** Arithmetic and comparison directly on a
  `list<i128>`/`list<u128>` element (`xs[0] + xs[1]`, `xs[i] > v`) previously required binding
  the element to a variable first; it now folds natively like any other 128-bit operand.
  Integer literals can also be pushed/inserted straight into a 128-bit list (`push(xs, 42)`,
  `list_insert(xs, 0, 99)`) with range checking, instead of erroring.
- **Fixed a use-after-free when reassigning a 128-bit variable.** `t = t + 1` (and any
  self-referential reassignment of an `i128`/`u128`, common when accumulating in a loop) freed
  the value's box while it was still live. The reassignment path now retains the new box and
  releases the old one like other reference-counted types. ASAN-clean over stress runs.
- **`rune run` failed on Windows with "'build' is not recognized".** The command that launched
  the built binary used a forward-slash path (`build/test.exe`), which cmd.exe reads as a
  command plus a `/test.exe` switch. The run path now uses backslashes and a `.\` prefix on
  Windows so it's unambiguously an executable path.
- **Garbled dash in `rune version` / `rune help` on Windows.** Printed output used a Unicode
  em-dash that the default Windows console codepage rendered as `ΓÇö`. User-facing messages now
  use a plain ASCII hyphen.
- **`rune` messages showed the Linux install path on Windows.** `rune help`, `rune uninstall`,
  and related messages hard-coded `~/.torvik`; they now show `%USERPROFILE%\.torvik` on Windows.
- **New-project version starts at `0.1.0`** (was `0.0.1`), a more conventional initial version.
- **`args_get` could return empty or corrupted strings.** A command-line argument read with
  `args_get` was returned as a borrowed pointer into the OS argv rather than an ARC-tracked
  string, so once the result flowed through further calls (or other string temporaries were in
  play) its memory could be reused — producing empty or garbled reads. `args_get` now returns a
  fresh owned copy, so arguments read reliably no matter how they're used. This affected any
  program parsing flags/arguments, including `rune`'s own option handling.
- **`fmt` printed a pointer address for a string list element.** `fmt("{}", xs[0])` on a
  `list<str>` stringified the element's pointer as a number. Index reads of string elements
  now carry their type, so `fmt`, concatenation, and comparisons all treat them as strings.
- **`len(a) + 1 != len(b)` mis-folded.** The old `len()` arithmetic special-case compiled the
  entire rest of the expression as one greedy operand, mis-parsing mixed arithmetic and
  comparison (and producing invalid IR). `len()` results now ride the standard chain folder
  with correct precedence.

- **Cascading phantom errors after a declaration error are gone.** The error-recovery paths
  mis-tracked string literals (they live in two internal streams), so any recovered error
  followed by a string literal garbled every later message in the file. All recovery paths
  now keep the streams aligned: each real error reports once, correctly.
- **`readenv` of an unset variable no longer crashes.** The runtime returned a null pointer
  for an environment variable that wasn't set, which segfaulted on first use. `readenv` now
  returns `""` when the variable is not set.

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
