# Security Policy

## Supported versions

Torvik is at an early stage. Security fixes are applied to the latest released
version only. There is no backported-support window yet; when that changes, this
section will be updated.

| Version | Supported          |
| ------- | ------------------ |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Reporting a vulnerability

**Please do not report security vulnerabilities through public GitHub issues,
pull requests, or discussions.**

Instead, report privately using GitHub's **[Private vulnerability reporting](https://github.com/torvik-lang/torvik/security/advisories/new)**
("Report a vulnerability" under the repository's **Security** tab). If you can't
use that, contact the maintainer at **<your-email-here>**.

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
