8600 # Initial process execs /init.
8601 # This code runs in user space.
8602 
8603 #include "syscall.h"
8604 #include "traps.h"
8605 
8606 
8607 # exec(init, argv)
8608 .globl start
8609 start:
8610   pushl $argv
8611   pushl $init
8612   pushl $0  // where caller pc would be
8613   movl $SYS_exec, %eax
8614   int $T_SYSCALL
8615 
8616 # for(;;) exit();
8617 exit:
8618   movl $SYS_exit, %eax
8619   int $T_SYSCALL
8620   jmp exit
8621 
8622 # char init[] = "/init\0";
8623 init:
8624   .string "/init\0"
8625 
8626 # char *argv[] = { init, 0 };
8627 .p2align 2
8628 argv:
8629   .long init
8630   .long 0
8631 
8632 
8633 
8634 
8635 
8636 
8637 
8638 
8639 
8640 
8641 
8642 
8643 
8644 
8645 
8646 
8647 
8648 
8649 
