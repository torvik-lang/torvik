# Torvik — maintainer documentation

This directory holds documentation for people working **on** the Torvik toolchain, as
opposed to people writing programs **in** Torvik (whose docs live in [`../docs`](../docs)).

- [`error-codes.md`](error-codes.md) — the registry of stable internal-error codes emitted
  by `torvc` and `rune`, and how the diagnostics log works.

## Maintainer-only scripts (not for the public release)

These build and self-install the toolchain from a local checkout. They are used while
developing Torvik and are **removed before the public GitHub upload** — end users never
interact with them (they use `install.sh` / `windows/install.ps1` and `rune update` instead):

| Linux / macOS      | Windows                     | Purpose                                              |
|--------------------|-----------------------------|------------------------------------------------------|
| `fixpoint.sh`      | `windows/fixpoint.ps1`      | Rebuild the compiler and verify the self-hosting fixpoint |
| `setup_local.sh`   | `windows/setup_local.ps1`   | Install the toolchain from the local checkout        |

The public-facing installers — `linux/install.sh` and `windows/install.ps1` (with a
compatibility shim at the repo root) — are
self-contained and do **not** depend on these, so removing the maintainer scripts is safe.
