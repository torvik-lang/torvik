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

If you omit `-o`, the output name is derived from the source file: `torvc app.tv` writes
`./app` (`app.exe` on Windows) in the current directory.

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
version     = "0.1.0"
description = ""
author      = ""

[runes]
# Dependencies (coming with the Rune Library, a future version — timeline depends on registry infrastructure and sponsorship)
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
| `rune update`    | Update the toolchain to the latest release (or a pinned version) |
| `rune uninstall` | Remove the Torvik toolchain (`~/.torvik`)               |
| `rune help`      | Show usage                                              |

### Managing toolchain versions

`rune update` installs the latest Torvik release. You can also **pin a specific version** by
passing it:

```bash
rune update            # latest release
rune update v1.1.0     # exactly 1.1.0
rune update v1.0       # the newest 1.0.x release
rune update v1         # the newest 1.x release
```

The version accepts an optional leading `v` and one, two, or three components. A partial
version (`v1`, `v1.0`) resolves to the newest matching release. If the version doesn't exist,
`rune` reports it cleanly and leaves your current toolchain in place. Add `--yes` to skip the
confirmation prompt.

A bare `rune update` first checks whether a newer release exists. If you're already on the
latest, it says so and does nothing — pass `--force` to reinstall anyway. If the latest is a
new **major** version (e.g. you're on 1.x and 2.0.0 is out), `rune` warns that major versions
may contain breaking changes and asks you to confirm before updating, so an update never
silently jumps a major boundary. Pinning an exact version with `rune update vX.Y.Z` skips
these checks and installs what you asked for.

Projects can require a **minimum** Torvik version in their `torvik.rune` manifest:

```
[project]
name   = "myapp"
torvik = "1.1.0"     # rune build/run refuses an older toolchain
```

If the installed toolchain is older than a project requires, `rune build` and `rune run` stop
with a clear message pointing you at `rune update`. `rune new` records the current version in
new projects automatically; remove or lower the line to relax the requirement.

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
with the Rune Library in a future release. For now a project is your own `.tv` sources under
`src/`, composed with [`apply`](GUIDE.md#modules-apply).

---

*For the language itself, see [GUIDE.md](GUIDE.md); for built-in functions, see
[STDLIB.md](STDLIB.md).*

## Platforms

Torvik runs on Linux and Windows (both x86-64). macOS is not yet supported (official builds planned for v1.2.0); until then, use Linux (a VM or container works). The compiler emits
LLVM IR and links it with `clang`, so a program behaves identically on every platform — only
the toolchain binaries and the installer differ.

### Windows

The Windows toolchain ships as `torvc.exe` and `rune.exe`. Install it from PowerShell:

```powershell
iwr -useb https://raw.githubusercontent.com/torvik-lang/torvik/main/windows/install.ps1 | iex
```

This places the binaries under `%USERPROFILE%\.torvik\bin`, the runtime and standard
library under `%USERPROFILE%\.torvik\lib`, and adds the `bin` directory to your user `PATH`.

As the back-end, `torvc` needs a clang that includes the **MinGW-w64** C headers and
libraries on `PATH` — plain LLVM alone does not include the C headers, and you'll get a
`'stdio.h' file not found` error. Install one of: [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw/releases)
(simplest), MSYS2 (`pacman -S mingw-w64-clang-x86_64-toolchain`), or [WinLibs](https://winlibs.com)
(UCRT build), and put its `bin` directory on `PATH`. `torvc` targets `x86_64-w64-windows-gnu`
and writes its intermediate IR to `%TEMP%\.torvik`.

Everything works the same as on Linux: `rune new`, `rune build`, `rune run`, `rune update`,
`rune uninstall`. Compiled programs are `.exe` files.
