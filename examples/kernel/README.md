# Forge your first kernel

A kernel is just a program with nothing underneath it. No operating system, no C
library, no runtime you didn't ask for — the machine hands you control and you keep
it. This walks through building one in Torvik, from three lines to a screenful of
text driven by real hardware.

Everything here is in this directory and builds today:

| File | What it is |
| --- | --- |
| `hello_kernel.tv` | The smallest thing that counts as a kernel — two characters and a halt |
| `heap_kernel.tv` | Adds an allocator so heap types work |
| `kernel.tv` | The reference kernel: VGA text, port I/O, allocator, cursor control |
| `boot.s` | The 32-bit boot stub that switches the CPU into long mode |
| `linker.ld` | Where the pieces go in memory |

---

## What you need

- A Torvik toolchain (`torvc version` should answer).
- `clang` with a linker — this is what torvc already uses to build ordinary programs.
- QEMU, to run it: `qemu-system-x86_64`.

Nothing else. In particular you do **not** need a cross-compiler. A freestanding
build is decided by the *target*, not by the machine you build on, so the same
command on Linux, macOS or Windows produces the same ELF kernel.

---

## 1. The smallest kernel

```torvik
df main() -> void {
    unsafe set vga: varda<u16> = from_addr(0xB8000);
    unsafe store_vol(vga, 0x0F48);
    whilst true {
        unsafe hlt();
    }
}
```

Build and look at it:

```sh
torvc hello_kernel.tv --bare -o hello.elf
```

That's a real freestanding image: statically linked, no libc, entry at `_start`.

Three things are going on.

**`from_addr(0xB8000)`** makes a raw pointer to a fixed physical address. On a PC
the VGA text buffer lives there: 80×25 cells, each cell a `u16` holding a character
byte and a colour byte. Writing to that address puts something on screen — there is
no driver in between.

**`store_vol`** writes *volatilely*. An ordinary store to memory that nothing else
reads is dead code, and the optimiser is within its rights to delete it. Volatile
says "the write itself is the point". Every hardware register access wants this.

**`hlt`** stops the processor until an interrupt arrives. Since we never enable
interrupts, it stops for good — and unlike `whilst true { }` it doesn't burn a core
doing it.

Notice that all three are `unsafe`. Torvik doesn't have an unsafe *mode*; it has
unsafe *statements*, and you write the word each time you reach for one. The safe
surface stays safe: `buf[i]` is still bounds-checked in the next function down.

---

## 2. Making it bootable

`hello.elf` runs nothing yet, because a bootloader doesn't know what to do with it.
The fix is a Multiboot header — a magic number in the first 8 KiB that says "I am a
kernel, load me at 1 MiB and jump to my entry point".

That's what `linker.ld` adds:

```ld
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4) :
    {
        LONG(0x1BADB002)
        LONG(0x00000003)
        LONG(-(0x1BADB002 + 0x00000003))
    }

    .text ALIGN(4K) : { *(.text) *(.text.*) }
    /* ... rodata, data, bss ... */
}
```

Three values: the magic number, some flags, and a checksum chosen so all three sum
to zero. The header is written as linker directives rather than Torvik data, because
it must sit at an exact place before any code.

`. = 1M;` puts the kernel at the one-megabyte mark. Everything below is real-mode
memory, the VGA window and firmware — that region isn't yours.

### The part nobody tells you about

There is a catch, and it is the one that trips up every first kernel: **Multiboot
hands control to you in 32-bit protected mode.** Torvik compiles to 64-bit code. Jump
straight from one to the other and the processor misreads your first instruction and
dies.

So something has to bridge the gap, and it cannot be written in Torvik — it runs
*before* long mode exists, so by definition it is not 64-bit code. That is `boot.s`,
and it is the only assembly in the project:

1. Build page tables — long mode will not start without paging already enabled.
2. Identity-map the first gigabyte with 2 MiB pages, so physical and virtual
   addresses are the same number and nothing below you moves.
3. Set `CR4.PAE`, then `EFER.LME`, then `CR0.PG` — in that order. Long mode is armed
   by the MSR and engaged by enabling paging.
4. Load a 64-bit GDT and far-jump into it. The far jump is what actually reloads `CS`
   and puts the processor in 64-bit mode.
5. `call kernel_main` — and from here on, you are writing Torvik.

Sixty lines, once, ever.

Now build it:

```sh
torvc kernel.tv --bare --elf32 --entry kernel_main \
      --link-script linker.ld --link-with boot.s -o kernel.elf

qemu-system-x86_64 -kernel kernel.elf
```

`--elf32` matters as much as the boot stub: Multiboot 1 will not load a 64-bit ELF at
all — QEMU says *"Cannot load x86-64 image, give a 32bit one"* — so the finished image
is rewritten as a 32-bit container. The code inside stays 64-bit; only the wrapper
changes, which is safe because the link is already complete.

`--link-with` assembles `boot.s` for the same target and links it alongside your
code. `--entry kernel_main` names the symbol torvc emits for your `main`, which is
what `boot.s` calls once the processor is in long mode. You still write `df main()`
as normal.

A window opens with text in it. That text came from a `.tv` file.

### Building it with rune

Passing four flags every time gets old. `rune` is the project manager, and a
`[build]` section in `torvik.rune` says all of it once:

```toml
[project]
name = mykernel
version = 0.1.0

[build]
target = bare
entry = kernel_main
arch = x86_64-unknown-none-elf
link-script = linker.ld
link-with = boot.s
elf32 = true
runner = qemu-system-x86_64 -kernel {output}
```

Then the usual commands do the right thing:

```sh
rune build      # -> build/mykernel.elf
rune run        # builds, then hands the image to the runner
```

A bare project builds to `.elf` rather than the host's executable extension, because
calling a kernel `mykernel.exe` would be a lie. `rune run` won't try to execute it
either — the host can't. It passes the image to whatever `runner` names, with
`{output}` replaced by the built path. Leave `runner` out and rune builds the image
and tells you how to add one.

One detail worth knowing if you write your own linker script: the Multiboot header
must be wrapped in `KEEP(...)`. Nothing in the program references it, so
`--gc-sections` will otherwise discard the one thing that makes the image bootable —
and the failure is silent. The image links fine and simply never boots.

---

## 3. Talking to hardware two ways

The reference kernel uses both mechanisms a PC offers.

**Memory-mapped I/O** is the VGA buffer: a device pretending to be memory. You write
with a volatile store and the hardware notices.

```torvik
df vga_put(row: i64, col: i64, ch: i64, colour: i64) -> void {
    fixed offset: i64 = (row * VGA_COLS) + col;
    unsafe set base: varda<u16> = from_addr(VGA_BASE);
    unsafe set cell: varda<u16> = ptr_add(base, offset);
    unsafe store_vol(cell, vga_cell(ch, colour));
}
```

`ptr_add` moves by *elements* — `ptr_add(base, 3)` on a `varda<u16>` advances six
bytes. When you want raw bytes, `ptr_byte_offset` does that instead. Two names
because mixing them up is a bug that looks like working code.

**Port I/O** is the other mechanism: a separate address space reached with dedicated
instructions. The VGA cursor lives there.

```torvik
df vga_move_cursor(row: i64, col: i64) -> void {
    fixed pos: i64 = (row * VGA_COLS) + col;
    fixed hi: i64 = pos / 256;
    fixed lo: i64 = pos - (hi * 256);
    unsafe outb(CRTC_INDEX, 14);
    unsafe outb(CRTC_DATA, hi);
    unsafe outb(CRTC_INDEX, 15);
    unsafe outb(CRTC_DATA, lo);
}
```

`outb`/`inb` (and the `w`/`l` widths) are named intrinsics rather than raw assembly,
so the register constraints live in the compiler where they're written once and
tested, instead of in every kernel. They compile to exactly the instruction you'd
write by hand — `out %al,(%dx)`.

When you need something with no intrinsic, drop to assembly directly:

```torvik
unsafe galdr { "cli"; "hlt"; }
```

A `galdr` block is inline assembly: one instruction per string. It's emitted with
`sideeffect`, so the optimiser leaves it exactly where you put it.

---

## 4. Memory, if you want it

Try to use a `str` or a `list` in a bare build and the link fails, because those
bottom out in `malloc` and there is no C library. Torvik doesn't paper over that —
it asks you to say where memory comes from:

```torvik
df on_alloc(size: i64) -> varda<u8> { ... }
df on_free(p: varda<u8>) -> void { ... }
```

Define both and torvc wires the runtime's allocation onto them, then links the
runtime. Leave them out and the runtime isn't linked at all — which is why
`hello_kernel.tv` produces such a small image.

The reference kernel uses a bump allocator: hand out aligned blocks from a fixed
region, never reclaim. That's a legitimate design for a kernel this size, and it's
about fifteen lines.

```torvik
df on_alloc(size: i64) -> varda<u8> {
    set n: i64 = size;
    fixed rem: i64 = n - ((n / 16) * 16);
    check rem != 0 { n = n + (16 - rem); }
    check arena_off + n > ARENA_SIZE {
        unsafe set nul: varda<u8> = from_addr(0);
        return nul;
    }
    fixed at: i64 = ARENA_BASE + arena_off;
    arena_off = arena_off + n;
    unsafe set block: varda<u8> = from_addr(at);
    return block;
}
```

Returning null when the arena is exhausted is the contract. The runtime panics, and
a panic in a freestanding image parks the machine — there is nowhere to report to.
If you want diagnostics, write them to the screen or a serial port yourself.

---

## 5. What the compiler refuses

Ask for something that needs an operating system and you get told at compile time,
not at link time:

```
error: 'readfile' needs an operating system, and this is a freestanding build
(--bare) - there is nothing underneath to service it. Talk to the hardware instead:
volatile stores for memory-mapped I/O, outb/inb for ports.
```

File I/O, `args()`, `time_ms`, `sys_run` — all refused under `--bare`. `apply std`
is refused too, for the same reason.

What still works is everything that doesn't need a kernel: arithmetic, `check` and
`whilst`, functions, `shape` structs, fixed arrays, raw pointers, and `size_of` /
`align_of` — which are compile-time layout facts and so are perfectly happy bare.

---

## 6. Useful flags

| Flag | What it does |
| --- | --- |
| `--bare` | Freestanding build: no libc, no runtime unless the hooks are defined. Implies `--no-std`. |
| `--no-std` | Refuse `apply std` without going fully freestanding. |
| `--entry NAME` | Entry symbol (default `_start`). |
| `--link-script FILE` | Hand the linker your section layout. Its `ENTRY(...)` wins over `--entry`. |
| `--link-with FILE` | Assemble/compile an extra file (a boot stub, say) and link it in. |
| `--arch TRIPLE` | Target triple (default `x86_64-unknown-none-elf`). |
| `--final` | Optimise for release. |

You don't need to pass section or garbage-collection flags — torvc adds what a
freestanding link needs.

---

## Where to go next

The kernel here prints and halts. The next steps are the classic ones: a GDT, an
interrupt descriptor table so the keyboard can talk to you, a timer, then paging and
a real allocator behind `on_alloc`. Nothing in the Forge stops you: interrupt
handlers are ordinary functions plus `galdr`, and descriptor tables are `packed
shape`s written into memory through a raw pointer.

`packed shape` is worth a mention there. A normal `shape` lets the compiler insert
padding for alignment; a `packed` one doesn't. Hardware structures are defined byte
by byte, so they must be packed:

```torvik
packed shape GdtEntry {
    limit_low: u16,
    base_low: u16,
    base_mid: u8,
    access: u8,
    flags: u8,
    base_high: u8
}
```

`size_of(GdtEntry)` reports 8, as the manual says it should. Take `addr_of` a field
and you have a pointer to write through.
