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
| `str_concat(a, b)` | `str` | Concatenate two strings |
| `substr(s, start, end)` | `str` | Substring from `start` up to (not including) `end` |
| `char_at(s, i)` | `str` | The character at index `i`, as a one-character string |
| `byte_at(s, i)` | `i64` | The raw byte value at index `i` |
| `trim(s)` | `str` | Remove leading and trailing whitespace |
| `triml(s)` | `str` | Remove leading whitespace |
| `trimr(s)` | `str` | Remove trailing whitespace |
| `replace(s, old, new)` | `str` | Replace occurrences of `old` with `new` |
| `contains(s, sub)` | `bool` | Whether `s` contains `sub` |
| `starts(s, prefix)` | `bool` | Whether `s` starts with `prefix` |
| `ends(s, suffix)` | `bool` | Whether `s` ends with `suffix` |
| `split(s, sep)` | `list<str>` | Split `s` on `sep` into a list of pieces |
| `fmt(template, ...)` | `str` | Build a string with named `{var}` or positional `{}` interpolation. For printing, `echo` interpolates directly — see [the guide](GUIDE.md#strings-and-interpolation) |

```torvik
echo!(str_concat("Hello, ", "Torvik"));   // Hello, Torvik
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
| `len(xs)` | `i64` | Number of elements |
| `xs[i]` | element | Index access (operator, not a function) |

> A `list_pop` (remove-and-return the last element) is planned for a later release — its
> ownership transfer needs compile-time element-type information that lands post-v1.0. For now,
> read `xs[len(xs) - 1]` (bind `len` to a variable first) and then `list_remove`. See
> [ROADMAP.md](../ROADMAP.md).

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
| `read()` | `str` | Read input |
| `readln()` | `str` | Read a line |
| `readint()` | `i64` | Read and parse an integer |
| `readfloat()` | `f64` | Read and parse a float |
| `readbool()` | `bool` | Read and parse a boolean |
| `readkey()` | `str` | Read a single keypress |
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
| `try_readfile(path)` | — | Read a file, signalling failure rather than aborting |
| `writefile(path, content)` | — | Write a string to a file |
| `fs_exists(path)` | `bool` | Whether a path exists |
| `fs_mkdir(path)` | — | Create a directory |
| `fs_mtime(path)` | `i64` | Modification time |
| `fs_remove(path)` | — | Remove a file or directory |

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

```torvik
set count: i64 = args();
check count > 1 {
    fixed first: str = args_get(1);
    echo!("first argument: {first}");
}
fixed code: i64 = sys_run("ls -la");
```

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
`apply std::math;`, `apply std::strings;`, or `apply std::list;`. The library is installed at
`~/.torvik/lib/` and versioned independently of the compiler (the `std` key in `VERSION`).
Set `std = no_std` in a project's `torvik.rune` to opt out entirely.

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

### `std::strings`

| Function | Returns | Description |
|----------|---------|-------------|
| `join(parts, sep)` | `str` | Join a `list<str>` with a separator |
| `repeat_str(s, n)` | `str` | Repeat `s` `n` times |
| `pad_left(s, width, pad)` | `str` | Left-pad `s` to `width` using `pad` |
| `pad_right(s, width, pad)` | `str` | Right-pad `s` to `width` using `pad` |
| `str_eq(a, b)` | `bool` | Whether two strings are equal |

### `std::list`

| Function | Returns | Description |
|----------|---------|-------------|
| `range(start, stop)` | `list<i64>` | Integers from `start` up to (not including) `stop` |
| `sum(xs)` | `i64` | Sum of a `list<i64>` |
| `list_max(xs)` | `i64` | Largest element |
| `list_min(xs)` | `i64` | Smallest element |

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
