9500 /* Simple linker script for the JOS kernel.
9501    See the GNU ld 'info' manual ("info ld") to learn the syntax. */
9502 
9503 OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
9504 OUTPUT_ARCH(i386)
9505 ENTRY(_start)
9506 
9507 SECTIONS
9508 {
9509 	/* Link the kernel at this address: "." means the current address */
9510         /* Must be equal to KERNLINK */
9511 	. = 0x80100000;
9512 
9513 	.text : AT(0x100000) {
9514 		*(.text .stub .text.* .gnu.linkonce.t.*)
9515 	}
9516 
9517 	PROVIDE(etext = .);	/* Define the 'etext' symbol to this value */
9518 
9519 	.rodata : {
9520 		*(.rodata .rodata.* .gnu.linkonce.r.*)
9521 	}
9522 
9523 	/* Include debugging information in kernel memory */
9524 	.stab : {
9525 		PROVIDE(__STAB_BEGIN__ = .);
9526 		*(.stab);
9527 		PROVIDE(__STAB_END__ = .);
9528 	}
9529 
9530 	.stabstr : {
9531 		PROVIDE(__STABSTR_BEGIN__ = .);
9532 		*(.stabstr);
9533 		PROVIDE(__STABSTR_END__ = .);
9534 	}
9535 
9536 	/* Adjust the address for the data segment to the next page */
9537 	. = ALIGN(0x1000);
9538 
9539 	/* Conventionally, Unix linkers provide pseudo-symbols
9540 	 * etext, edata, and end, at the end of the text, data, and bss.
9541 	 * For the kernel mapping, we need the address at the beginning
9542 	 * of the data section, but that's not one of the conventional
9543 	 * symbols, because the convention started before there was a
9544 	 * read-only rodata section between text and data. */
9545 	PROVIDE(data = .);
9546 
9547 
9548 
9549 
9550 	/* The data segment */
9551 	.data : {
9552 		*(.data)
9553 	}
9554 
9555 	PROVIDE(edata = .);
9556 
9557 	.bss : {
9558 		*(.bss)
9559 	}
9560 
9561 	PROVIDE(end = .);
9562 
9563 	/DISCARD/ : {
9564 		*(.eh_frame .note.GNU-stack)
9565 	}
9566 }
9567 
9568 
9569 
9570 
9571 
9572 
9573 
9574 
9575 
9576 
9577 
9578 
9579 
9580 
9581 
9582 
9583 
9584 
9585 
9586 
9587 
9588 
9589 
9590 
9591 
9592 
9593 
9594 
9595 
9596 
9597 
9598 
9599 
