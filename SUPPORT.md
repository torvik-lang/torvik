# Support policy

Every major version of Torvik — and of `rune` and Vefna — is supported for **five
years** from its release, in three stages.

## The stages

| Stage | Years | What it gets |
| --- | --- | --- |
| **Active** | 0 – 3 | New features, bug fixes, security fixes. Minor releases (`1.6.0`, `1.7.0`) happen here. |
| **Maintenance** | 3 – 4 | Bug fixes and security fixes. No new features. Patch releases only. |
| **Security** | 4 – 5 | Security fixes only. |
| **End of life** | 5+ | No further releases. The version keeps working; it stops being updated. |

The rule in one line: **minor releases during Active, patches during Maintenance,
security patches during Security.**

## Current status

| Line | Released | Stage | Active until | Maintenance until | End of life |
| --- | --- | --- | --- | --- | --- |
| **Torvik 1.x** | 4 July 2026 | **Active** | 4 July 2029 | 4 July 2030 | **4 July 2031** |
| **rune 1.x** | 4 July 2026 | **Active** | 4 July 2029 | 4 July 2030 | **4 July 2031** |
| **Vefna 1.x** | 4 July 2026 | **Active** | 4 July 2029 | 4 July 2030 | **4 July 2031** |

The standard library versions independently but tracks the toolchain line it ships
with: std 1.x is supported for as long as Torvik 1.x is.

## What this means in practice

**You are not rushed onto a new major version.** `rune update` deliberately keeps you
inside your current major and only *tells* you a new one exists — see
[TOOLING.md](docs/TOOLING.md). A 1.x project keeps receiving fixes for five years
whether or not 2.0 has shipped.

**Nothing stops working at end of life.** A compiler that built your program last
year will build it next year. What ends is the flow of updates, so the practical
question is how long you want to go without security fixes.

**Upgrading is a decision, not an emergency.** Three years of Active support means a
new major arrives long before the old one is in trouble, and the two years after that
exist so you can move on your own schedule.

## Reporting a security issue

See [SECURITY.md](SECURITY.md). Security fixes are backported to every line still in
Active, Maintenance, or Security support.
