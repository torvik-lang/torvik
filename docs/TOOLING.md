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

On success, `torvc` prints a confirmation with the compile time, for example
`Compiled successfully! (0.18s)`. Use `-q` to silence it.

### Running a file directly

With no `-o`, `torvc` **runs** the file instead of building it — compile, execute,
clean up, no binary left behind:

```bash
torvc hello.tv                      # prints its output
torvc scan.tv report.csv data.csv   # arguments reach the program
```

Anything after the `.tv` file is passed to the program, where `args()` and
`args_get(i)` read it. This is the quickest way to try something out, and it makes a
`.tv` file usable as a script.

### Flags

| Flag                 | Meaning                                             |
|----------------------|-----------------------------------------------------|
| `-o <file>`          | Build to this executable instead of running         |
| `--no-warn`          | Suppress compile warnings (they never fail a build; `!@NO_WARN;` at the top of a file does the same per-file; `-q` does **not** suppress them — warnings are diagnostics) |
| `-q`, `--quiet`      | Suppress the success / timing message               |
| `-v`, `--verbose`    | More detailed output                                |
| `--final`            | Production build (maximum optimization, stripped)   |
| `--version`          | Print the Torvik version                            |
| `-h`, `--help`       | Show usage                                          |

### Freestanding builds

For systems and OS work, `torvc` can produce an image with no operating system
beneath it. See [Forge your first kernel](../examples/kernel/README.md) for the full
walkthrough.

| Flag                  | Meaning                                            |
|-----------------------|----------------------------------------------------|
| `--bare`              | Freestanding image: no libc, no runtime unless the allocator hooks are defined. Implies `--no-std`. |
| `--no-std`            | Refuse `apply std` without going fully freestanding |
| `--entry <name>`      | Entry symbol to emit (default `_start`)             |
| `--arch <triple>`     | Target triple (default `x86_64-unknown-none-elf`)   |
| `--link-script <file>`| Section layout for the linker; its `ENTRY(...)` wins over `--entry` |
| `--link-with <file>`  | Assemble/compile an extra file (a boot stub, say) and link it in |
| `--elf32`             | Rewrite the finished image as a 32-bit ELF. The **code stays 64-bit** — only the container changes. Multiboot 1 loaders (GRUB, `qemu -kernel`) refuse a 64-bit ELF outright, so a multiboot kernel needs this. |

A bare build is decided by the **target**, not by the machine you build on, so the
same command produces the same image on Linux and Windows. You don't need to pass
section or garbage-collection flags — `torvc` adds what a freestanding link needs.

### How compilation works

`torvc` lexes and parses your source, generates LLVM IR, and links it with `clang` to
produce a native binary. You need `clang` available on your `PATH`. Because the compiler is
self-hosting, the same `torvc` that builds your programs is itself built from Torvik source.

---

## `rune` — the project tool

`rune` is the easiest way to work on anything larger than a single file. It uses a simple
convention: a project has a `torvik.rune` manifest and a `src/main.tv` entry point.

> As of v1.4.0 `rune` lives in its own repository
> ([torvik-lang/rune](https://github.com/torvik-lang/rune)) and carries its own version
> number and changelog. It is still part of the Torvik toolchain and installs alongside
> `torvc` — the split just lets it evolve on its own cadence. `rune version` reports both
> its version and the language/std versions.

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
| `rune run <file.tv> [args...]` | Run a single file directly, forwarding arguments |
| `rune list`      | Show project info (alias: `ls`)                         |
| `rune clean`     | Remove the `build/` directory                           |
| `rune version`   | Show tool and language versions                         |
| `rune update`    | Update the toolchain to the latest release (or a pinned version) |
| `rune self-update` | Update just `rune` itself, from its own repository    |
| `rune uninstall` | Remove the Torvik toolchain (`~/.torvik`)               |
| `rune help`      | Show usage                                              |

### Version policy

Updates keep you on your **current major**. A new major version can change or remove
things your code depends on, so `rune` will tell you about one rather than installing
it:

```
Heads up: Torvik v2.0.0 is available - a NEW MAJOR (you're on v1.5.0).
  Major versions may include breaking changes, so rune is keeping you on your
  current major (1.x) and will still pull its newest patches.
  When ready:  rune update v2 --yes    (review the changelog first)
  Silence this notice:  rune update --silence-major
```

You keep getting fixes within your major the whole time. To move up, ask explicitly:

```bash
rune update v2 --yes
```

`--silence-major` quiets the notice until the *next* major ships. It refuses if
there's nothing to silence, so you can't accidentally mute a version that hasn't been
released yet.

### The standard library

std lives in [its own repository](https://github.com/torvik-lang/std) with its own
version line, so it can move independently of the compiler. Pin what your project
needs in `torvik.rune`:

```toml
std = "1.3.0"
```

A pin is an explicit instruction and is honoured, including across a major. Without
one, `rune update` keeps std inside its current major and surfaces a new one the same
way it does for Torvik. To opt in:

```bash
rune update --std-major          # check only - reports, changes nothing
rune update --std-major --yes    # install it, and record it in torvik.rune
```

With `--yes`, rune writes `std = <version>` into your manifest so the project and the
toolchain agree from then on. `--silence-std-major` quiets that notice.

To build with no standard library at all, set `std = no_std`.

### Building a kernel: the `[build]` section

For systems work, declare the build once instead of passing flags every time:

```toml
[project]
name = mykernel
version = 0.1.0

[build]
target = bare                              # bare | native (default native)
entry = kernel_main                        # symbol torvc emits
arch = x86_64-unknown-none-elf
link-script = linker.ld
link-with = boot.s
elf32 = true
runner = qemu-system-x86_64 -kernel {output}
```

Then the usual commands do the right thing:

```bash
rune build      # -> build/mykernel.elf
rune run        # builds, then hands the image to the runner
```

A bare project builds to `.elf` rather than the host's executable extension, and
`rune run` doesn't try to execute it — the host can't. It passes the image to
`runner`, with `{output}` replaced by the built path. Leave `runner` out and rune
builds it and tells you how to add one.

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

Since v1.4.0, `rune` and Torvik release from separate repositories, so `rune update`
checks each **independently** and updates whichever has moved — the toolchain, `rune`
itself, or both. To update only `rune` without touching the toolchain, use
`rune self-update`.

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
std    = "1.1.0"     # likewise for the standard library (it ships with the toolchain)
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

Torvik runs on Linux and Windows (both x86-64). macOS is not yet supported (planned for a future version once real Apple hardware is available for testing); until then, use Linux (a VM or container works). The compiler emits
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
