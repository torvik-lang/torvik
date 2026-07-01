# Torvik Tooling: `torvc` and `rune`

Torvik ships with two command-line tools:

- **`torvc`** — the compiler. Turns a `.tv` source file into a native executable.
- **`rune`** — the project tool. Creates projects, builds and runs them, and manages the
  build directory.

Both are themselves written in Torvik.

---

## `torvc` — the compiler

### Compiling a single file

```bash
torvc hello.tv -o hello     # compile hello.tv to ./hello
./hello
```

If you omit `-o`, the output name is derived from the source file.

On success, `torvc` prints a confirmation with the compile time, for example
`Compiled successfully! (0.18s)`. Use `-q` to silence it.

### Flags

| Flag              | Meaning                                             |
|-------------------|-----------------------------------------------------|
| `-o <file>`       | Set the output executable name                      |
| `-q`, `--quiet`   | Suppress the success / timing message               |
| `-v`, `--verbose` | More detailed output                                |
| `--final`         | Production build (maximum optimization, stripped)   |
| `--version`       | Print the Torvik version                            |
| `-h`, `--help`    | Show usage                                          |

### How compilation works

`torvc` lexes and parses your source, generates LLVM IR, and links it with `clang` to
produce a native binary. You need `clang` available on your `PATH`. Because the compiler is
self-hosting, the same `torvc` that builds your programs is itself built from Torvik source.

---

## `rune` — the project tool

`rune` is the easiest way to work on anything larger than a single file. It uses a simple
convention: a project has a `torvik.rune` manifest and a `src/main.tv` entry point.

### Creating a project

```bash
rune new myapp      # create ./myapp with a manifest and src/main.tv
cd myapp
```

`rune init` does the same thing in the current directory.

A new project looks like:

```
myapp/
  torvik.rune        # project manifest
  src/
    main.tv          # entry point (df main)
  .gitignore
```

The manifest is straightforward:

```
[project]
name        = "myapp"
version     = "0.0.1"
description = ""
author      = ""

[runes]
# Dependencies (coming with the Rune Library; not yet supported)
```

### Building and running

```bash
rune build      # compile to build/<name>
rune run        # compile (incrementally) and run
```

`rune run` is incremental: it recompiles only when your sources change, so repeated runs
are fast. The build timer reports how long compilation took.

### Other commands

| Command          | What it does                                            |
|------------------|---------------------------------------------------------|
| `rune new <name>`| Create a new project in a new directory                 |
| `rune init [name]`| Create a project in the current directory              |
| `rune build`     | Compile the project to `build/<name>`                   |
| `rune run`       | Compile incrementally and run                           |
| `rune list`      | Show project info (alias: `ls`)                         |
| `rune clean`     | Remove the `build/` directory                           |
| `rune version`   | Show tool and language versions                         |
| `rune uninstall` | Remove the Torvik toolchain (`~/.torvik`)               |
| `rune help`      | Show usage                                              |

### Production builds

```bash
rune build --final     # maximum optimization, stripped binary
```

Use `--final` for release artifacts; the default build is tuned for fast iteration.

### Exit codes

`rune run` and `rune build` propagate the program's (or compiler's) exit code, so they fit
cleanly into scripts and CI: a non-zero status from your program — including a `halt` or a
failed `vow` — is reported faithfully by `rune`.

---

## A note on dependencies

The `[runes]` section of the manifest is reserved for external dependencies, which arrive
with the Rune Library in a future release. In v1.0 a project is your own `.tv` sources under
`src/`, composed with [`apply`](GUIDE.md#modules-apply).

---

*For the language itself, see [GUIDE.md](GUIDE.md); for built-in functions, see
[STDLIB.md](STDLIB.md).*
