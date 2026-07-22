# The Torvik Guide

A complete tutorial and reference for the Torvik programming language, version 1.4.

Torvik is a compiled, statically-typed, general-purpose language with a Norse-inspired
keyword set. It compiles to native binaries through LLVM (emitting LLVM IR that is linked
with `clang`), and its own compiler (`torvc`) and package manager (`rune`) are written in
Torvik itself.

This guide describes **only what version 1.4 actually implements**. Features planned for
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
11. [Aetts and pattern matching: `aett` / `when`](#aetts-and-pattern-matching-aett--when)
12. [Loops: `whilst` and `each`](#loops-whilst-and-each)
13. [Functions: `df`](#functions-df)
14. [Collections: `list`, `table`, `bag`](#collections-list-table-bag)
15. [Assertions and aborts: `vow` and `halt`](#assertions-and-aborts-vow-and-halt)
16. [Error handling: `result<T>`, `ok`, and `err`](#error-handling-resultt-ok-and-err)
17. [Concurrency: ravens and bridges](#concurrency-ravens-and-bridges)
18. [Compile warnings](#compile-warnings)
19. [The `unsafe` prefix](#the-unsafe-prefix)
20. [Modules: `apply`](#modules-apply)
21. [Memory model](#memory-model)
22. [Roadmap & limitations](#roadmap--limitations)
23. [Keyword reference](#keyword-reference)
24. [Operator reference](#operator-reference)

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
`byte_at(s, i)` returns the raw byte value as an `i64`. `chr(code)` goes the
other way — the one-character string for a byte code (`chr(65)` is `"A"`).

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

### Everywhere else, a string is just a string

Interpolation belongs to `echo`, `echo!`, and `fmt` — and only to a string literal written
**directly** as one of their arguments. A string literal anywhere else is plain data: braces
are ordinary characters, nothing is read from scope, and no escapes are needed. That's what
lets CSS, JSON, or template text sit in an ordinary string untouched:

```torvik
fixed css: str = ".a{color:red}";          // exactly this text
fixed slot: str = "{{title}}";             // exactly {{title}} — no collapse
fixed msg: str = fmt("Hi {name}");         // fmt interpolates: Hi Odin
echo!(slot);                                // prints {{title}} — the variable's
                                            // contents are never re-scanned
```

The last line shows the flip side: interpolation happens at **compile time on literals**,
never at run time on values — printing a variable that happens to contain braces prints them
exactly as they are. (Before v1.3.0, every string literal interpolated; brace-heavy data
needed `{{` escapes everywhere. Code that kept interpolated literals inside `echo`/`fmt` —
the documented idiom — is unaffected.)

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

## Aetts and pattern matching: `aett` / `when`

An **aett** is a family of named values — Torvik's take on an enumeration, named for the
*ættir*, the families the runes of the futhark are grouped into. Declare one at the top
level and access its variants with `::`:

```tv
aett Status { Pending, Active, Closed }

df main() -> void {
    set s: Status = Status::Pending;
    echo!(s);            // 0 - variants are i64 ordinals, in declaration order
    echo!(typeof(s));    // Status
    s = Status::Closed;
    check s == Status::Closed { echo!("closed"); }
}
```

Aett values are i64-backed (0-based ordinals), so they print, compare, store in lists,
and pass to functions like any integer — while the aett name works as a type annotation
for variables, parameters, and return types, and `typeof` reports it.

**`when`** matches a value against patterns. Arms use `=>`, with a single statement or a
block on the right, and `fallback =>` as the default arm (last, as always in Torvik):

```tv
df describe(s: Status) -> str {
    when s {
        Status::Pending => return "waiting";
        Status::Active  => return "live";
        Status::Closed  => return "done";
    }
    return "";
}
```

When the scrutinee is aett-typed and there's no `fallback`, the compiler checks
**exhaustiveness**: a missing variant is a clean compile error naming exactly what's
uncovered — add the arm or a `fallback`. Adding a variant to an aett later instantly
surfaces every `when` that needs updating.

`when` also matches integers (literal patterns, negatives included), where a `fallback`
arm is required — the compiler can't prove integer patterns cover every value:

```tv
when n {
    1        => echo!("one");
    7        => echo!("seven");
    fallback => echo!("many");
}
```

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

### Optional parameters — `^`

Prefix a parameter with `^` to make it optional. A caller may leave trailing
optional arguments off; each omitted one takes a type-appropriate zero (`0`,
`0.0`, `false`, or `""`). Required parameters must come before optional ones, so
positional calls stay unambiguous.

```torvik
df greet(name: str, ^count: i64, ^tag: str) -> void {
    echo!("hi {name} x{count} [{tag}]");
}

df main() -> void {
    greet("alpha");                 // hi alpha x0 []
    greet("beta", 3);               // hi beta x3 []
    greet("gamma", 5, "vip");       // hi gamma x5 [vip]
}
```

Passing too few (missing a required argument) or too many arguments is a clean,
located error: `function 'greet' expects 1 to 3 argument(s) but got 4`. Optional
parameters are limited to scalar and `str` types.

### Variadic parameters — `*`

Prefix the **last** parameter with `*` to gather zero or more trailing arguments
into a `list<T>`, which you iterate like any list. A function has at most one
variadic parameter, and it combines with optional ones in the order
required → optional → variadic.

```torvik
df join_words(sep: str, *words: str) -> str {
    set out: str = "";
    set first: bool = true;
    each w in words {
        check first { out = w; first = false; }
        fallback { out = str_concat(out, sep, w); }
    }
    return out;
}

df main() -> void {
    echo!(join_words(", "));                        // (empty)
    echo!(join_words(", ", "a", "b", "c"));         // a, b, c
}
```

A variadic gathers a `str` or integer-family element type.

### Arguments are type-checked

Every argument is checked against its parameter at the call site. A definite
mismatch — a number where a string is expected (or the reverse), a decimal where
an integer is expected, a list where a scalar is expected — is a clean, located
error rather than silent garbage at run time. Ambiguous cases are left alone, so
correct programs are never rejected.

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

To iterate a table, take its keys — `table_keys(t)` returns them as a **sorted**
`list<str>` (sorted so loops are reproducible; a hash table's internal order shifts as it
grows) — and fetch each value with `table_get`:

```tv
fixed ks: list<str> = table_keys(ages);
set i: i64 = 0;
whilst i < len(ks) {
    fixed k: str = ks[i];
    echo!("{k} is {table_get(ages, k)}");
    i += 1;
}
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

## Error handling: `result<T>`, `ok`, and `err`

For operations that can fail, Torvik has an explicit result type instead of exceptions.
A `result<T>` is either an **ok** carrying a value of type `T`, or an **err** carrying a
message (and optionally a numeric code). Your own functions produce them with `ok(value)`
and `err(message)` / `err(code, message)`:

```tv
df safe_div(a: i64, b: i64) -> result<i64> {
    check b == 0 { return err("division by zero"); }
    return ok(a / b);
}

df main() -> void {
    set r: result<i64> = safe_div(10, 0);
    check is_ok(r) {
        echo!(unwrap(r));
    } fallback {
        echo!("failed: {err_msg(r)}");     // failed: division by zero
    }
    echo!(unwrap_or(r, -1));               // -1 - the fallback value
}
```

The consumers: `is_ok(r)` / `is_err(r)` test the state; `unwrap(r)` takes the value
(and halts with the error message if the result is an err — check first, or use
`unwrap_or(r, default)`); `err_msg(r)` and `err_code(r)` read the error side.
`result<i64>`, `result<str>`, and `result<f64>` are supported, as parameters, return
types, and locals.

Several builtins come in a result-returning form: `try_readfile(path)`,
`try_toint(s)`, and `try_tofloat(s)` — the same operations as their plain
counterparts, but a failure is an `err` you can inspect instead of a halt.

---

## Concurrency: ravens and bridges

Torvik runs work on more than one thread with two ideas that fit together. A **raven**
carries a task out to its own thread and comes back with what it found. A **bridge** is a
typed channel that carries values between tasks. The whole model is built on one rule that
makes it safe by construction: **tasks share no mutable state.** Values are copied when they
cross a thread boundary, so the reference-counting runtime stays correct without locks on
every object, and data races are not something you *avoid* — they are something the language
gives you no way to write.

### Ravens: spawning tasks

`raven` prefixes a call to one of your `df` functions and runs it on a new OS thread. There
are two forms. Fire-and-forget runs the function and moves on:

```tv
raven log_event("started");     // runs on its own thread; main does not wait
```

Or capture a **handle** so you can collect the result later. A handle has the type
`task<T>`, where `T` is the spawned function's return type, and `join` blocks until the task
finishes and hands back its value:

```tv
df fetch(url: str) -> str { /* ... slow work ... */ return body; }

df main() -> void {
    set h: task<str> = raven fetch("https://example.com");
    // ... do other work here while fetch runs ...
    fixed page: str = join(h);      // wait for the task, take its result
    echo!("got {len(page)} bytes");
}
```

`join(h)` is an ordinary expression — it composes like any call, so `check join(h) == "ok"`
works. A `task<void>` (spawning a function that returns nothing) is joined as a *statement*:
`join(h);` waits and yields nothing. Every handle may be joined **exactly once**; a second
join is a clean panic, because the result has already been taken. A bare `join(h);` on a
value-returning task is also fine — it waits and discards the result.

Every crossable type can be a task result: all the integer widths, `f64`, `bool`, `str`,
`i128`/`u128`, and `aett` values.

### Arguments are copied at the spawn

When you spawn `raven fetch(url)`, the argument is **deep-copied at the moment the spawn
statement runs**, on your thread. After that line, the task owns its own copy and you own
yours — reassigning your variable afterward cannot affect the task, and the task cannot
affect you:

```tv
set name: str = "original";
set h: task<str> = raven greet(name);
name = "changed";               // the task already has its own copy
echo!(join(h));                 // greeted "original", not "changed"
```

This is why the model needs no locks on ordinary values: there is only ever one thread that
can see any given string, list, or box. (Collections — `list`, `table`, `bag` — and
`result` cannot cross into a task in this version; pass the scalars and strings a task needs
and rebuild structure inside it. The one exception is a bridge, below, which is *shared* on
purpose.)

### The self-contained rule

A raven carries everything it needs in its claws. **A function that is ever spawned, and
every function it transitively calls, may not read or write a global variable.** The
compiler checks this and reports the spawn, the offending function, and the global by name:

```tv
set counter: i64 = 0;

df tick() -> i64 { return counter; }    // reads a global

df main() -> void {
    set h: task<i64> = raven tick();    // error: 'tick' is not self-contained
    echo!(join(h));
}
```

The reason is precise: even *reading* a global string from two threads races on its
reference count, which is not atomic. Rather than make every refcount in the language pay
for atomics, Torvik requires tasks to be self-contained and keeps the runtime lock-free.
Pass what the task needs as arguments. (This rule can only loosen in future versions, never
tighten — so code that compiles today keeps compiling.)

### Panics in tasks

If a task panics — an out-of-bounds index, a failed `vow`, a `halt` — the **whole process
stops immediately** with the usual message, exactly as it would on the main thread. A
background failure is never silently swallowed or deferred until you happen to join. For
failure you want to *handle*, return a `result<T>` from the task and inspect it after the
join, the same as anywhere else in Torvik.

### Bridges: channels between tasks

A **bridge** is a typed, buffered queue that more than one thread can use at once. It is the
one object tasks share — and everything that touches its interior goes through its own lock,
so sharing it is safe. Create one with a capacity, then `send` and `recv`:

```tv
set ch: bridge<str> = bridge_new(8);    // capacity 8 (must be >= 1)

send(ch, "hello");                       // copies the value in; blocks if full
fixed msg: str = recv(ch);               // takes one out; blocks if empty
```

Values **deep-copy on send**, the same guarantee as spawn arguments: once a value crosses
the bridge, sender and receiver own independent copies. Bridges carry the same types tasks
do — every integer width, `f64`, `bool`, `str`, `i128`/`u128`, and `aett` values.

A bridge is passed *into* a task as an argument — and unlike every other argument, the task
receives **the bridge itself**, not a copy (a private copy of a channel would be pointless).
That makes a bridge the one shared object in a Torvik program; its own lock protects it.

### Closing a bridge and the worker loop

`bridge_close(ch)` says "no more values will be sent." It is idempotent and wakes anyone
waiting. After a bridge is closed *and* emptied, `recv` panics — reading past the end of a
finished stream is a mistake. To drain a stream to its natural end, use `try_recv`, which
returns a `result<T>`: an ok value while data remains, and a single `err` when the bridge is
closed and drained. That err is the loop's stop signal:

```tv
df produce(ch: bridge<i64>, n: i64) -> void {
    set i: i64 = 0;
    whilst i < n { send(ch, i); i += 1; }
    bridge_close(ch);                    // done producing
}

df main() -> void {
    set ch: bridge<i64> = bridge_new(4);
    raven produce(ch, 10);               // producer runs on its own thread
    set total: i64 = 0;
    whilst true {
        set r: result<i64> = try_recv(ch);
        check is_err(r) { break; }       // closed and drained: stop cleanly
        total += unwrap(r);
    }
    echo!("sum: {total}");
}
```

`send` into a closed bridge panics — closing means you promised not to. Multiple producers
and multiple consumers on one bridge are fully supported; the bridge's lock serializes them,
and each value is received exactly once by exactly one consumer.

### What crosses, and what waits

A quick reference for this version:

- **Cross a spawn or a bridge:** every integer width, `u64`, `f64`, `bool`, `str`,
  `i128`/`u128`, and `aett` values. Collections and `result` do not cross yet.
- **`bridge_new(cap)`** needs `cap >= 1` (unbuffered rendezvous channels are a later
  addition); a smaller capacity panics.
- **Ordering** is whatever `join` and bridge blocking impose — there is no hidden ordering,
  and no sleeps are ever needed to make concurrent Torvik deterministic. Force the ordering
  you want by joining, or by the sequence of sends and receives.

---

## Compile warnings

The compiler warns about code that is legal but probably not what you meant. Warnings
never fail the build — the binary is produced either way — and each one comes with the
same line-and-caret display as errors:

- **Unused variable** — a `set` or `fixed` binding that is never read (including
  write-only variables that are assigned but never consulted). Prefix the name with an
  underscore to say the discard is deliberate: `fixed _r: i64 = f(x);` calls `f` for
  its effect and never warns.
- **Unreachable code** — a statement after a `return`, `break`, `continue`, `halt`, or
  `exit` in the same block. One exception: a `return` right after `halt`/`exit` is the
  sanctioned idiom for satisfying the all-paths-return check and never warns.
- **Unused result** — a bare statement call of a non-void function (`init(1);`)
  silently discards its return value, a common source of quiet bugs. Bind it —
  `fixed r: i64 = init(1);` — or use an underscore name (`_r`) to say the discard
  is deliberate.
- **Deprecations** — builtins scheduled for removal warn at every call site with the
  replacement to use. (Nothing is deprecated today; the channel ships so future
  deprecations are visible immediately.)

`torvc --no-warn` suppresses them (`-q` does not — warnings are diagnostics, shown
even in quiet builds, which is how they reach you through `rune run`). Loop variables
and function parameters are never flagged.

### `!@` warning directives

Warnings can also be controlled from inside the file, at the top:

```tv
!@NO_WARN;                    // suppress every warning for this compilation
!@ALLOW[unused_variable];     // suppress one category (stackable - one per line)
```

The categories are `unused_variable`, `unreachable_code`, `deprecated`, and `unused_result`. A typo'd
directive or unknown category is a clean compile error, not a silent no-op — a
directive that silently did nothing would be exactly the bug class Torvik hunts.
Because `apply` inlines modules into one compilation unit, a directive covers the
whole compile of the file it appears in, applied modules included. The `!@` prefix
is reserved for warning-system directives, so future controls slot in without new
syntax.

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
- **Lock-free across threads.** Concurrency (see [Ravens and bridges](#concurrency-ravens-and-bridges))
  copies values as they cross thread boundaries, so ordinary refcounts never need atomics. The
  single exception is a bridge's own refcount — a bridge is deliberately shared between threads,
  so that one count is atomic; nothing else in the language is.

---

## Roadmap & limitations

Torvik is deliberately focused. The following are **not** in the language yet — including
official macOS support, which is waiting on real Apple hardware for credible testing. All
are planned for future versions (see [ROADMAP.md](../ROADMAP.md) for the full plan and
reasoning):

- **Structs (`shape`)**.
- **A `pub`** visibility keyword.
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

Concurrency has its own documented edges (see [Ravens and bridges](#concurrency-ravens-and-bridges)):
collections and `result` values don't cross spawns or bridges yet, `bridge_new` needs a
capacity of at least 1 (no unbuffered channels), and there is no `select` over multiple
bridges — each is a clean compile error or panic today and a candidate for a future version.

Every one of these is a clean compile error or a documented narrow case, never a silent wrong
answer or a crash. The full plan is in [ROADMAP.md](../ROADMAP.md). Windows support landed in
v1.1.0 and macOS is not yet supported (planned for a future version once real Apple hardware
is available for testing). The warnings system, `aett` + `when` pattern matching, and Result
types landed in v1.2.0; concurrency — `raven` tasks and `bridge` channels — landed in v1.3.0.

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
| `aett`      | Declare a family of named values (an enumeration)              |
| `when`      | Pattern matching over an aett or integers (`fallback` arm as default) |
| `raven`     | Spawn a `df` function as a concurrent task (fire-and-forget, or bind a `task<T>` handle) |
| `task`      | The type of a raven handle: `task<T>` where `T` is the spawned function's return type |
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
| Logical    | `&&`  `\|\|`  `!`                       | Short-circuiting; usable as values (v1.4.0) |
| Bitwise    | `&`  `\|`  `^`  `~`  `<<`  `>>`         | On integer types                       |
| Assignment | `=`  `+=`  `-=`  `*=`  `/=`  `%=`       | Compound forms update in place         |
| Range      | `..`  `..+`                             | `..` exclusive, `..+` inclusive; used in `each` |
| Ternary    | `?>`  `!>`                              | `cond ?> a !> b`; only the taken branch runs |
| Weave      | `~>`                                    | `x ~> f ~> g` is `g(f(x))`             |
| Variant    | `::`                                    | `Status::Active` reads an aett variant |
| When arm   | `=>`                                    | `pattern => statement-or-block`        |
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

**Boolean expressions are fully chainable (v1.4.0).** `&&` and `||` work as
*values* — bind one to a variable, not just use it in a condition — and chain
any number of operands. A boolean-returning builtin (`contains`, `starts`,
`ends`) or user function can be compared to `true`/`false` directly.

```torvik
fixed ok:   bool = a && b;                     // && / || as a value
fixed all:  bool = a && b && c;                // any number of operands
check contains(line, "error") == false { ... } // bool call vs a literal
fixed clean: bool = starts(path, "/") == false;
```

---

*This guide tracks Torvik v1.4. For the compiler and project tooling, see
[TOOLING.md](TOOLING.md); for the built-in function library, see [STDLIB.md](STDLIB.md).*
