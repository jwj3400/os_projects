8500 #include "types.h"
8501 #include "x86.h"
8502 #include "traps.h"
8503 
8504 // I/O Addresses of the two programmable interrupt controllers
8505 #define IO_PIC1         0x20    // Master (IRQs 0-7)
8506 #define IO_PIC2         0xA0    // Slave (IRQs 8-15)
8507 
8508 // Don't use the 8259A interrupt controllers.  Xv6 assumes SMP hardware.
8509 void
8510 picinit(void)
8511 {
8512   // mask all interrupts
8513   outb(IO_PIC1+1, 0xFF);
8514   outb(IO_PIC2+1, 0xFF);
8515 }
8516 
8517 
8518 
8519 
8520 
8521 
8522 
8523 
8524 
8525 
8526 
8527 
8528 
8529 
8530 
8531 
8532 
8533 
8534 
8535 
8536 
8537 
8538 
8539 
8540 
8541 
8542 
8543 
8544 
8545 
8546 
8547 
8548 
8549 
8550 // Blank page.
8551 
8552 
8553 
8554 
8555 
8556 
8557 
8558 
8559 
8560 
8561 
8562 
8563 
8564 
8565 
8566 
8567 
8568 
8569 
8570 
8571 
8572 
8573 
8574 
8575 
8576 
8577 
8578 
8579 
8580 
8581 
8582 
8583 
8584 
8585 
8586 
8587 
8588 
8589 
8590 
8591 
8592 
8593 
8594 
8595 
8596 
8597 
8598 
8599 
