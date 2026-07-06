# Torvik Roadmap


## v1.1.0 — current release

Everything in this section has landed. The items below were the v1.0 rough edges that v1.1.0
smooths over, plus the new capabilities the release adds.

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

## v1.2.0 — planned

- **`task`** — async / concurrent tasks. (Moved from v1.1.0 to give it the room it needs.)
- **Warnings system** — general compile-time warnings (unused variables/imports, unreachable
  code), plus **deprecation warnings** for code scheduled for removal. (A first piece shipped
  in v1.1.0 as a hard error: redeclaring a variable with a different type in the same
  function — previously silent memory corruption.)
- **Official macOS support** — macOS has no prebuilt binaries today and is not yet supported;
  v1.2.0 brings full testing and official support (installer, file-type/icon integration, CI coverage).
- **Windows `.tv` file-type & icon integration** — the icon assets already ship
  (`assets/torvik-file.ico`), but the Windows installer does not yet register the `.tv` file
  association or icon in the registry the way the Linux installer does via freedesktop. Add
  that registration to `windows/install.ps1` (and matching cleanup on uninstall) so `.tv`
  files show the Torvik icon on Windows.
- **Result types** — `ok` / `err` / `result<T>` for explicit error handling. The runtime
  already ships the `torvik_result_*` family (`try_readfile`, `try_toint`, `try_tofloat`,
  unwrap/unwrap_or, error messages), so the remaining work is the language surface and codegen.

---

## Future versions — planned, not yet scheduled

Features that WILL come to Torvik but are not tied to a version yet. They moved out of the
v1.1.0 plan so that v1.1.0 — which carries important fixes for v1.0 users — can ship sooner.

### Language & types

- **`shape`** — structs / record types.
- **`when`** — pattern matching.
- **`enum`** — enumerations.
- **`pub`** — visibility / export control.
- **`f32`**, a dedicated **`char`** type, and **fixed-size arrays** (`[T; N]`).
- **`\u{...}` Unicode escapes** — with the scanner alignment that requires.

### Standard library & tooling

- **Methods** — method-style calls in the standard library (e.g. `s.str_cmp(t)`).
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
