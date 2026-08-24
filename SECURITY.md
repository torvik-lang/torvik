# Security Policy

## Supported versions

Every major version is supported for five years: three of **Active** development
(features, bugs, security), one of **Maintenance** (bugs and security), and one of
**Security** fixes only. Security fixes are backported to every line still inside
that window.

| Line | Stage | Security fixes until |
| ---- | ----- | -------------------- |
| 1.x  | **Active** | 4 July 2031 |
| < 1.0 | End of life | — |

The full schedule, including the Active and Maintenance boundaries, is in
[SUPPORT.md](SUPPORT.md).

## Advisories

### TV-2026-003 — shell injection via paths (fixed in 1.5.4)

**Severity:** high. **Affected:** all 1.x up to and including 1.5.3.
**Fixed in:** torvc 1.5.4.

Every path the compiler puts on a command line — the output path from `-o`, the
run directory derived from `HOME`, temporary objects, link scripts — went through
one quoting helper that wrapped it in **double quotes**. Double quotes do not stop
command substitution: `$(...)` and backticks expand inside them. So a path
containing shell syntax executed with the privileges of whoever ran `torvc`.

Two vectors were confirmed:

    torvc x.tv -o '$(command)'
    HOME='/tmp/x$(command)' torvc x.tv run

The second matters more than it looks. Any context where `HOME` comes from
configuration rather than from a login — CI runners, container entrypoints,
service accounts, wrapper scripts — was exposed without the caller passing
anything unusual.

**Fix.** The quoting helper now REFUSES a path containing shell syntax instead of
quoting it. The check lives in that one function, so all seventeen call sites are
covered at once.

---

### A note on this family of issues

TV-2026-003 is the seventh command-injection issue found in Torvik, and the last
of a family. They shared one cause: the compiler builds command lines for `clang`
and the linker, and values reached those command lines without being checked.

Each earlier fix was correct and incomplete — it closed the site that had been
found and left the others open. Manifest fields were fixed in 1.5.2, `torvc run`
arguments in 1.5.3, and this release fixes the path helper that all of them
ultimately passed through.

**What changed beyond the fix.** The rule now is that no path or user-supplied
string reaches a command line without passing a validator, and the validators
refuse rather than escape. Escaping has to be correct on two different shells and
stays correct only until either changes; refusing does not.

**If you write tooling around torvc**, this is worth a look at your own wrappers.
A script that builds a `torvc` command line from a filename, a branch name, or any
value it did not choose has the same problem, and updating Torvik does not fix
your script.

**Credit:** found during internal review. No reports of exploitation.

### TV-2026-002 — command injection via `torvc run` arguments (fixed in 1.5.3)

**Severity:** high. **Affected:** all 1.x up to and including 1.5.2.
**Fixed in:** torvc 1.5.3.

Arguments forwarded to a program under `torvc run` were placed into a command line
handed to a shell. Each argument was double-quoted, which prevents word-splitting
but **not command substitution** — `$(...)` and backticks expand inside double
quotes — so an argument containing shell syntax could execute arbitrary commands
with the privileges of the user running `torvc`.

    torvc run prog.tv '$(rm -rf ~/data)'

This matters wherever the arguments are not typed by the person running the
command: a CI job, a wrapper script, a task runner, anything passing a filename or
a field it read from data.

**Fix.** An argument containing shell syntax is now refused with a clear message
rather than quoted and hoped for. Quoting was never sufficient here, which is why
the argument is rejected instead. To pass such an argument, compile with `-o` and
run the binary directly — no shell is involved on that path.

This is the same class as TV-2026-001 and the sixth issue found in this family
during a review of every place the compiler builds a command line.

**Credit:** found during internal review; no reports of exploitation.

### TV-2026-001 — command injection via build configuration (fixed in 1.5.2)

**Severity: critical. Update promptly.**

`torvc` interpolated the `--arch` target triple into the `clang` command line
without validating it, so a value containing shell syntax executed at build time.
The same value can be set from a project's `torvik.rune` manifest, which means the
vulnerability was reachable by cloning a repository and running an ordinary
`rune build` — no need to run the resulting binary, and no warning that anything
unusual happened.

`--entry` was affected in the same way. It is written directly into generated LLVM
IR, so a crafted value could inject arbitrary IR into the compiled program.

Both values are now validated against an allowlist: a target triple may contain
letters, digits, and `_ . -`; an entry symbol follows C identifier rules. Anything
else is refused with a clear message rather than passed to a shell.

**Affected:** Torvik 1.5.0 and 1.5.1 (the `--arch` flag was introduced in 1.5.0).
**Fixed in:** 1.5.2.
**Companion fix:** `rune` 1.5.1 validates the same fields in `torvik.rune` before
building its own command line, so both tools must be updated. See that project's
advisory.

**What to do:** update both `torvc` and `rune`. If you have built an untrusted or
unfamiliar project with an affected version, treat that build as having run
arbitrary code with your user's privileges.

Quoting was not a sufficient fix and was not used as one. Shell double-quoting
prevents word-splitting but **does not prevent command substitution** — `"$(...)"`
still expands — so every value that reaches a shell is validated instead.

## Reporting a vulnerability

**Please do not report security vulnerabilities through public GitHub issues,
pull requests, or discussions.**

Instead, report privately using GitHub's **[Private vulnerability reporting]**
("Report a vulnerability" under the repository's **Security** tab). If you can't
use that, contact the maintainer at **torviklang@gmail.com**.

Please include as much of the following as you can:

- The type of issue (for example: arbitrary code execution during compilation or
  `rune` build, path traversal, unsafe file writes, command injection via
  `clang`/`sys_run`, install-script or supply-chain concerns).
- The affected component (`torvc`, `rune`, the runtime, the install script, or a
  standard-library module) and version.
- Steps to reproduce, ideally with a minimal `.tv` file or command.
- The impact as you see it, and any suggested fix.

## What to expect

Torvik is currently maintained by a single developer, so please allow time for a
response.

- **Acknowledgement:** I aim to confirm I've received your report within about a
  week.
- **Updates:** I'll keep you posted on whether the report is accepted, needs more
  information, or is declined, and on progress toward a fix.
- **Disclosure:** I ask that you keep the report private until a fix is released.
  I'm happy to credit you in the release notes and the advisory unless you'd
  prefer to remain anonymous.

## Scope

Reports about the Torvik toolchain itself are in scope: the compiler (`torvc`),
the package manager (`rune`), the runtime, the standard library, and the official
install script. Issues in your own Torvik programs, or in third-party code, are
not — though I'm glad to hear about anything that looks like a language or
compiler flaw enabling them.

Because Torvik compiles to native code and links with `clang`, a `.tv` program can
do anything a native program can once you build and run it. Compiling or running
untrusted `.tv` source is therefore no safer than running any untrusted program —
that's expected behavior, not a vulnerability. Reports about the *toolchain* being
made to do something unintended (for example, a crafted source file or manifest
causing `torvc`/`rune` to act outside the file it was asked to build) are in scope.
