# Changelog

## [1.3.0] — 2026-07

### Added

- **Table iteration: `table_keys(t)`.** Returns every key currently in a table
  as a sorted `list<str>` (sorted because hash order shifts as a table resizes —
  loops over a table must be reproducible). Iterate the keys and fetch values
  with `table_get`; the captured list is independent of later table mutation.

- **Bridges: typed channels between tasks.** `set ch: bridge<T> = bridge_new(cap);`
  creates a buffered, thread-safe queue (cap ≥ 1); `send(ch, v)` deep-copies the
  value in (blocking while full); `recv(ch)` takes one out (blocking while
  empty); `bridge_close(ch)` says "no more values" (idempotent, wakes everyone).
  A closed-and-drained bridge panics on `recv` and turns into
  `err(0, "bridge closed")` under `try_recv(ch) -> result<T>` — the worker-loop
  primitive, mirroring `try_readfile`. Elements: every integer width, `f64`,
  `bool`, `str`, `i128`/`u128`, and aett values (`try_recv` covers what
  `result<T>` covers today: ints, `f64`, `str`). Multiple producers and
  consumers on one bridge are fully supported.
- **A bridge is the only object two threads share.** Passing a bridge into a
  raven-spawned task passes *the bridge*, retained — the one deliberate
  exception to arguments-are-deep-copied (a private copy of a channel would be
  useless). Every touch of its interior goes through its own lock, and its
  refcount is the single atomic operation in the entire concurrency design;
  element refcounts stay non-atomic because only one thread ever owns an
  element. Bridges live in bindings and flow down through arguments: no bridge
  globals, no bridges in collections, no returning bridges (clean errors).
- **Ravens: concurrent tasks.** `raven f(args);` runs a user function on its own
  OS thread, fire-and-forget; `set h: task<T> = raven f(args);` keeps a handle,
  and `join(h)` blocks until the task finishes and yields its return value
  (composing in expressions like any call: `check join(h) == "done"`). A bare
  `join(h);` statement waits and discards — legal for every task type, releasing
  a managed result so nothing leaks. Every crossable type works: all integer
  widths, `u64`, `f64`, `bool`, `str`, `i128`/`u128`, and aett values;
  `task<void>` tasks join as statements. Each handle joins exactly once (a
  second join is a clean runtime panic). `join` stays context-sensitive:
  `std::list`'s `join(list, sep)` is untouched.
- **Arguments deep-copy at spawn.** The copy is taken synchronously in the spawn
  statement, on the spawner's thread — after that line, the two threads share no
  object, so the existing non-atomic ARC runtime needed zero changes and data
  races are impossible by construction. Collections, results, and task handles
  cannot cross into a task (clean located error naming the parameter);
  `task<...>` cannot nest in collection types or live in a global.
- **The self-contained rule.** A raven-spawned function — and everything it
  transitively calls — may not read *or* write a global (even a read races a
  global string's non-atomic refcount). Enforced at compile time by a
  reachability pass that names the spawn, the offending function, and the
  global; reads through `echo` interpolation (`"{g}"`) are caught too. A raven
  carries everything it needs in its claws.
- **Panics in tasks halt the process** immediately with the usual message —
  never deferred to `join`, never swallowed. Recoverable failure in tasks is
  what `result<T>` returns are for, same as everywhere else.
- Printing a task handle (`echo!(h)` or `"{h}"`) is a clean compile error
  instead of raw pointer garbage.

### Changed

- **String interpolation is now scoped to `echo`, `echo!`, and `fmt`.** A string
  literal written directly as an argument of those three interpolates exactly as
  before (`{name}`, expressions, `{{` escapes). A string literal anywhere else —
  assignments, ordinary call arguments, returns — is now plain data: braces are
  ordinary characters, so CSS, JSON, and template text no longer need `{{`
  escapes (and no longer trip "undefined variable" on things like
  `".a{color:red}"`). This matches what the guide always documented as the
  idiom; code that kept interpolated literals inside `echo`/`fmt` is unaffected.

- **`str_concat` is now variadic.** It folds any number of arguments (two or
  more) left-to-right, so `str_concat(a, b, c, d)` works directly with no
  nesting. Crucially, calling it with **one** argument is now a clean located
  compile error — previously a fixed two-argument form silently dropped any
  extra arguments, which could hide real bugs (a three-argument call quietly
  lost its third piece). Every intermediate is released exactly once
  (ASAN-clean).
- **The lexer's two keyword lists are now one.** `is_keyword()` is derived from
  `keyword_for()` instead of hand-maintained in lockstep, eliminating the
  TVC-1001 cursor-drift class (a word known to one list but not the other) at
  the root. New keywords are added in exactly one place.

### Standard library (v1.1.0 → v1.2.0)

Additive growth across the opt-in `apply std` layer; no breaking changes.

- **`std::strings`** — `strip_prefix`, `strip_suffix`, `is_digits`, `is_alpha`,
  `center`, `truncate`.
- **`std::list`** — `contains_int`, `contains_str_in`, `count_int`, `unique`,
  `take`, `drop`, `mean`.
- **`std::math`** — `pow_checked` (overflow-signalling power, returns
  `result<i64>`), `digit_count`, `at_least`, `at_most`, `ilog2`.
- **`std::convert`** (new module) — `to_hex`, `to_bin`, `from_hex`, `to_int`.
  Base conversions and safe parsing; the parsers return `result<i64>` so bad
  input is recoverable rather than a halt.

### Fixed

- **Functions returning a `table` or `bag` handed back freed memory.** The
  +1-on-return retain covered `str`, `list`, `result`, and 128-bit values but
  not tables or bags, so `fixed cfg: table<str, str> = read_config(path);`
  received a dangling pointer and crashed on first use. Both kinds now retain
  on return and release correctly at the call site (ASAN-verified, including
  discarded results).

### Added

- **File-system surface for real tools**: `dir_list(path)` — the entry names of a
  directory as a `list<str>`, sorted bytewise for deterministic output on every
  platform and filesystem (`.`/`..` excluded; an unopenable path halts with a clean
  message, matching `readfile`); `fs_is_dir(path)`; and `fs_copy(src, dst)` — a
  binary-safe copy (`readfile` is NUL-terminated text, so images could not
  round-trip through it). `fs_mkdir`'s docs now say what it always did: create
  parents like `mkdir -p`. Cross-platform (POSIX dirent / Win32 FindFirstFile),
  ASAN-clean, covered by pos/39_fs_walk.

### Fixed

- **Unannotated declarations no longer compile silently broken.** `set v = trim(x);`
  produced garbage aliasing unrelated memory, and `set b = a;` segfaulted — both now
  clean located errors. The guide has always required type annotations; only the
  documented `set xs = list_new();` inference may omit one.
- **Bare statement calls of value-returning builtins get a truthful error.**
  `trim(s);` as a statement previously claimed "call to undefined function 'trim'";
  it now explains the real situation and shows the fix.
- **Diagnostics under `apply` report the user's real line numbers.** Applied modules
  are prepended to the compilation unit, so an error on the user's line 11 was
  reported as line 337 of their file. Offsets past the prelude now subtract its
  lines; diagnostics inside an applied module say so instead of mislabeling the
  user's file.

- **Warnings now reach `rune run` users.** rune builds with `-q`, and `-q` implied
  `--no-warn` — so the entire warnings system was invisible through rune, the way
  most programs are built. `-q` now suppresses only the informational build output;
  **warnings are diagnostics, like errors, and always show** unless `--no-warn`
  (or a `!@` directive) explicitly silences them. The suite now asserts warnings
  survive `-q` builds.

### Added (warnings & I/O)

- **`unused_result` warning** — a bare statement call of a non-void function
  discards its return value silently; the compiler now says so (bind it, or use an
  `_name` to discard deliberately). New `!@ALLOW[unused_result];` category.
- **Recoverable write-side I/O**: `try_writefile(path, data)`,
  `try_appendline(path, line)`, and `try_fs_copy(src, dst)` — each returns
  `result<i64>` (`ok(0)` on success, an `err` carrying the OS message on failure)
  so a program can inspect and continue instead of halting. Completes the family
  started by `try_readfile`. ASAN-clean.

## [1.2.0]

A language-ergonomics and tooling release.

### Added

- **`aett` — a family of named values.** Torvik's enumeration, named for the *ættir*
  (the families the runes of the futhark are grouped into): `aett Status { Pending,
  Active, Closed }` at the top level, variants accessed as `Status::Pending`. Variants
  are i64 ordinals (0-based, declaration order) — they print, compare, store in lists,
  and pass to functions like integers — while the aett name serves as a type annotation
  for variables, parameters, and return types, and `typeof` reports it. Duplicate
  aetts/variants, empty aetts, unknown variants, and `X::Y` on a non-aett are all clean
  located errors.
- **`when` — pattern matching.** `when value { pattern => statement-or-block; ...;
  fallback => ...; }` with `=>` arms. Patterns are aett variants and integer literals
  (negatives included); `fallback` is the default arm and must be last. An aett-typed
  scrutinee without a `fallback` is checked for **exhaustiveness** — missing variants
  are a compile error naming exactly what's uncovered, so adding a variant later
  surfaces every `when` that needs updating. Non-aett scrutinees require a `fallback`.
  Matching over the wrong aett is a clean error.
- **Compile warnings system.** The compiler now warns (never fails) on code that is
  legal but probably unintended, with the same line-and-caret rendering as errors:
  **unused variables** (including write-only bindings; underscore-prefixed names like
  `_r` are exempt as deliberate discards; loop variables and parameters never flag),
  **unreachable code** (statements after `return`/`break`/`continue`/`halt`/`exit`,
  with the `halt(...); return X;` all-paths-return idiom exempt), and a **deprecation
  channel** (builtins scheduled for removal warn at each call site — empty today).
  Warnings render before errors so an "unreachable code" note can explain a
  "must return a value" error. New `torvc --no-warn` flag; `-q` implies it.
  **File-level `!@` directives**: `!@NO_WARN;` suppresses everything for the
  compilation; `!@ALLOW[category];` suppresses one category and stacks. Typo'd
  directives and unknown categories are clean located compile errors. Directives
  inside string literals are inert.
  Enabling the system on the compiler itself found and removed four dead variables
  in torvc and rune.
- **Result types completed.** `ok(value)` and `err(msg)` / `err(code, msg)`
  constructors mean user functions can now return `result<T>` (`i64`/`str`/`f64`),
  closing the loop with the existing consumers (`is_ok`, `is_err`, `unwrap`,
  `unwrap_or`, `err_msg`, `err_code`, `try_readfile`, `try_toint`, `try_tofloat`).
  Documented in GUIDE.md and STDLIB.md. ASAN-clean.
- **`find(s, sub)`** — byte index of the first occurrence of a substring, `-1` when
  absent. The companion `contains` always told you *whether*; `find` tells you *where*.
- **Version-string-looking literals get a real error.** `0.1.0` is not a number (a
  float has a single decimal point), but it previously produced a confusing cascade of
  parse errors. It's now one clean located error suggesting the fix: quote it.
- **`rune uninstall` now removes itself on Windows.** Windows locks a running
  program's file, so rune.exe couldn't delete itself the way the Linux uninstall
  does — `bin\rune.exe` (and thus `bin\`) was left behind. The uninstall now
  schedules the final sweep: a small batch in `%TEMP%` waits for rune to exit,
  removes the remainder of `~/.torvik`, and deletes itself.
- **Windows `.tv` file-type & icon integration.** The Windows installer now
  registers the `.tv` extension with a friendly type name and the Torvik file
  icon (per-user `HKCU` keys, no admin needed; no open-command is registered —
  source files belong to your editor). `rune uninstall` removes the keys.
- **Standard library grown (std v1.1.0).** New module **`std::path`** (`path_base`,
  `path_dir`, `path_ext`, `path_join` — `/` and `\` both recognized on input);
  **`std::list`** gains in-place `sort` / `sort_str` (stable insertion sort),
  `reverse_list`, `index_of`, `index_of_str`; **`std::math`** gains `sign` and `isqrt`
  (Newton's method, halts on negative input); **`std::strings`** gains `count_str`,
  `reverse_str`, and `capitalize`.
- **The standard library is now meaningfully versioned.** std carries its own semver
  (the `std` line in VERSION): additive growth bumps its minor, breaking changes bump
  its major — decoupled from the compiler's version. `torvc --version` and
  `rune version` report it, and a project can require a minimum with `std = "x.y.z"`
  in `torvik.rune` — a too-old installation is a clean build error pointing at
  `rune update`. This release demonstrates the policy: std moves 1.0.0 → 1.1.0
  while remaining fully backward compatible.

## [1.1.3] — 2026-07

Stability release: a full compiler-wide sweep for silent wrong answers, silent
no-ops, and compile-then-crash holes, driven by a new 72-case end-to-end test
suite (included under `/dev`). No new language features beyond making documented
behavior real.

### Silent wrong answers fixed

- **Leading unary minus precedence.** `-m + 2` compiled as `-(m + 2)` (gave `-7`
  for m=5 instead of `-3`). A leading `-` now consumes exactly one value, in every
  position (echo, declarations, conditions, chains). Negation is also type-aware:
  `-x` on `f64` and `i128` now works (previously invalid IR or a segfault), and
  negating a `str`, `list`, `bool`, or `u128` is a clean error.
- **`fixed` is now enforced.** Reassigning a `fixed` binding (plain or compound,
  any type) previously compiled and mutated silently; it is now the compile error
  the guide always documented. Enforcement covers both function locals and
  top-level globals — a `fixed` global reassigned from any function is the same
  clean error.
  Enabling this surfaced 131 latent `fixed`-then-mutated local bindings inside
  torvc, rune, and the compiler's own passes — all converted to `set` (behavior
  identical, intent now truthful).
- **Return-type checking.** `df f() -> str { return 5; }` compiled and crashed at
  run time (and `-> i64 { return "x"; }` printed a pointer). A definite kind
  mismatch between the returned value and the declared return type is now a
  located compile error.
- **Weave arity on bare stages.** `5 ~> addn` where `addn` takes two arguments
  compiled and called it with one (garbage second argument). Bare stages now get
  the same arity check as the `x ~> f(a, b)` form.
- **`list<u64>` element leading an expression** used signed division/modulo
  (`us[0] / 2` on a full-range value went negative). Element leads now select
  unsigned ops, matching u64 variables and call results.
- **Bool printing.** `echo!(true)` printed `1` (variables printed `true`);
  `tostr(true)` returned `"1"`; `fs_exists(p)` echoed `1/0`. All bool values now
  print `true`/`false` consistently.
- **Global `u64` literals** lost their annotation (recorded as `i64`), so a
  full-range global printed as a negative signed value. They now initialize
  through the same startup path as other sized integers.
- **Weave stages are type-checked.** A definite mismatch between the woven
  value and a stage's first parameter (`5 ~> shout` where `shout` takes `str`,
  or a `str` woven into an integer stage) is now a clean compile error -
  previously it compiled and crashed or produced garbage at run time.
- **`int_to_str` (and whole-list printing) truncated large values on Windows.**
  The formatter used `%ld` with a `long` cast; `long` is 32-bit on Win64, so
  `-9223372036854775807` printed as `1` and `u32` max as `-1`. Linux was
  unaffected (`long` is 64-bit there). Now `%lld` throughout, and the
  membership helpers' return types now match their declared i64 ABI.

### Crashes fixed

- **Integer-returning weave inside `echo!()`** segfaulted — including the
  guide's own `echo!(v ~> addn(3) ~> addn(10))` example. Weave results now carry
  their full return type (int widths, u64, f64, bool, 128-bit) end to end.
- **`list<bool>` element reads** (`bs[0]`, annotated or inferred) segfaulted.
- **`set c: bool = true;`** could silently drop the store (the declaration's
  store dispatch had no branch for a bool-tagged literal), leaving the variable
  uninitialized — ternaries on such variables picked arbitrary branches.
- **`bag<T>` declarations skipped reference counting entirely** — the kind
  check used exact equality (`== "bag"`) where lists and tables use a prefix
  match, so a parameterized `bag<str>` annotation never matched. The freshly
  created bag was released at end-of-statement and every later `bag_add` /
  `bag_has` was a use-after-free: glibc happened to tolerate the dangling
  reads on Linux, while the Windows heap access-violated (0xC0000005).
  Confirmed and re-verified with AddressSanitizer.
- **`readkey()` ignored redirected stdin on Windows.** `_getch()` reads the
  console device, so with piped input (files, test harnesses, `cmd <input`)
  it blocked waiting for a physical keypress. It now uses the console only
  when stdin *is* the console, and reads the redirected stream otherwise.

### Silent no-ops fixed

- **`torvc file.tv` without `-o`** printed "Compiled successfully!" but wrote the
  binary to `~/tv_out` instead of the documented derived name. The output is now
  `./file` (`file.exe` on Windows), derived from the source name.

### Windows install fixed

- **`apply std;` failed on every Windows install** ("unknown module 'std'"):
  both the public installer (`install.ps1`) and the maintainer setup script
  copied the `std/` submodules but never the umbrella `src/std.tv`, so
  `apply std::math;` worked while `apply std;` did not. Both scripts now
  install the same library set as their Linux counterparts.

### Documented-but-missing implemented

- **`~` (bitwise NOT)** — in the operator table since v1.0, previously a parse
  error. Now implemented for integers, with the same one-value prefix binding as
  unary minus.
- **`upper(s)` / `lower(s)` as ordinary calls** — previously they existed only as
  weave stages; a direct call was "undefined function".
- **Global `i128`/`u128` literals** now work at any magnitude — including
  `u128` max and `i128` min — heap-boxed at startup exactly like local
  declarations, with clean range errors. Expression initializers for 128-bit
  globals remain a single-literal limitation (a clean error explains the
  workaround).
- **Console builtins per STDLIB.md:** `read()`, `readint()`, `readfloat()`, and
  `readbool()` now work with zero arguments (an optional prompt argument is still
  accepted). `readkey()` keeps its i64 byte-code return unchanged — STDLIB.md
  previously misdocumented it as `str` (that form never compiled in any release,
  so no working code is affected); the docs now describe the real i64 return,
  and the new additive `readkey_str()` returns the keypress as a 1-char string
  for code that wants the documented-style form. Not a breaking change.

### Known limitations (documented)

- 128-bit globals accept a single literal initializer; expressions in a
  128-bit global initializer are a clean compile error (compute in a
  function instead).

### Internal

- New runtime helper `torvik_byte_str` (backs `readkey_str`), registered in the
  ARC temp-cleanup pass. Full 78-case suite is ASAN-clean on Linux;
  three-generation self-hosting fixpoint verified (gen2 == gen3,
  byte-identical) and confirmed independently on Windows.

## [1.1.2] — 2026-07

Patch release: `rune update` robustness and a version-pinning fix. No language or
compiler changes.

### Fixes

- **Pinned installs/downgrades now report the correct version.** When installing a
  specific version (e.g. `rune update v1.1.0`), the binaries came from the pinned
  release but the `VERSION` marker, runtime, and standard library were still
  fetched from `main` — so `rune version` could report the wrong version after a
  downgrade. The installer now pulls all version-specific content from the
  matching release **tag**, so binaries and version marker always agree.
- **`rune update` gives a clean message when GitHub rate-limits (HTTP 429).**
  Previously a rate-limited download surfaced as a raw PowerShell/curl exception
  dump. `rune update` now detects a 429 and prints a short, clear "try again in a
  few minutes" message on both Windows and Linux.
- **Windows: the leftover `rune.exe.old` from a self-update is cleaned up.** After
  `rune update` renames the running `rune.exe` aside, the next `rune` invocation
  removes the stale `.old` file automatically.


## [1.1.1] — 2026-07

Patch release: security hardening and Windows build reproducibility. No language
changes.

### Security

- **`fs_remove` (recursive delete) is now symlink-safe.** The recursive directory
  removal behind `rune clean` re-resolved each path between checking its type and
  acting on it — a time-of-check/time-of-use (TOCTOU) pattern flagged by CodeQL.
  On POSIX it now recurses over directory file descriptors using `openat` /
  `fstatat` / `unlinkat` with `AT_SYMLINK_NOFOLLOW` and `O_NOFOLLOW`; on Windows
  it skips reparse points (symlinks / junctions) rather than following them. A
  symlink inside a deleted tree no longer causes anything outside the tree to be
  removed.

### Fixes

- **Reproducible Windows builds.** `torvc` links with `--no-insert-timestamp` on
  Windows, so identical source produces byte-identical `.exe` files. This also
  fixes the Windows self-hosting fixpoint falsely reporting "binaries differ" when
  the IR was identical (the only difference had been the PE header timestamp).
- **`rune update` self-update on Windows** no longer fails with a misleading "no
  build" error. Windows can't overwrite a running executable, and `rune update` is
  `rune.exe` updating itself; the installer now downloads to temp files and
  renames the running binary aside before swapping in the new one.
- Remaining em-dashes in the Windows installer/maintainer scripts (which garbled
  in the default console codepage) are replaced with ASCII hyphens.

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
