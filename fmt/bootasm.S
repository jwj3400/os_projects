9300 #include "asm.h"
9301 #include "memlayout.h"
9302 #include "mmu.h"
9303 
9304 # Start the first CPU: switch to 32-bit protected mode, jump into C.
9305 # The BIOS loads this code from the first sector of the hard disk into
9306 # memory at physical address 0x7c00 and starts executing in real mode
9307 # with %cs=0 %ip=7c00.
9308 
9309 .code16                       # Assemble for 16-bit mode
9310 .globl start
9311 start:
9312   cli                         # BIOS enabled interrupts; disable
9313 
9314   # Zero data segment registers DS, ES, and SS.
9315   xorw    %ax,%ax             # Set %ax to zero
9316   movw    %ax,%ds             # -> Data Segment
9317   movw    %ax,%es             # -> Extra Segment
9318   movw    %ax,%ss             # -> Stack Segment
9319 
9320   # Physical address line A20 is tied to zero so that the first PCs
9321   # with 2 MB would run software that assumed 1 MB.  Undo that.
9322 seta20.1:
9323   inb     $0x64,%al               # Wait for not busy
9324   testb   $0x2,%al
9325   jnz     seta20.1
9326 
9327   movb    $0xd1,%al               # 0xd1 -> port 0x64
9328   outb    %al,$0x64
9329 
9330 seta20.2:
9331   inb     $0x64,%al               # Wait for not busy
9332   testb   $0x2,%al
9333   jnz     seta20.2
9334 
9335   movb    $0xdf,%al               # 0xdf -> port 0x60
9336   outb    %al,$0x60
9337 
9338   # Switch from real to protected mode.  Use a bootstrap GDT that makes
9339   # virtual addresses map directly to physical addresses so that the
9340   # effective memory map doesn't change during the transition.
9341   lgdt    gdtdesc
9342   movl    %cr0, %eax
9343   orl     $CR0_PE, %eax
9344   movl    %eax, %cr0
9345 
9346 
9347 
9348 
9349 
9350   # Complete the transition to 32-bit protected mode by using a long jmp
9351   # to reload %cs and %eip.  The segment descriptors are set up with no
9352   # translation, so that the mapping is still the identity mapping.
9353   ljmp    $(SEG_KCODE<<3), $start32
9354 
9355 .code32  # Tell assembler to generate 32-bit code now.
9356 start32:
9357   # Set up the protected-mode data segment registers
9358   movw    $(SEG_KDATA<<3), %ax    # Our data segment selector
9359   movw    %ax, %ds                # -> DS: Data Segment
9360   movw    %ax, %es                # -> ES: Extra Segment
9361   movw    %ax, %ss                # -> SS: Stack Segment
9362   movw    $0, %ax                 # Zero segments not ready for use
9363   movw    %ax, %fs                # -> FS
9364   movw    %ax, %gs                # -> GS
9365 
9366   # Set up the stack pointer and call into C.
9367   movl    $start, %esp
9368   call    bootmain
9369 
9370   # If bootmain returns (it shouldn't), trigger a Bochs
9371   # breakpoint if running under Bochs, then loop.
9372   movw    $0x8a00, %ax            # 0x8a00 -> port 0x8a00
9373   movw    %ax, %dx
9374   outw    %ax, %dx
9375   movw    $0x8ae0, %ax            # 0x8ae0 -> port 0x8a00
9376   outw    %ax, %dx
9377 spin:
9378   jmp     spin
9379 
9380 # Bootstrap GDT
9381 .p2align 2                                # force 4 byte alignment
9382 gdt:
9383   SEG_NULLASM                             # null seg
9384   SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # code seg
9385   SEG_ASM(STA_W, 0x0, 0xffffffff)         # data seg
9386 
9387 gdtdesc:
9388   .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
9389   .long   gdt                             # address gdt
9390 
9391 
9392 
9393 
9394 
9395 
9396 
9397 
9398 
9399 
