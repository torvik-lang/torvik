# The standard library has moved

As of **Torvik v1.5.0** the standard library lives in its own repository:

**[github.com/torvik-lang/std](https://github.com/torvik-lang/std)**

The full reference is there, in [`docs/STDLIB.md`](https://github.com/torvik-lang/std/blob/main/docs/STDLIB.md).

## Why

std now carries its own version line, independent of the compiler. A project can
hold the standard library on one major version while the toolchain moves on, and a
breaking change to std no longer forces a compiler release (or the reverse).

Nothing changes in how you use it:

```torvik
apply std;              // the whole library
apply std::math;        // or one part
```

It still ships with the toolchain — the installers fetch it from the std repository,
and `rune update` keeps it current. You can pin the version your project needs in
`torvik.rune`:

```
std = "1.3.0"
```

`rune` will not move you across a **major** std version without being told to. See
[TOOLING.md](TOOLING.md) for `rune update --std-major` and the notice controls.
