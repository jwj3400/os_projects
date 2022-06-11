9400 // Boot loader.
9401 //
9402 // Part of the boot block, along with bootasm.S, which calls bootmain().
9403 // bootasm.S has put the processor into protected 32-bit mode.
9404 // bootmain() loads an ELF kernel image from the disk starting at
9405 // sector 1 and then jumps to the kernel entry routine.
9406 
9407 #include "types.h"
9408 #include "elf.h"
9409 #include "x86.h"
9410 #include "memlayout.h"
9411 
9412 #define SECTSIZE  512
9413 
9414 void readseg(uchar*, uint, uint);
9415 
9416 void
9417 bootmain(void)
9418 {
9419   struct elfhdr *elf;
9420   struct proghdr *ph, *eph;
9421   void (*entry)(void);
9422   uchar* pa;
9423 
9424   elf = (struct elfhdr*)0x10000;  // scratch space
9425 
9426   // Read 1st page off disk
9427   readseg((uchar*)elf, 4096, 0);
9428 
9429   // Is this an ELF executable?
9430   if(elf->magic != ELF_MAGIC)
9431     return;  // let bootasm.S handle error
9432 
9433   // Load each program segment (ignores ph flags).
9434   ph = (struct proghdr*)((uchar*)elf + elf->phoff);
9435   eph = ph + elf->phnum;
9436   for(; ph < eph; ph++){
9437     pa = (uchar*)ph->paddr;
9438     readseg(pa, ph->filesz, ph->off);
9439     if(ph->memsz > ph->filesz)
9440       stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
9441   }
9442 
9443   // Call the entry point from the ELF header.
9444   // Does not return!
9445   entry = (void(*)(void))(elf->entry);
9446   entry();
9447 }
9448 
9449 
9450 void
9451 waitdisk(void)
9452 {
9453   // Wait for disk ready.
9454   while((inb(0x1F7) & 0xC0) != 0x40)
9455     ;
9456 }
9457 
9458 // Read a single sector at offset into dst.
9459 void
9460 readsect(void *dst, uint offset)
9461 {
9462   // Issue command.
9463   waitdisk();
9464   outb(0x1F2, 1);   // count = 1
9465   outb(0x1F3, offset);
9466   outb(0x1F4, offset >> 8);
9467   outb(0x1F5, offset >> 16);
9468   outb(0x1F6, (offset >> 24) | 0xE0);
9469   outb(0x1F7, 0x20);  // cmd 0x20 - read sectors
9470 
9471   // Read data.
9472   waitdisk();
9473   insl(0x1F0, dst, SECTSIZE/4);
9474 }
9475 
9476 // Read 'count' bytes at 'offset' from kernel into physical address 'pa'.
9477 // Might copy more than asked.
9478 void
9479 readseg(uchar* pa, uint count, uint offset)
9480 {
9481   uchar* epa;
9482 
9483   epa = pa + count;
9484 
9485   // Round down to sector boundary.
9486   pa -= offset % SECTSIZE;
9487 
9488   // Translate from bytes to sectors; kernel starts at sector 1.
9489   offset = (offset / SECTSIZE) + 1;
9490 
9491   // If this is too slow, we could read lots of sectors at a time.
9492   // We'd write more to memory than asked, but it doesn't matter --
9493   // we load in increasing order.
9494   for(; pa < epa; pa += SECTSIZE, offset++)
9495     readsect(pa, offset);
9496 }
9497 
9498 
9499 
