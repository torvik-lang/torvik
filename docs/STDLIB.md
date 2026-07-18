# Torvik Standard Library Reference

Torvik gives you functions in two layers:

1. **Core builtins** — built into the compiler and **always available**, with no import. They
   make up the bulk of this reference and are grouped by area below.
2. **The standard library** — an **opt-in** layer you bring in with
   [`apply std`](#the-standard-library-apply-std), covering higher-level math, string, and
   list helpers.

A note that applies throughout: when a function **returns** a value that you want to use
directly inside a larger arithmetic or comparison expression, bind it to a variable first.
For example, write `set n: i64 = toint("7"); echo!(n + 1);` rather than `echo!(toint("7") + 1);`.
This is the expression-chaining limitation described in [the guide](GUIDE.md#roadmap--limitations);
it is reported as a clean compile error, never a crash.

---

## Strings

| Function | Returns | Description |
|----------|---------|-------------|
| `len(s)` | `i64` | Length of a string (also works on lists) |
| `str_concat(a, b, ...)` | `str` | Concatenate two **or more** strings, left to right (v1.3.0: variadic; one argument is a compile error) |
| `substr(s, start, end)` | `str` | Substring from `start` up to (not including) `end` |
| `char_at(s, i)` | `str` | The character at index `i`, as a one-character string |
| `byte_at(s, i)` | `i64` | The raw byte value at index `i` |
| `trim(s)` | `str` | Remove leading and trailing whitespace |
| `triml(s)` | `str` | Remove leading whitespace |
| `trimr(s)` | `str` | Remove trailing whitespace |
| `upper(s)` | `str` | Uppercase copy of `s` (also usable as a weave stage: `s ~> upper`) |
| `lower(s)` | `str` | Lowercase copy of `s` (also usable as a weave stage: `s ~> lower`) |
| `replace(s, old, new)` | `str` | Replace occurrences of `old` with `new` |
| `contains(s, sub)` | `bool` | Whether `s` contains `sub` |
| `find(s, sub)` | `i64` | Byte index of the first occurrence of `sub` in `s`, or `-1` |
| `starts(s, prefix)` | `bool` | Whether `s` starts with `prefix` |
| `ends(s, suffix)` | `bool` | Whether `s` ends with `suffix` |
| `split(s, sep)` | `list<str>` | Split `s` on `sep` into a list of pieces |
| `fmt(template, ...)` | `str` | Build a string with named `{var}` or positional `{}` interpolation. For printing, `echo` interpolates directly — see [the guide](GUIDE.md#strings-and-interpolation) |

```torvik
echo!(str_concat("Hello, ", "Torvik"));            // Hello, Torvik
echo!(str_concat(key, " = ", value, "\n"));        // any number of parts - no nesting
echo!(substr("hello", 0, 4));             // hell
echo!(replace("a.b.c", ".", "/"));        // a/b/c
set parts: list<str> = split("x,y,z", ","); // ["x", "y", "z"]
```

---

## Conversions

| Function | Returns | Description |
|----------|---------|-------------|
| `toint(s)` | `i64` | Parse a string to an integer |
| `tofloat(s)` | `f64` | Parse a string to a float |
| `tostr(x)` | `str` | Convert a value to its string form |
| `int_to_str(n)` | `str` | Integer to string |
| `float_to_str(f)` | `str` | Float to string |
| `try_toint(s)` | — | Parse to integer, signalling failure rather than aborting |
| `try_tofloat(s)` | — | Parse to float, signalling failure rather than aborting |

```torvik
set n: i64 = toint("42");
echo!(tostr(99));    // 99
```

---

## Math

| Function | Returns | Description |
|----------|---------|-------------|
| `abs(n)` | `i64` | Absolute value |
| `min(a, b)` | `i64` | Smaller of two values |
| `max(a, b)` | `i64` | Larger of two values |
| `randint(lo, hi)` | `i64` | Random integer in `[lo, hi]` |
| `rand()` | — | Random value |

```torvik
echo!(abs(-9));        // 9
echo!(max(2, 8));      // 8
set r: i64 = randint(1, 6); // a dice roll
```

---

## Lists — `list<T>`

| Function | Returns | Description |
|----------|---------|-------------|
| `list_new()` | `list<T>` | Create an empty list |
| `push(xs, x)` | — | Append `x` to the list |
| `list_insert(xs, i, x)` | — | Insert `x` at index `i` |
| `list_remove(xs, i)` | — | Remove the element at index `i` |
| `list_pop(xs)` | element | Remove and return the last element |
| `len(xs)` | `i64` | Number of elements |
| `xs[i]` | element | Index access (operator, not a function) |

> `list_pop` returns the removed element, typed as the list's element type — `fixed v: str =
> list_pop(words);`. Popping an empty list is a clean runtime panic. (Popping a `list<i128>`
> is not yet wired; read the last element into an i128 variable and `list_remove` instead.)

```torvik
set xs: list<i64> = list_new();
push(xs, 10);
push(xs, 20);
echo!(xs[1]);          // 20
echo!(len(xs));        // 2
```

---

## Tables — `table<K, V>`

| Function | Returns | Description |
|----------|---------|-------------|
| `table_new()` | `table<K,V>` | Create an empty table |
| `table_set(t, k, v)` | — | Set key `k` to value `v` |
| `table_get(t, k)` | `V` | Value for key `k` (a zero value if absent) |
| `table_has(t, k)` | `bool` | Whether key `k` is present |
| `table_del(t, k)` | — | Remove key `k` |
| `table_len(t)` | `i64` | Number of entries |
| `table_keys(t)` | `list<str>` | Every key, sorted — iterate these and fetch values with `table_get` (v1.3.0) |

```torvik
set scores: table<str, i64> = table_new();
table_set(scores, "Freya", 10);
echo!(table_get(scores, "Freya"));  // 10
echo!(table_has(scores, "Loki"));   // (a bool)
```

---

## Bags — `bag<T>`

| Function | Returns | Description |
|----------|---------|-------------|
| `bag_new()` | `bag<T>` | Create an empty bag |
| `bag_add(b, x)` | — | Add `x` |
| `bag_has(b, x)` | `bool` | Whether `x` is present |
| `bag_remove(b, x)` | — | Remove `x` |
| `bag_len(b)` | `i64` | Number of elements |

---

## Console input

These read from standard input.

| Function | Returns | Description |
|----------|---------|-------------|
| `read()` / `read(prompt)` | `str` | Read input; the optional prompt is printed first |
| `readln()` | `str` | Read a line |
| `readint()` / `readint(prompt)` | `i64` | Read and parse an integer (re-prompts until valid) |
| `readfloat()` / `readfloat(prompt)` | `f64` | Read and parse a float (re-prompts until valid) |
| `readbool()` / `readbool(prompt)` | `bool` | Read and parse a boolean (re-prompts until valid) |
| `readkey()` | `i64` | Read a single keypress as its raw byte code (no Enter needed); pairs with `byte_at` |
| `readkey_str()` | `str` | Read a single keypress as a 1-char string (no Enter needed); pairs with `char_at` |
| `readenv(name)` | `str` | Read an environment variable |

```torvik
echo("What is your name? ");
fixed name: str = readln();
echo!("Hello, {name}!");
```

---

## Files

| Function | Returns | Description |
|----------|---------|-------------|
| `readfile(path)` | `str` | Read a file's contents |
| `try_readfile(path)` | `result<str>` | Read a file; failure is an `err` you can inspect instead of a halt |
| `writefile(path, content)` | — | Write a string to a file |
| `appendline(path, line)` | — | Append a line (with newline) to a file |
| `fs_exists(path)` | `bool` | Whether a path exists |
| `fs_is_dir(path)` | `bool` | Whether a path exists **and** is a directory |
| `dir_list(path)` | `list<str>` | Entry names in a directory, sorted bytewise (`.`/`..` excluded); an unopenable path halts with a clean message, like `readfile`. Bind before iterating: `fixed xs: list<str> = dir_list(p); each e in xs { ... }` |
| `fs_mkdir(path)` | — | Create a directory, parents included (like `mkdir -p`) |
| `fs_copy(src, dst)` | — | Binary-safe file copy — images and other non-text assets round-trip exactly (`readfile` is text) |
| `fs_mtime(path)` | `i64` | Modification time |
| `fs_remove(path)` | — | Remove a file or directory |

## Results (`result<T>`)

Explicit error handling without exceptions: a `result<T>` is either an **ok** carrying a
`T`, or an **err** carrying a message and an optional numeric code. `result<i64>`,
`result<str>`, and `result<f64>` are supported. See [the guide](GUIDE.md#error-handling-resultt-ok-and-err)
for the full walkthrough.

| Function | Returns | Description |
|----------|---------|-------------|
| `ok(value)` | `result<T>` | Construct a success carrying `value` |
| `err(msg)` | `result<T>` | Construct a failure with message `msg` (code 1) |
| `err(code, msg)` | `result<T>` | Construct a failure with a numeric code and message |
| `is_ok(r)` | `bool` | Whether `r` is a success |
| `is_err(r)` | `bool` | Whether `r` is a failure |
| `unwrap(r)` | `T` | The value; halts with the error message if `r` is an err |
| `unwrap_or(r, default)` | `T` | The value, or `default` if `r` is an err |
| `err_msg(r)` | `str` | The failure message (`""` for an ok) |
| `err_code(r)` | `i64` | The failure code (`0` for an ok) |
| `try_readfile(path)` | `result<str>` | `readfile` that signals failure instead of halting |
| `try_writefile(path, data)` | `result<i64>` | `writefile` that signals failure (an `err` with the OS message) instead of halting; `ok(0)` on success |
| `try_appendline(path, line)` | `result<i64>` | `appendline`, recoverable |
| `try_fs_copy(src, dst)` | `result<i64>` | `fs_copy`, recoverable |
| `try_toint(s)` | `result<i64>` | `toint` that signals failure instead of halting |
| `try_tofloat(s)` | `result<f64>` | `tofloat` that signals failure instead of halting |

```torvik
writefile("/tmp/note.txt", "skål");
check fs_exists("/tmp/note.txt") {
    echo!(readfile("/tmp/note.txt"));
}
```

---

## System

| Function | Returns | Description |
|----------|---------|-------------|
| `exit(code)` | — | Exit the program with a status code |
| `sys_run(cmd)` | `i64` | Run a shell command; returns its exit code |
| `sys_home_dir()` | `str` | The user's home directory |
| `cwd()` | `str` | The current working directory |
| `args()` | `i64` | Number of command-line arguments (including the program name) |
| `args_get(i)` | `str` | The `i`-th command-line argument (index 0 is the program) |
| `sleep(ms)` | — | Pause for a number of milliseconds |
| `clear_screen()` | — | Clear the terminal |
| `sys_os_name()` | `str` | Operating system name (e.g. `linux`, `macos`) |
| `sys_os_version()` | `str` | Kernel/OS release string |
| `sys_arch()` | `str` | CPU architecture (e.g. `x86_64`, `aarch64`) |
| `sys_hostname()` | `str` | The machine's hostname |
| `sys_username()` | `str` | The current user's name |
| `sys_cpu_count()` | `i64` | Number of online CPU cores |
| `sys_mem_total()` | `i64` | Total system memory, in bytes |
| `sys_mem_free()` | `i64` | Free system memory, in bytes |
| `sys_pid()` | `i64` | The current process ID |
| `typeof(x)` | `str` | The type of a variable or literal, as a string — resolved at **compile time** (e.g. `"i64"`, `"str"`, `"list<str>"`) |

```torvik
set count: i64 = args();
check count > 1 {
    fixed first: str = args_get(1);
    echo!("first argument: {first}");
}
fixed code: i64 = sys_run("ls -la");
fixed osn: str = sys_os_name();
fixed cores: i64 = sys_cpu_count();
check osn == "linux" && cores >= 4 {
    echo!("linux with {cores} cores");
}
set n: i64 = 5;
check typeof(n) == "i64" { echo!("n is an i64"); }
```

> `rand()` is seeded automatically at program start — there is no seed call to make.

---

## Time

| Function | Returns | Description |
|----------|---------|-------------|
| `time_ms()` | `i64` | Milliseconds since the epoch |
| `time_now()` | — | Current time |
| `time_str()` | `str` | Current time as a string |
| `date_str()` | `str` | Current date as a string |
| `datetime_str()` | `str` | Current date and time as a string |

```torvik
set start: i64 = time_ms();
// ... work ...
set elapsed: i64 = time_ms();   // bind, then subtract: elapsed - start
echo!(datetime_str());
```

---

## The standard library (`apply std`)

Everything above is a core builtin, always available. The functions below are the **opt-in
standard library**: bring them in with `apply std;` (all of it) or per-module with
`apply std::math;`, `apply std::strings;`, `apply std::list;`, or `apply std::path;`. The
library is installed at `~/.torvik/lib/` and versioned independently of the compiler (the
`std` key in `VERSION`). Set `std = no_std` in a project's `torvik.rune` to opt out entirely.

**Standard-library versioning.** std has its own semver, separate from the compiler's:
additive growth bumps its minor version, a breaking change bumps its major — without the
compiler having to move. The installed version always ships with the toolchain
(`torvc --version` and `rune version` both report it), and a project can require a minimum
with `std = "1.1.0"` in `torvik.rune`; a too-old installation is a clean build error that
says to run `rune update`.

### `std::math`

| Function | Returns | Description |
|----------|---------|-------------|
| `pow(base, exp)` | `i64` | `base` raised to the power `exp` |
| `gcd(a, b)` | `i64` | Greatest common divisor |
| `lcm(a, b)` | `i64` | Least common multiple |
| `clamp(x, lo, hi)` | `i64` | Constrain `x` to the range `[lo, hi]` |
| `factorial(n)` | `i64` | `n!` |
| `is_even(n)` | `i64` | `1` if `n` is even, else `0` |
| `is_odd(n)` | `i64` | `1` if `n` is odd, else `0` |
| `sign(n)` | `i64` | `-1`, `0`, or `1` by the sign of `n` |
| `isqrt(n)` | `i64` | Integer square root: largest `r` with `r*r <= n` (halts on negative input) |
| `pow_checked(base, exp)` | `result<i64>` | `base**exp`, or `err` if it would overflow i64 or `exp < 0` |
| `digit_count(n)` | `i64` | Number of base-10 digits in `n` (sign ignored; `0` has `1`) |
| `at_least(n, lo)` | `i64` | `n` raised to at least `lo` (one-sided clamp) |
| `at_most(n, hi)` | `i64` | `n` lowered to at most `hi` (one-sided clamp) |
| `ilog2(n)` | `i64` | Integer base-2 log (highest set bit); `-1` for `n <= 0` |

### `std::strings`

| Function | Returns | Description |
|----------|---------|-------------|
| `join(parts, sep)` | `str` | Join a `list<str>` with a separator |
| `repeat_str(s, n)` | `str` | Repeat `s` `n` times |
| `pad_left(s, width, pad)` | `str` | Left-pad `s` to `width` using `pad` |
| `pad_right(s, width, pad)` | `str` | Right-pad `s` to `width` using `pad` |
| `str_eq(a, b)` | `bool` | Whether two strings are equal |
| `count_str(s, sub)` | `i64` | Non-overlapping occurrences of `sub` in `s` |
| `reverse_str(s)` | `str` | The string reversed (bytewise) |
| `capitalize(s)` | `str` | First character uppercased, rest unchanged |
| `strip_prefix(s, prefix)` | `str` | `s` with `prefix` removed from the front if present |
| `strip_suffix(s, suffix)` | `str` | `s` with `suffix` removed from the end if present |
| `is_digits(s)` | `bool` | Whether `s` is non-empty and all ASCII digits |
| `is_alpha(s)` | `bool` | Whether `s` is non-empty and all ASCII letters |
| `center(s, width, pad)` | `str` | Center `s` within `width` using `pad` |
| `truncate(s, width, ellipsis)` | `str` | Shorten `s` to `width`, appending `ellipsis` if cut |

### `std::list`

| Function | Returns | Description |
|----------|---------|-------------|
| `range(start, stop)` | `list<i64>` | Integers from `start` up to (not including) `stop` |
| `sum(xs)` | `i64` | Sum of a `list<i64>` |
| `list_max(xs)` | `i64` | Largest element |
| `list_min(xs)` | `i64` | Smallest element |
| `sort(xs)` | — | Sort a `list<i64>` in place, ascending (stable) |
| `sort_str(xs)` | — | Sort a `list<str>` in place, ascending by content |
| `reverse_list(xs)` | — | Reverse a `list<i64>` in place |
| `index_of(xs, v)` | `i64` | Index of the first element equal to `v`, or `-1` |
| `index_of_str(xs, s)` | `i64` | Index of the first element equal to `s` (content), or `-1` |
| `contains_int(xs, v)` | `bool` | Whether `xs` contains `v` |
| `contains_str_in(xs, s)` | `bool` | Whether the `list<str>` `xs` contains `s` |
| `count_int(xs, v)` | `i64` | How many times `v` appears in `xs` |
| `unique(xs)` | `list<i64>` | A new list with duplicates removed, first-seen order |
| `take(xs, n)` | `list<i64>` | The first `n` elements of `xs` |
| `drop(xs, n)` | `list<i64>` | `xs` with its first `n` elements dropped |
| `mean(xs)` | `i64` | Integer mean (floor division; `0` for an empty list) |

### `std::path`

File-path helpers. Both `/` and `\` are recognized as separators on input; joins use `/`,
which every supported platform accepts.

| Function | Returns | Description |
|----------|---------|-------------|
| `path_base(p)` | `str` | Final component: `path_base("a/b/c.tv")` is `"c.tv"` |
| `path_dir(p)` | `str` | Directory part: `"a/b"`; `"."` when there is no separator |
| `path_ext(p)` | `str` | Extension with the dot (`".tv"`), `""` if none; a lone leading dot is a hidden name, not an extension |
| `path_join(a, b)` | `str` | Join two segments with exactly one separator |

### `std::convert`

Numeric-and-string conversions beyond the core `tostr` builtin: base conversions and
safe parsing. The parsing functions return `result<i64>` so bad input is recoverable
rather than a halt.

| Function | Returns | Description |
|----------|---------|-------------|
| `to_hex(n)` | `str` | `n` as a lowercase hex string (no `0x`; leading `-` if negative) |
| `to_bin(n)` | `str` | `n` as a binary string (no `0b`; leading `-` if negative) |
| `from_hex(s)` | `result<i64>` | Parse a hex string (optional `-`), or `err` on bad input |
| `to_int(s)` | `result<i64>` | Parse a decimal string (optional `-`), or `err` on bad input |

```torvik
apply std;

df main() -> void {
    echo!(gcd(48, 36));                      // 12
    echo!(pad_left("7", 3, "0"));            // 007
    set xs: list<i64> = range(1, 6);          // [1, 2, 3, 4, 5]
    echo!(sum(xs));                          // 15
}
```

---

*For the language itself, see [GUIDE.md](GUIDE.md); for the compiler and project tooling, see
[TOOLING.md](TOOLING.md).*
