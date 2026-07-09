# The Torvik Guide

A complete tutorial and reference for the Torvik programming language, version 1.1.

Torvik is a compiled, statically-typed, general-purpose language with a Norse-inspired
keyword set. It compiles to native binaries through LLVM (emitting LLVM IR that is linked
with `clang`), and its own compiler (`torvc`) and package manager (`rune`) are written in
Torvik itself.

This guide describes **only what version 1.1 actually implements**. Features planned for
later versions are listed in [Roadmap & limitations](#roadmap--limitations) so you always
know where the edges are.

---

## Table of contents

1. [Hello, world](#hello-world)
2. [Comments](#comments)
3. [Variables: `set` and `fixed`](#variables-set-and-fixed)
4. [Types](#types)
5. [Operators](#operators)
6. [Printing: `echo` and `echo!`](#printing-echo-and-echo)
7. [Strings and interpolation](#strings-and-interpolation)
8. [Conditionals: `check` / `fallback`](#conditionals-check--fallback)
9. [The ternary: `?>` / `!>`](#the-ternary----)
10. [Guards: `guard` / `fallback`](#guards-guard--fallback)
11. [Loops: `whilst` and `each`](#loops-whilst-and-each)
12. [Functions: `df`](#functions-df)
13. [Collections: `list`, `table`, `bag`](#collections-list-table-bag)
14. [Assertions and aborts: `vow` and `halt`](#assertions-and-aborts-vow-and-halt)
15. [The `unsafe` prefix](#the-unsafe-prefix)
16. [Modules: `apply`](#modules-apply)
17. [Memory model](#memory-model)
18. [Roadmap & limitations](#roadmap--limitations)
19. [Keyword reference](#keyword-reference)
20. [Operator reference](#operator-reference)

---

## Hello, world

Every Torvik program has a `main` function. The simplest program is:

```torvik
df main() -> void {
    echo!("Hello from Torvik!");
}
```

`df` defines a function, `main` is the entry point, `-> void` says it returns nothing,
and `echo!` prints a line. Save it as `hello.tv` and compile:

```bash
torvc hello.tv -o hello
./hello
```

See [the tooling guide](TOOLING.md) for `torvc` and the `rune` project tool.

---

## Comments

Line comments start with `//` and run to the end of the line. Block comments open with `#-`
and close with `-#`, span any number of lines, and **nest** — a `#-` inside a block comment
opens another level, so you can comment out code that already contains block comments.

```torvik
// This is a comment.
set x: i64 = 1; // Comments can also trail a statement.

#- A block comment. -#
set y: i64 = #- they work inline, too -# 5;

#-
   Anything inside is ignored, across as many lines as you need.
   #- Nested blocks are fine — this whole region stays one comment. -#
-#
```

An unterminated `#-` or a stray `-#` with no opener is a clean, located compile error.

---

## Variables: `set` and `fixed`

Torvik has two ways to bind a value, and both require a type annotation:

- `set` — a **mutable** variable you can reassign.
- `fixed` — an **immutable** binding you cannot reassign.

```torvik
df main() -> void {
    set count: i64 = 0;     // mutable
    fixed name: str = "Sigrid"; // immutable

    count = count + 1;      // OK
    count += 1;             // OK (compound assignment)
    // name = "Bjorn";      // error: `name` is fixed
}
```

The form is always `set NAME: TYPE = VALUE;` or `fixed NAME: TYPE = VALUE;`.

---

## Types

Torvik is statically typed. The built-in types are:

### Integers

Signed: `i8`, `i16`, `i32`, `i64`. Unsigned: `u8`, `u16`, `u32`, `u64`.
Wide integers: `i128`, `u128`.

```torvik
set a: i64  = 42;
set b: u8   = 255;
set big: i128 = 100000000000000000000; // far beyond i64's range
set u: u64  = 18000000000000000000;
```

`i64` is the everyday integer type. The 128-bit types are heap-backed and support the
full set of arithmetic and comparison operators.

> A leading `-` negates a value: `-5`, or `set n: i64 = -x;`. It also works as a chain
> operand, including a binary minus immediately followed by a unary minus — `a - -b`,
> `a + -5`, `a * -f(x)` — in both integer and float expressions. A leading `-` binds to
> exactly **one** value: `-m + 2` is `(-m) + 2`, not `-(m + 2)`. The same applies to the
> bitwise NOT prefix `~` (integers only): `~0` is `-1`, and `~m & 7` is `(~m) & 7`.

### Floating point

`f64` is a 64-bit double. Float literals always echo with a decimal point.

```torvik
fixed pi: f64 = 3.14159;
echo!(pi);        // 3.14159
echo!(2.0);       // 2.0
```

### Booleans

`bool` is `true` or `false`.

```torvik
fixed ready: bool = true;
```

### Strings and characters

`str` is a string. String literals use double quotes. A **character literal** uses single
quotes and produces a one-character string:

```torvik
fixed greeting: str = "hello";
fixed letter:   str = 'A';        // a 1-character string, "A"
echo!(byte_at(letter, 0));        // 65  (the byte value)
echo!(char_at(greeting, 1));      // "e" (a 1-character string)
```

`char_at(s, i)` returns the character at index `i` as a one-character string;
`byte_at(s, i)` returns the raw byte value as an `i64`.

### Collections

`list<T>`, `table<K, V>`, and `bag<T>` are covered in
[Collections](#collections-list-table-bag).

---

## Operators

Arithmetic: `+`, `-`, `*`, `/`, `%`.
Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`.
Logical: `&&`, `||`, `!`.
Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`.
Compound assignment: `+=`, `-=`, `*=`, `/=`, `%=`.

```torvik
set x: i64 = 10;
x += 5;            // 15
echo!((2 + 3) * 4);// 20
echo!(6 & 3);      // 2
echo!(1 << 3);     // 8
echo!(7 > 3 && 2 < 5); // (used in a condition)
```

Use parentheses to group. See the [operator reference](#operator-reference) for the full
list and precedence notes.

---

## Printing: `echo` and `echo!`

- `echo!(value)` prints the value followed by a newline.
- `echo(value)` prints the value with no trailing newline.

```torvik
echo("no newline... ");
echo!("and the rest.");
```

`echo` works on any printable scalar — strings, all integer widths, `f64`, and `bool`.

### `echo` interpolates on its own

You don't need any helper to put values into a printed string. `echo` and `echo!` perform
**named interpolation** directly: any `{name}` in the string is replaced with the value of
the variable `name` in scope, formatted for its type.

```torvik
fixed name: str = "Odin";
set count:  i64 = 3;
fixed ratio: f64 = 0.5;
fixed ok:   bool = true;

echo!("Hello {name}!");                 // Hello Odin!
echo!("{name} has {count} runes");      // Odin has 3 runes
echo!("ratio={ratio}, ok={ok}");        // ratio=0.5, ok=true
```

Whatever the variable's type — integer, `f64`, `bool`, or `str` — `echo` formats it correctly.
Use `{{` and `}}` for a literal brace.

What goes inside the braces can be a **variable name or a full expression** — arithmetic, a
function call, an index, or a combination:

```torvik
echo!("total: {price * qty}");          // arithmetic
echo!("doubled: {dbl(count)}");         // a function call
echo!("first: {runes[0]}");             // an index
echo!("{count} and {count + 1}");       // mixed literals, bare vars, and expressions
```

A bare `{name}` is still the common case and stays the fast path; expressions are compiled
just like any other code, so they follow the usual typing and precedence rules.

`echo` and `echo!` also accept **several comma-separated arguments**, printed in order and
separated by spaces — handy when you'd rather not write a template at all:

```torvik
echo!("count:", count);                 // count: 3
echo!(name, "rolled", count);           // Odin rolled 3
```

---

## Strings and interpolation

For **printing**, you usually don't need anything beyond `echo`'s built-in interpolation
(above): `echo!("Hello {name}!")` is the idiomatic way.

`fmt` is for the two cases `echo` interpolation doesn't cover:

**1. Building a string value** (rather than printing it) — to assign, pass to a function, or
return:

```torvik
fixed who: str = "World";
set n: i64 = 3;
fixed line: str = fmt("Hi {who}, n={n}");  // a str value you can keep
greet(fmt("user_{n}"));
```

`fmt` supports the same named `{name}` interpolation as `echo`.

**2. Positional placeholders** — `{}` filled from trailing arguments, left to right:

```torvik
fixed label: str = fmt("{} + {} = {}", 2, 3, 5); // "2 + 3 = 5"
```

As with `echo`, use `{{` and `}}` for a literal brace. Named braces accept a variable name or
a full expression (`fmt("sum={a + b}")`, `fmt("y={f(x)}")`).

> In short: reach for `echo`'s own interpolation when you're printing, and for `fmt` when you
> need the resulting **string** as a value or want **positional** placeholders.

Common string builtins include `len`, `str_concat`, `substr`, `trim` / `triml` / `trimr`,
`replace`, `contains`, `starts`, `ends`, and `split`. They are documented with examples in
[the standard library reference](STDLIB.md).

```torvik
echo!(str_concat("Hello, ", "Torvik")); // Hello, Torvik
echo!(substr("hello", 0, 4));           // hell
echo!(contains("hello", "ell"));        // (a bool)
```

---

## Conditionals: `check` / `fallback`

`check` is Torvik's `if`. An optional `fallback` is its `else`.

```torvik
df classify(n: i64) -> str {
    check n > 0 {
        return "positive";
    } fallback {
        return "non-positive";
    }
}
```

Conditions can combine with `&&` and `||`:

```torvik
check age >= 13 && age < 20 {
    echo!("teenager");
}
```

For more than two branches, nest `check` inside `fallback`, or use the ternary for simple
value selection.

---

## The ternary: `?>` / `!>`

Torvik's conditional expression is `condition ?> value_if_true !> value_if_false`. Think of
`?>` as "then" and `!>` as "else".

```torvik
set n: i64 = 7;
fixed parity: str = n % 2 == 0 ?> "even" !> "odd"; // "odd"
echo!(n > 0 ?> "positive" !> "non-positive");
```

Ternaries chain to the right, so you can express several cases:

```torvik
fixed sign: str = n > 0 ?> "positive"
               !> n < 0 ?> "negative"
               !> "zero";
```

Both branches must have the same kind of type (two numbers, two strings, and so on).

> The ternary **short-circuits**: only the taken branch is evaluated, so it is safe to guard
> with it — `d != 0 ?> 100 / d !> 0` never divides by zero, and a recursive call in a branch
> only runs when that branch is taken.

---

## Guards: `guard` / `fallback`

`guard` states a condition that must hold to continue. If the condition is **false**, the
`fallback` block runs — typically to return early.

```torvik
df reciprocal_ok(n: i64) -> str {
    guard n != 0 fallback {
        return "undefined";
    }
    // Past this point, n is guaranteed non-zero.
    return "ok";
}
```

Guards read top-to-bottom and keep the happy path unindented:

```torvik
df clamp_label(n: i64) -> str {
    guard n >= 0   fallback { return "negative"; }
    guard n < 100  fallback { return "too big"; }
    return "in range";
}
```

If a guard's condition is compound, wrap it in parentheses: `guard (a > 0 && b > 0) fallback { ... }`.

---

## Loops: `whilst` and `each`

### `whilst` — condition loop

`whilst` repeats while a condition holds (a `while` loop):

```torvik
set i: i64 = 0;
whilst i < 5 {
    echo!(i);
    i += 1;
}
```

### `each` — range loop

`each` iterates an integer range. `START..END` is **exclusive** (END is not included);
`START..+END` is **inclusive** (END is the last value):

```torvik
each i in 0..5 {
    echo!(i);            // 0 1 2 3 4
}

set sum: i64 = 0;
each k in 1..5 {
    sum += k;            // 1 + 2 + 3 + 4
}
echo!(sum);             // 10

each i in 0..+5 {
    echo!(i);            // 0 1 2 3 4 5  (..+ includes the end)
}
```

To walk a list, range over its length and index it:

```torvik
set xs: list<i64> = list_new();
push(xs, 10); push(xs, 20); push(xs, 30);

set total: i64 = 0;
each i in 0..len(xs) {
    total += xs[i];
}
echo!(total);           // 60
```

### `break` and `continue`

Both loops support `break` (exit the loop) and `continue` (skip to the next iteration).
In an `each` loop, `continue` still advances the counter.

```torvik
each i in 0..10 {
    check i == 3 { break; }     // stops at 3
    echo!(i);
}

each i in 0..5 {
    check i == 2 { continue; }  // skips 2
    echo!(i);                    // 0 1 3 4
}
```

> The range forms are `START..END` (exclusive) and `START..+END` (inclusive). You can also
> iterate a **list directly** — `each x in xs { ... }` binds `x` to each element in turn,
> typed as the list's element type:
>
> ```torvik
> set words: list<str> = list_new();
> push(words, "for"); push(words, "the"); push(words, "horde");
> each w in words { echo!(w); }        // for / the / horde
> ```

---

## Functions: `df`

A function is `df NAME(PARAMS) -> RETURN_TYPE { BODY }`. Parameters are
`name: type`, comma-separated. Use `-> void` for no return value.

```torvik
df add(a: i64, b: i64) -> i64 {
    return a + b;
}

df greet(name: str) -> void {
    echo!("Hello, {name}!");
}

df main() -> void {
    echo!(add(2, 3));   // 5
    greet("Astrid");
}
```

Functions may call functions defined later in the file, and recursion is fully supported:

```torvik
df factorial(n: i64) -> i64 {
    check n <= 1 { return 1; }
    return n * factorial(n - 1);
}
```

---

## Collections: `list`, `table`, `bag`

### Lists — `list<T>`

An ordered, growable sequence. Create with `list_new()`, append with `push`, index with
`xs[i]`, and get the length with `len`.

```torvik
set xs: list<i64> = list_new();
push(xs, 10);
push(xs, 20);
echo!(xs[0]);          // 10
echo!(len(xs));        // 2

list_insert(xs, 1, 15); // insert 15 at index 1 -> [10, 15, 20]
list_remove(xs, 0);     // remove index 0       -> [15, 20]

fixed last: i64 = list_pop(xs); // remove & return the last element -> 20
```

Lists work with any element type, including `list<str>`, `list<f64>`, and `list<i128>`. The element type
usually comes from the annotation (`set xs: list<str> = list_new();`), but Torvik can also
**infer** it from the first `push` when you leave the annotation off:

```torvik
set names = list_new();   // element type inferred...
push(names, "odin");      // ...as str, from this push
echo!(names[0]);          // odin
```

(Inference covers `str`, integer, `f64`, and `bool` element types. Annotate other cases, or
when inference can't see a `push`.)

### Tables — `table<K, V>`

A hash map. Create with `table_new()`, then `table_set` / `table_get` / `table_has` /
`table_del` / `table_len`.

```torvik
set ages: table<str, i64> = table_new();
table_set(ages, "Ivar", 30);
echo!(table_get(ages, "Ivar"));      // 30
echo!(table_has(ages, "Ivar"));      // (a bool)
echo!(table_len(ages));              // 1
```

A lookup for a missing key returns a zero value (for an integer-valued table, `0`).

> When you pull a value out of a `table` to use it in arithmetic, bind it to a typed
> variable first: `set a: i64 = table_get(ages, "Ivar"); echo!(a * 2);`. Echoing or
> formatting a table value directly works; using it directly inside a larger arithmetic
> expression is the one case that needs the intermediate variable (see
> [limitations](#roadmap--limitations)).

### Bags — `bag<T>`

A multiset / set-like collection. Create with `bag_new()`, then `bag_add` / `bag_has` /
`bag_remove` / `bag_len`.

```torvik
set seen: bag<str> = bag_new();
bag_add(seen, "rune");
echo!(bag_has(seen, "rune"));   // (a bool)
echo!(bag_len(seen));           // 1
```

---

## Pipelines and membership: `~>` and `<|`

Two operators make common patterns read more naturally.

### Weave — `~>`

The **weave** operator pipes a value through one or more functions, left to right:
`x ~> f` is `f(x)`, and `x ~> f ~> g` is `g(f(x))`.

```torvik
df shout(s: str) -> str { return str_concat(s, "!"); }

fixed raw: str = "  hello  ";
set cleaned: str = raw ~> trim;          // trim(raw)
set loud:    str = raw ~> trim ~> shout; // shout(trim(raw))
echo!(loud);                             // hello!
```

Any value can lead the weave — a variable, a literal, a call result, or a list element. A
stage may also take **arguments**: `x ~> f(a, b)` inserts the woven value as the FIRST
argument, meaning `f(x, a, b)`. This works with your own functions and with `replace` and
`substr`; the one-argument builtins (`trim`, `upper`, ...) are written bare.

```torvik
fixed csv: str = " a,b,c ";
fixed slug: str = csv ~> trim ~> replace(",", "-") ~> upper;
echo!(slug);                             // A-B-C

df addn(x: i64, n: i64) -> i64 { return x + n; }
set v: i64 = 5;
echo!(v ~> addn(3) ~> addn(10));         // 18
```

Weave results are typed, so a trailing comparison folds by content:
`check s ~> trim == "done" { ... }`. Arity is checked counting the inserted value — a clean
compile error tells you exactly how many arguments a stage still needs.

The woven value's type is also checked against each stage's first parameter when both
are known: weaving an `i64` into a function whose first parameter is `str` (or the
reverse) is a clean compile error rather than run-time misbehavior.

### Membership — `<|`

The **membership** operator tests whether a value is in a collection: `item <| collection`
yields a `bool`.

```torvik
set ids: list<i64> = list_new();
push(ids, 10); push(ids, 20); push(ids, 30);

set needle: i64 = 20;
check needle <| ids {
    echo!("present");
}
```

Any value can go on the left — a variable, a literal, or a call result (`20 <| ids`,
`"hi" <| names`, `f(x) <| ids`). Integer lists compare by value; **string lists compare by
content** (v1.1.0), so a freshly built string matches an equal stored string. A mismatch
between the item's type and the list's element type is a clean compile error.

---

## Assertions and aborts: `vow` and `halt`

`halt(message)` prints a message and exits the program immediately with a non-zero status.

```torvik
df main() -> void {
    halt("unrecoverable: configuration missing");
    // nothing here runs
}
```

`vow(condition, message)` is an assertion: if the condition is **false**, it prints the
message and exits non-zero; if true, execution continues.

```torvik
df main() -> void {
    set balance: i64 = compute_balance();
    vow(balance >= 0, "balance must never be negative");
    echo!("invariant held");
}
```

Use `vow` to enforce invariants and `halt` for unrecoverable error paths.

---

## The `unsafe` prefix

`unsafe` is **not** a general "lower-level" block. It is a single-statement prefix that opts
into a specific operation the compiler would otherwise **reject** — you're telling the compiler
"I know this is normally refused, and I take responsibility for it."

The one operation it unlocks is **wrapping an out-of-range integer literal into a sized
type**. By default the compiler rejects a literal that doesn't fit its declared type, because
silently truncating it would change the value and hide bugs:

```torvik
set b: i8 = 200;         // error: literal 200 is out of range for i8 (valid -128 to 127)
unsafe set b: i8 = 200;  // allowed — you take responsibility; b wraps to -56
unsafe x = 300;          // also works on an assignment to a sized variable
```

`unsafe` prefixes exactly one declaration (`set`/`fixed`) or assignment — the statement whose
rejected operation you're opting into. It has **no meaning on any other statement**, so using
it where there's nothing to override is a clean compile error rather than a silent no-op:

```torvik
unsafe echo!("hi");   // error: unsafe can only prefix a declaration or assignment
```

Future releases will extend `unsafe` to other operations that are rejected by default but that
you may deliberately want; each will be an explicit, documented opt-in like this one — never a
blanket "anything goes" region.

---

## Modules: `apply`

`apply NAME;` brings the definitions from another Torvik module into the current file. The
Torvik toolchain itself uses this to share modules across its source files.

```torvik
apply helpers;   // pull in definitions from helpers

df main() -> void {
    // functions from `helpers` are now available
}
```

### The standard library

On top of the always-available [core builtins](STDLIB.md), Torvik ships an **opt-in standard
library** that you bring in with `apply`. Pull in the whole library, or just one part:

```torvik
apply std;          // everything: math, strings, list
// or, more selectively:
apply std::math;    // just the math module
apply std::strings; // just the strings module
apply std::list;    // just the list module
```

The library lives at `~/.torvik/lib/` (installed alongside the toolchain) and is versioned
independently of the compiler — see the `std` key in the `VERSION` file. A quick taste:

```torvik
apply std;

df main() -> void {
    echo!(pow(2, 10));               // std::math   -> 1024
    echo!(join(split("a,b,c", ","), "-")); // std::strings -> a-b-c
    set xs: list<i64> = range(1, 5);  // std::list -> [1,2,3,4]
    echo!(sum(xs));                   // 10
}
```

To opt out of the standard library entirely for a project, set `std = no_std` in its
`torvik.rune` manifest. Every function the standard library provides is listed in
[the standard library reference](STDLIB.md#the-standard-library-apply-std).

---

## Memory model

Torvik manages memory with **automatic reference counting (ARC)**. Values that live on the
heap (strings, lists, tables, bags, 128-bit integers) are reference-counted and released
when no longer referenced. There is no garbage collector and no manual `free`.

What this guarantees:

- **Leak-free for supported patterns.** Ordinary code that builds and discards strings and
  collections releases its memory deterministically.
- **Clean out-of-memory behavior.** If an allocation cannot be satisfied, the program panics
  cleanly rather than corrupting state. There are no built-in size or duration limits.
- **No reference cycles.** Reference counting cannot reclaim cycles, but Torvik
  has no construct that can create one (that would require nested mutable containers, which
  arrive in a later version alongside a cycle strategy).

---

## Roadmap & limitations

Torvik is deliberately focused. The following are **not** in the language yet. `task` (async),
a compile-time warnings system, and official macOS support are planned for **v1.2.0**; the
rest are planned for future versions (see [ROADMAP.md](../ROADMAP.md) for the full plan and
reasoning):

- **Structs (`shape`)**, **pattern matching (`when`)**, and **async / concurrent tasks
  (`task`)**.
- **Result types** (`ok` / `err` / `result<T>`), **enums**, and a **`pub`** visibility keyword.
- **Additional numeric types**: `f32` and a dedicated `char` type (today, character literals
  are one-character strings).
- **Fixed-size arrays** (`[T; N]`).

- **Systems / OS-development primitives** (inline assembly, volatile memory access, raw
  pointer operations, packed structs). Torvik is a general-purpose compiled language;
  these are not implemented.

Known limitations:


- **128-bit integers are fully supported, including in lists.** As of v1.1.0 you can do
  arithmetic and comparisons directly on `list<i128>`/`list<u128>` elements (`xs[0] + xs[1]`,
  `xs[i] > v`), push and insert integer literals straight into a 128-bit list, and reassign
  128-bit variables freely. (`u64` signedness is fully per-operand: a `u64` anywhere in an
  expression — lead or not, variable or call result — selects unsigned division, modulo,
  shifts, and comparison from that operand onward.)

Every one of these is a clean compile error or a documented narrow case, never a silent wrong
answer or a crash. The full plan is in [ROADMAP.md](../ROADMAP.md). Windows support landed in
v1.1.0 (Linux and Windows are both supported; macOS is not yet supported, coming in v1.2.0); the warnings system,
`task` (async), Result types, and official macOS support are planned for v1.2.0.

---

## Keyword reference

| Keyword     | Purpose                                                        |
|-------------|----------------------------------------------------------------|
| `df`        | Define a function                                              |
| `set`       | Declare a mutable variable                                     |
| `fixed`     | Declare an immutable variable                                  |
| `return`    | Return from a function                                         |
| `check`     | Conditional (`if`)                                             |
| `fallback`  | Else branch of `check`, or the recovery block of `guard`       |
| `guard`     | Require a condition, else run a fallback (early-exit)          |
| `whilst`    | Condition loop (`while`)                                       |
| `each`      | Range loop (`each i in START..END`)                            |
| `in`        | Separates the loop variable from its range in `each`           |
| `break`     | Exit the current loop                                          |
| `continue`  | Skip to the next loop iteration                                |
| `echo`      | Print without a trailing newline                               |
| `echo!`     | Print with a trailing newline                                  |
| `vow`       | Assertion: abort with a message if a condition is false        |
| `halt`      | Print a message and exit immediately                           |
| `unsafe`    | Prefix a declaration/assignment to opt into a rejected op (wrap an out-of-range literal) |
| `apply`     | Bring another module's definitions into the file              |
| `true` / `false` | Boolean literals                                          |

---

## Operator reference

| Category   | Operators                              | Notes                                  |
|------------|----------------------------------------|----------------------------------------|
| Arithmetic | `+`  `-`  `*`  `/`  `%`                 | Prefix `-` negates (`-x`)              |
| Comparison | `==`  `!=`  `<`  `>`  `<=`  `>=`        | Yield `bool`                           |
| Logical    | `&&`  `\|\|`  `!`                       | Short-circuiting in conditions         |
| Bitwise    | `&`  `\|`  `^`  `~`  `<<`  `>>`         | On integer types                       |
| Assignment | `=`  `+=`  `-=`  `*=`  `/=`  `%=`       | Compound forms update in place         |
| Range      | `..`  `..+`                             | `..` exclusive, `..+` inclusive; used in `each` |
| Ternary    | `?>`  `!>`                              | `cond ?> a !> b`; only the taken branch runs |
| Weave      | `~>`                                    | `x ~> f ~> g` is `g(f(x))`             |
| Membership | `<\|`                                   | `item <\| collection` yields a `bool`  |

Arithmetic binds tighter than comparison, including with call/index operands
(`a + f(x) > b` reads as `(a + f(x)) > b`). Use parentheses whenever they make intent
clearer.

**Comparisons are fully chainable (v1.1.0).** Any value — a variable, a literal, a function
call's result, a list element, a `table_get`, or a parenthesized expression — can sit on
either side of a comparison. **Strings compare by content**, never by pointer: `==` and `!=`
test equality, and `<` `<=` `>` `>=` order strings lexicographically (byte order). Comparing
a string with a numeric value is a clean compile error; convert first with `tostr(...)` or
`toint(...)`.

```torvik
whilst char_at(s, i) == " " { i += 1; }      // skip leading spaces
check substr(name, 0, 3) == "st_" { ... }
check xs[i] != "" { ... }                     // list<str> element, by content
check "apple" < "banana" { echo!("sorted"); }
```

---

*This guide tracks Torvik v1.1. For the compiler and project tooling, see
[TOOLING.md](TOOLING.md); for the built-in function library, see [STDLIB.md](STDLIB.md).*
