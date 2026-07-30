/* Torvik reference kernel — linker script.
 *
 * One script, every host. This is GNU linker-script syntax, which both GNU ld and
 * LLVM's ld.lld understand, so the same file works on Linux, macOS and Windows
 * (llvm-mingw bundles ld.lld). Nothing here depends on the machine you build on —
 * the target triple decides the output format, not the host.
 *
 * The Multiboot header is emitted here rather than from Torvik source. A bootloader
 * scans the first 8 KiB of the image for the magic number, so the header has to sit
 * at a known place before any code. Writing it as linker directives keeps the Torvik
 * side free of raw-byte-layout tricks.
 */

ENTRY(_start)

SECTIONS
{
    /* Below 1 MiB is real-mode memory, the VGA window, and firmware. Every
       protected-mode kernel starts at or above 1 MiB. */
    . = 1M;

    /* The Multiboot header itself is defined in boot.s - it only needs placing
       here, first and before any code, where a bootloader will find it. */
    .multiboot ALIGN(4) :
    {
        /* KEEP matters: nothing in the program references this header, so
           --gc-sections would otherwise throw away the very thing that makes the
           image bootable. */
        KEEP(*(.multiboot))
    }

    .text ALIGN(4K) :
    {
        KEEP(*(.text._start))
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Anything the toolchain emits that a freestanding image has no use for:
       exception-handling tables, comments, note sections, dynamic-linking stubs. */
    /DISCARD/ :
    {
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.comment)
        *(.note*)
        *(.gnu*)
    }
}
