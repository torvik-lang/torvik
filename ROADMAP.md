# Torvik Roadmap


## v1.1.0 — planned

### Known v1.0 limitations to fix

These are the rough edges in v1.0. Each is a clean compile error or a documented narrow case
today and each is slated to be smoothed over in v1.1.0.

- **Expression chaining (stage 2).** Allow a value taken directly from a function call or an
  index (e.g. `table_get(...)`, `xs[i]`) to sit next to an arithmetic/comparison operator,
  removing the current "bind to a variable first" requirement. This also unblocks the
  literal-left forms of `<|` and `~>` below.
- **`u64` non-lead operand signedness.** Signedness is currently taken from the lead operand,
  so a `u64` that appears only as a *non-lead* operand in a mixed expression — e.g. `2 / x`
  (literal leads) or `i64var / u64var` — still uses signed operations. Every pure-`u64` form
  (a `u64` variable or call leads) is already correct. The full fix is per-operand type
  tracking inside `fold_chain6`; until then, the rule "lead with the `u64` value" covers
  normal usage.
- **Unary `-` beyond prefix.** v1.0 does prefix negation (`-x`, `set y = -x`). Revisit full
  binary/unary disambiguation so forms like `a - -b` parse cleanly.
- **`<|` full string-value membership.** v1.0 does integer/identity membership (value compare
  on integer lists; pointer-identity on string lists). Add per-element string comparison so
  `"foo" <| names` matches by string value.
- **`~>` argument insertion.** v1.0 weaves bare function names (`a ~> trim ~> shout`). Add the
  first-argument-insertion form, so `a ~> replace(",", "")` means `replace(a, ",", "")`.
- **Ternary short-circuit.** The ternary currently lowers to a hardware `select` (both branches
  evaluated). Move to branch + phi so only the taken branch runs.
- **Collection element-type inference.** Infer `list`/`table` element types instead of
  requiring annotations; this also lets `table_get`/index results carry their value type fully.
- **`list_pop` wiring**, and the remaining smaller
  known limitations, fixed where possible within v1.x.

### Language features

- **`shape`** — structs / record types.
- **`when`** — pattern matching.
- **`task`** — async / concurrent tasks.
- **Result types** — `ok` / `err` / `result<T>`.
- **`enum`** — enumerations.
- **`pub`** — visibility / export control.
- **Block comments** — alongside the existing `//` line comments.
- **Inclusive ranges** (`..=`) and **direct collection iteration** (`each x in xs`).
- **`\u{...}` Unicode escapes** — with the scanner alignment that requires.

### Types

- **`f32`** — 32-bit float.
- **`char`** — a dedicated character type (today, character literals are one-character strings).
- **Fixed-size arrays** — `[T; N]`.

### Standard library & tooling

- **Methods** — method-style calls in the standard library (e.g. `s.str_cmp(t)`).
- **Warnings system** — general compile-time warnings (unused variables/imports, unreachable
  code), plus **deprecation warnings** for code scheduled for removal.
- **Rune Library** — external dependencies via the `[runes]` section of `torvik.rune`
  (`rune add` / `remove` / `update`).

### Memory & safety

- **Cycle detector** — detect (and reclaim) reference cycles, ahead of constructs (nested
  mutable containers) that can create them.

### Runtime & platform

- **C → Torvik runtime port** — migrate the C runtime to Torvik over time.
- **Full Windows support** — including the installer and the file-type / icon integration that
  Linux and macOS get.

---

*This roadmap supersedes the short "Roadmap & limitations" summary in
[docs/GUIDE.md](docs/GUIDE.md#roadmap--limitations); that section stays as a quick user-facing
overview.*
