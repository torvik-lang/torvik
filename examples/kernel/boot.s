/* Torvik reference kernel — boot stub.
 *
 * Multiboot hands control to a kernel in 32-BIT protected mode. Torvik compiles to
 * 64-bit code. Something has to bridge the two, and that something cannot be written
 * in Torvik: it runs before long mode exists, so it cannot be 64-bit code.
 *
 * This is that bridge. It runs in 32-bit mode, builds the page tables and the 64-bit
 * GDT the processor needs, switches the CPU into long mode, and jumps to the Torvik
 * entry point. Roughly sixty lines, once, and then you are writing Torvik.
 *
 * Assembled by torvc via:  --link-with boot.s
 */

.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000003          /* align modules, supply a memory map */
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

/* ── Multiboot header ──────────────────────────────────────────────────────
   A bootloader scans the first 8 KiB for this, so it must land before any code. */
.section .multiboot, "a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

/* ── Page tables and stack ────────────────────────────────────────────────
   Long mode requires paging to already be on before it will start, so the tables
   have to exist before the switch. .bss is zeroed by the loader. */
.section .bss, "aw", @nobits
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096
.align 16
stack_bottom:
    .skip 16384                        /* 16 KiB is plenty for a kernel this size */
stack_top:

/* ── 64-bit GDT ───────────────────────────────────────────────────────────
   Long mode largely ignores segmentation, but it still insists on a code segment
   descriptor with the long-mode bit set before it will execute 64-bit code. */
.section .rodata
.align 8
gdt64:
    .quad 0                            /* null descriptor */
gdt64_code = . - gdt64
    .quad (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)
    /*      executable   code/data    present     64-bit  */
gdt64_pointer:
    .word gdt64_pointer - gdt64 - 1    /* limit */
    .quad gdt64                        /* base  */

.section .text
.code32
.global _start
.type _start, @function
_start:
    movl $stack_top, %esp

    /* PML4[0] -> PDPT, PDPT[0] -> PD. Present + writable. */
    movl $pdpt, %eax
    orl  $0x3, %eax
    movl %eax, pml4

    movl $pd, %eax
    orl  $0x3, %eax
    movl %eax, pdpt

    /* Identity-map the first gigabyte with 2 MiB pages: PD[i] = (i * 2MiB) | 0x83.
       0x83 = present | writable | page-size. Identity mapping means a physical
       address and its virtual address are the same number, which keeps everything
       that follows straightforward. */
    xorl %ecx, %ecx
1:
    movl $0x200000, %eax
    mull %ecx                          /* edx:eax = 2 MiB * ecx */
    orl  $0x83, %eax
    movl %eax, pd(, %ecx, 8)           /* low dword; high dword stays zero */
    incl %ecx
    cmpl $512, %ecx
    jne  1b

    /* CR3 points the processor at the top-level table. */
    movl $pml4, %eax
    movl %eax, %cr3

    /* CR4.PAE — long mode is built on physical address extension. */
    movl %cr4, %eax
    orl  $(1 << 5), %eax
    movl %eax, %cr4

    /* EFER.LME — arm long mode. It does not engage until paging is enabled. */
    movl $0xC0000080, %ecx
    rdmsr
    orl  $(1 << 8), %eax
    wrmsr

    /* CR0.PG — enable paging. The processor is now in compatibility mode. */
    movl %cr0, %eax
    orl  $(1 << 31), %eax
    movl %eax, %cr0

    /* Load the 64-bit GDT and far-jump into it. The far jump is what actually
       reloads CS and puts the processor into 64-bit mode. */
    lgdt gdt64_pointer
    ljmp $gdt64_code, $long_mode_entry

.code64
long_mode_entry:
    /* Long mode ignores the data segments, but stale 32-bit selectors can still
       confuse things. Zero them. */
    xorw %ax, %ax
    movw %ax, %ss
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* ── Enable SSE ────────────────────────────────────────────────────────
       This is not optional, and leaving it out produces one of the most
       confusing failures in kernel work: the machine boots, prints a few
       lines, and then silently reboots forever.

       Here is why. A compiler targeting x86_64 assumes SSE is available - it is
       part of the base architecture - so it emits SSE instructions freely, even
       for something as ordinary as zeroing a 20-byte array. But the processor
       comes out of reset with SSE *disabled*, and executing an SSE instruction
       while CR4.OSFXSR is clear raises #UD. With no interrupt descriptor table
       yet, that exception has nowhere to go: #UD escalates to a double fault,
       the double fault has no handler either, and the CPU triple-faults and
       resets. Hence the boot loop, always at whatever line first touched a wide
       memory operation.

       CR0.EM off, CR0.MP on   - use the FPU/SSE, don't emulate it.
       CR4.OSFXSR on           - enable SSE and the FXSAVE/FXRSTOR area.
       CR4.OSXMMEXCPT on       - report SSE numeric errors as #XM.  */
    movq %cr0, %rax
    andq $0xFFFFFFFFFFFFFFFB, %rax   /* clear EM  (bit 2) */
    orq  $0x2, %rax                  /* set   MP  (bit 1) */
    movq %rax, %cr0

    movq %cr4, %rax
    orq  $0x600, %rax                /* set OSFXSR (bit 9) + OSXMMEXCPT (bit 10) */
    movq %rax, %cr4

    /* Into Torvik. kernel_main is the symbol torvc emits for --entry kernel_main. */
    call kernel_main

    /* If it ever returns, stop cleanly rather than running off into whatever
       happens to be next in memory. */
2:
    cli
    hlt
    jmp 2b
