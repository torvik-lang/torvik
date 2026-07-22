# Torvik Roadmap


## v1.4.0 — current public release

- **Optional (`^`) and variadic (`*`) parameters.** Mark a trailing parameter `^` to let
  callers omit it (it takes a type-appropriate zero), or `*` to gather the remaining
  arguments into a `list<T>`. They combine in the order required → optional → variadic, and
  arity errors are clean located ranges.
- **Comprehensive argument type-checking.** A definite mismatch between an argument and its
  parameter (string vs number, container vs scalar, `f64` vs integer, str-list vs raw-list)
  is now a clean located error instead of silent garbage or a crash.
- **Boolean chaining is limit-free.** `&&` / `||` work as values bound to a variable, chain
  any number of operands, and a boolean-returning builtin or user function can be compared
  to `true`/`false` directly — the last of the old "bind to a variable first" rough edges.
- **`std::net` — a minimal HTTP layer.** `apply std::net;` adds request parsing, MIME types,
  and binary-safe file serving over transport primitives (`net_listen`/`net_accept`/
  `net_recv`/`net_send`/`net_send_file`/`net_close`) that dead-strip out when unused. Powers
  Vefna's `vefna serve`.
- **`chr(code)` and `fs_size(path)` builtins**; **standard library v1.3.0**.
- **`rune` decoupled** into its own repository, versioned independently (still installed with
  the toolchain).
- Fixes: exhaustive `when` flow-checker false positive; boolean-literal argument IR; `bool`
  user-function return storage; single-character function-name resolution under `apply std`.

---


## v1.3.0

- **File-system surface for real tools** — `dir_list(path)` (sorted directory entry names
  as `list<str>`), `fs_is_dir(path)`, and `fs_copy(src, dst)` (binary-safe, so images and
  other assets round-trip exactly); `fs_mkdir` documented as the `mkdir -p` it always was.
  Built for the Torvik-showcase static site generator, useful to every tool that walks a tree.
- **Concurrency — ravens and bridges.** `raven` spawns a `df` function as a task on its own
  OS thread (fire-and-forget, or a `task<T>` handle joined with `join`); `bridge<T>` is a
  typed, buffered channel between tasks (`bridge_new` / `send` / `recv` / `try_recv` /
  `bridge_close`). The model is safe by construction: values deep-copy as they cross a thread
  boundary, so tasks share no mutable state and the reference-counting runtime stays lock-free
  — the one atomic in the design is a bridge's own refcount. A compile-time reachability pass
  enforces that spawned functions are self-contained (touch no globals). Given its own release,
  as promised, because concurrency touches the runtime and deserved the room.
- **Variadic `str_concat`** — folds any number of arguments, and a one-argument call is now a
  clean error instead of silently dropping extras.
- **Interpolation scoped to `echo`/`echo!`/`fmt`** — a string literal anywhere else is plain
  data, so CSS, JSON, and template text sit in ordinary strings with no `{{` escapes. Found
  while building [Vefna](https://github.com/torvik-lang/vefna), fixed before either shipped.
- **Standard library v1.2.0** — additive growth across `std::strings`, `std::list`, and
  `std::math`, plus a new `std::convert` module (base conversions and safe parsing). No
  breaking changes; see [CHANGELOG](CHANGELOG.md).

---


## v1.2.1

A patch release fixing four correctness bugs in v1.2.0; no new features, no breaking
changes for correct programs. **Warnings now actually reach `rune run` users** — `-q`
no longer implies `--no-warn` (warnings are diagnostics, like errors), and rune
captures the compiler's diagnostics and replays them on cached runs until the code is
fixed or a suppressor is used. **Unannotated declarations** (`set v = trim(x);`,
`set b = a;`) previously compiled silently broken — garbage reads and segfaults — and
are now clean located errors (only the documented `set xs = list_new();` inference may
omit an annotation). **Bare statement calls of value-returning builtins** (`trim(s);`)
now get a truthful error instead of "call to undefined function". **Diagnostics under
`apply`** now report the user's real line numbers instead of positions inside the
prepended module text.

## v1.2.0

A language-ergonomics and tooling release with **no breaking changes**, verified by
three-generation self-hosting fixpoints and the full 93-case end-to-end suite on both Linux
and Windows. Highlights: **`aett`** — a family of named values, Torvik's enumeration, named
for the *ættir* the runes are grouped into (`aett Status { Pending, Active, Closed }`,
variants as `Status::Pending`, i64-backed with the aett name as a type annotation and
`typeof` support); **`when`** pattern matching with `=>` arms, `fallback =>` defaults, and
compile-time **exhaustiveness checking** over aetts (missing variants are named in the
error); a **compile warnings system** (unused variables with `_name` opt-out, unreachable
code, a deprecation channel) that never fails a build, with `--no-warn` / `-q` and per-file
`!@NO_WARN;` / `!@ALLOW[category];` directives where a typo'd directive is a clean error;
**Result types completed** (`ok` / `err` constructors, so user functions return
`result<i64|str|f64>`); the **`find(s, sub)`** builtin; **standard-library growth** (new
`std::path`; `sort` / `sort_str` / `reverse_list` / `index_of` in `std::list`; `sign` /
`isqrt`; `count_str` / `reverse_str` / `capitalize`) under a new **independent std
versioning** policy (std moves to 1.1.0, projects can gate with `std = "x.y.z"` in
`torvik.rune`); and **Windows `.tv` file-type & icon registration** (per-user, removed by
`rune uninstall`).

## v1.1.3

Stability release: a full compiler-wide audit for silent wrong answers, silent no-ops, and
compile-then-crash holes, backed by a new 78-case end-to-end test suite (in `/dev/tests`,
with Linux and Windows runners) verified on both platforms. Highlights: leading unary minus
precedence corrected everywhere; `fixed` immutability enforced for locals **and** globals;
return types checked; weave stages arity- **and** type-checked; `bag<T>` reference counting
repaired (a use-after-free that crashed on Windows); large-integer printing fixed on Windows
(LLP64 `%ld` truncation); `readkey()` honors redirected stdin on Windows; `apply std;`
repaired on every Windows install; full-range `i128`/`u128` global literals; `~` bitwise NOT,
`upper()`/`lower()` calls, and the zero-arg console builtins implemented as documented; plus
the additive `readkey_str()`. No breaking changes.

## v1.1.2

Patch release on top of v1.1.1. `rune update` robustness (clean HTTP 429 handling, a
version-pinning fix so pinned installs report the correct version, and automatic cleanup of
the Windows self-update `.old` file). No language or compiler changes.

## v1.1.1

Security and reliability patch: `fs_remove` is now symlink-safe (TOCTOU hardening flagged by
CodeQL), Windows builds are reproducible (byte-identical `.exe` from identical source), and the
Windows `rune update` self-update no longer trips over the lock on a running executable.

## v1.1.0

The feature release these patches build on. Everything in this section has landed — the items
below were the v1.0 rough edges that v1.1.0 smooths over, plus the new capabilities the release
adds.

### v1.0 limitations fixed in v1.1.0

Each of these was a clean compile error or a documented narrow case in v1.0, and each is now
resolved.

- **Expression chaining (stage 2).** Allow a value taken directly from a function call or an
  index (e.g. `table_get(...)`, `xs[i]`) to sit next to an arithmetic/comparison operator,
  removing the current "bind to a variable first" requirement. *(landed in the v1.1.0 tree:
  all comparison positions accept any operand shape, strings compare by content with
  lexicographic ordering, mixed arithmetic+comparison folds with correct precedence without
  parentheses, and the ternary's bind-first caveat for call branches is gone. The
  literal-left forms of `<|` and `~>` are unblocked and move to the operator-edges item
  below.)*
- **`u64` non-lead operand signedness.** Signedness is currently taken from the lead operand,
  so a `u64` appearing only as a non-lead operand in a mixed expression uses signed operations.
  *(landed in the v1.1.0 tree: per-operand tracking — a u64 variable, call result, or index
  anywhere in a chain upgrades division/modulo/shifts/comparisons to unsigned from that
  operand onward, and a u64 on EITHER side of a comparison selects unsigned ordering.
  Signed-only expressions are untouched.)*

- **Unary `-` beyond prefix.** v1.0 does prefix negation (`-x`, `set y = -x`). Revisit full
  binary/unary disambiguation so forms like `a - -b` parse cleanly. *(landed in the v1.1.0
  tree: a unary minus may prefix any chain operand — literals, variables, calls, indexes,
  parens — in both the integer and float chains)*
- **`<|` full string-value membership.** v1.0 does integer/identity membership (value compare
  on integer lists; pointer-identity on string lists). Add per-element string comparison so
  `"foo" <| names` matches by string value. *(landed in the v1.1.0 tree: list<str> membership
  is by content, any operand shape may lead `<|`, and item/element type mismatches are clean
  compile errors)*
- **`~>` argument insertion.** v1.0 weaves bare function names (`a ~> trim ~> shout`). Add the
  first-argument-insertion form, so `a ~> replace(",", "")` means `replace(a, ",", "")`.
  *(landed in the v1.1.0 tree: `x ~> f(a, b)` means f(x, a, b) for user functions plus
  replace/substr, with arity checked counting the insertion; any operand shape may lead `~>`;
  weave results are typed so trailing comparisons fold by content)*
- **Ternary short-circuit.** The ternary currently lowers to a hardware `select` (both branches
  evaluated). Move to branch + phi so only the taken branch runs. *(landed in the v1.1.0 tree,
  along with ternary support inside parentheses, in plain/compound reassignment, and in
  indexed stores; function-call branches of ref type still follow the bind-first rule until
  expression chaining stage 2 lands)*
- **Collection element-type inference.** *(landed in the v1.1.0 tree: an unannotated
  `set xs = list_new();` infers its element type from the first `push` — str, integer, or bool.
  `table` inference and `list<f64>` element storage remain open.)*
- **`list_pop` wiring** — *(landed in the v1.1.0 tree: `list_pop(xs)` removes and returns the
  last element, typed by the list's element type, with correct ARC ownership transfer for
  `list<str>`. `list<i128>` pop stays a clean error for now.)*
- **`list<f64>` element storage** — *(landed in the v1.1.0 tree: float elements are stored as
  their bit pattern in the uniform cell and recovered on read, so push, index read, arithmetic,
  comparison, `each`, `list_insert`, `list_pop`, and inference all work on `list<f64>`)*
- **Interpolation expressions** — v1.0 interpolation (`echo!("x is {x}")`, `fmt`) accepted only
  a plain variable name inside the braces. *(landed in the v1.1.0 tree: `{...}` now accepts a
  full expression — arithmetic, calls, indexing, and combinations — compiled with the usual
  typing and precedence; a bare `{name}` stays the fast path)*

### Language features

- **Block comments** — alongside the existing `//` line comments. *(landed in the v1.1.0
  tree as nestable `#- ... -#`)*
- **Inclusive ranges** — landed in the v1.1.0 tree as `START..+END`.
- **Direct collection iteration** — landed in the v1.1.0 tree: `each x in xs { ... }` binds
  the loop variable to each element, typed as the list's element type (list<str> elements are
  ARC-managed correctly across the loop).

- **System introspection builtins** — wire the runtime's system-info functions that shipped
  unwired in v1.0: `sys_os_name` / `sys_os_version` / `sys_arch` / `sys_hostname` /
  `sys_username` (str), `sys_cpu_count` / `sys_mem_total` / `sys_mem_free` / `sys_pid` (i64),
  plus `appendline(path, line)` for file appends and compile-time `typeof(x)`. *(landed in
  the v1.1.0 tree, fully chainable and documented in STDLIB)*
- **`rune update` — toolchain self-update.** *(landed in the v1.1.0 tree: `rune update`
  re-runs the official installer for the current platform — Linux/macOS shell or Windows
  PowerShell — refreshing torvc, rune, the runtime, and the standard library in place.
  Dependency management via the Rune Library remains a future item.)*
- **`rune uninstall` full cleanup** — automatically remove the Torvik PATH entry from the
  user's shell startup files and deregister the `.tv` file type / icons, so no manual step
  remains after uninstalling. *(landed in the v1.1.0 tree)*

### Runtime & platform

- **Full Windows support** — *(landed in the v1.1.0 tree: torvc.exe and rune.exe build and run
  on Windows x86-64; the C runtime carries a Win32 platform layer for terminal input, process
  id, wall-clock time, OS/arch/host/memory info, running commands, and directory walks. Native
  file-type / icon integration is a follow-up; the core toolchain is complete.)*

---

## Future versions — planned, not yet scheduled

Features that WILL come to Torvik but are not tied to a version yet. They moved out of the
v1.1.0 plan so that v1.1.0 — which carries important fixes for v1.0 users — can ship sooner.

### Platforms

- **Official macOS support** — macOS has no prebuilt binaries today and is not yet
  supported. Official support needs real Apple hardware for testing (Apple Silicon ABI,
  Mach-O linking, codesigning) — blind cross-compilation isn't a credible basis for
  "supported". The port is scheduled once real Apple hardware access is sorted.

### Language & types

- **`shape`** — structs / record types.
- **`pub`** — visibility / export control.
- **`f32`**, a dedicated **`char`** type, and **fixed-size arrays** (`[T; N]`).
- **`\u{...}` Unicode escapes** — with the scanner alignment that requires.

### Standard library & tooling

- **Methods** — method-style calls in the standard library (e.g. `s.str_cmp(t)`). The
  standard library itself keeps growing in the meantime as plain functions (v1.3.0 added a
  batch across strings, lists, math, and a new `convert` module); method *syntax* is the
  future addition.
- **Rune Library** — the package registry: external dependencies via the `[runes]` section of
  `torvik.rune` (`rune add` / `remove` / `update`). This is a substantial project on its own —
  registry infrastructure (website, storage, hosting) comes with real costs, so its timeline
  depends on planning and sponsorship. How it will be hosted (possibly through GitHub itself)
  will be determined closer to the build.

### Memory & runtime

- **Cycle detector** — detect (and reclaim) reference cycles, ahead of constructs (nested
  mutable containers) that can create them.
- **C → Torvik runtime port** — migrate the C runtime to Torvik over time.

---

*This roadmap supersedes the short "Roadmap & limitations" summary in
[docs/GUIDE.md](docs/GUIDE.md#roadmap--limitations); that section stays as a quick user-facing
overview.*
