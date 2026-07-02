<div align="center">

<img src="assets/torvik-mark.png" alt="Torvik" width="128" height="128">

# Torvik

**A self-hosting, compiled, general-purpose programming language**

[![Version](https://img.shields.io/badge/version-1.0.0-blue)](https://github.com/torvik-lang/torvik/releases)
[![License](https://img.shields.io/badge/license-AGPL--3.0-green)](LICENSE)
[![Self-Hosting](https://img.shields.io/badge/self--hosting-yes-brightgreen)]()
[![Platform](https://img.shields.io/badge/platform-linux-lightgrey)]()

```torvik
df main() -> void {
    echo!("Hello from Torvik!");
}
```

</div>

---

## What is Torvik?

Torvik is a compiled, statically-typed, general-purpose language with a clean,
Norse-inspired keyword set (`df`, `check`, `whilst`, `each`, `guard`, `echo!`, `rune`). It
compiles to native binaries through LLVM — emitting LLVM IR that is linked with `clang` — so
there is no virtual machine and no garbage collector.

**Highlights:**

- **Native compilation** via an LLVM backend — real executables, no runtime VM.
- **Self-hosting** — the Torvik compiler (`torvc`) and package manager (`rune`) are both
  written in Torvik.
- **Automatic reference counting** — deterministic memory management, no GC, no manual frees.
- **A small, sharp type system** — integers `i8`–`i64`, `u8`–`u64`, the wide `i128`/`u128`,
  `f64`, `bool`, `str`, and the `list`, `table`, and `bag` collections.
- **`rune` project tool** — create, build, and run projects with one command.

---

## Install

Install the latest release with one command — it places `torvc` and `rune` in `~/.torvik`,
fetches the runtime and standard library, and adds Torvik to your `PATH`:

```sh
curl -fsSL https://raw.githubusercontent.com/torvik-lang/torvik/main/install.sh | sh
```

You also need `clang` installed — Torvik uses it as its linker and back-end
(`sudo eopkg install clang` on Solus, `sudo apt install clang` on Debian/Ubuntu,
`xcode-select --install` on macOS). Then restart your shell (or run `. ~/.bashrc`) and confirm
with `rune --version`. Rune manages Torvik from there — `rune uninstall` removes it.

**Supported platforms:**

| Platform            | Architecture | Status          |
|---------------------|--------------|-----------------|
| Linux               | x86_64       | Supported       |
| macOS               | x86_64 / arm | Experimental    |
| Windows             | —            | Planned (v1.1.0)|

> Torvik is 64-bit only.

---

## Update

Already have Torvik? Re-run the same one-liner to update to the latest release:

```sh
curl -fsSL https://raw.githubusercontent.com/torvik-lang/torvik/main/install.sh | sh
```

It refreshes `torvc`, `rune`, the runtime, and the standard library in `~/.torvik` — your
projects, config, and PATH setup are untouched. Check what you're running with
`rune version`.

> A built-in `rune update` command is planned; until it lands, re-running the installer is
> the supported update path.

---

## Quick start

```bash
# Create and run a project
rune new myapp
cd myapp
rune run
```

Or compile a single file directly:

```bash
torvc hello.tv -o hello
./hello
```

---

## A taste of the language

```torvik
df factorial(n: i64) -> i64 {
    check n <= 1 { return 1; }
    return n * factorial(n - 1);
}

df classify(n: i64) -> str {
    guard n != 0 fallback { return "zero"; }
    return n > 0 ?> "positive" !> "negative";
}

df main() -> void {
    each i in 1..6 {
        set f: i64 = factorial(i);
        echo!("{i}! = {f}");
    }

    set scores: table<str, i64> = table_new();
    table_set(scores, "Freya", 10);
    set freya: i64 = table_get(scores, "Freya");
    echo!("Freya: {freya}");

    echo!(classify(-7));
}
```

---

## Documentation

- **[The Torvik Guide](docs/GUIDE.md)** — full tutorial and language reference.
- **[Tooling](docs/TOOLING.md)** — the `torvc` compiler and the `rune` project tool.
- **[Standard library](docs/STDLIB.md)** — built-in function reference.

---

## Toolchain

| Tool    | Purpose                      |
|---------|------------------------------|
| `torvc` | Compiler                     |
| `rune`  | Project & build tool         |

```bash
torvc myfile.tv -o myfile     # compile
torvc myfile.tv --final       # production build
rune new myapp                # create a project
rune build                    # compile the project
rune run                      # compile and run
```

---

## Self-hosting

Torvik compiles itself. The compiler and package manager are written entirely in Torvik
(`.tv`) source under `src/`, and a Torvik binary rebuilds them — there is no other language
in the bootstrap. A clean self-rebuild reproduces the compiler bit-for-bit.

---

## License

Torvik is licensed under the **GNU AGPL-3.0** with a runtime-library exception, so programs
you write in Torvik are entirely your own and carry no license obligation from the compiler.
See [LICENSE](LICENSE) for the full text.
