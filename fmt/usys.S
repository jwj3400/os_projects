8650 #include "syscall.h"
8651 #include "traps.h"
8652 
8653 #define SYSCALL(name) \
8654   .globl name; \
8655   name: \
8656     movl $SYS_ ## name, %eax; \
8657     int $T_SYSCALL; \
8658     ret
8659 
8660 SYSCALL(fork)
8661 SYSCALL(exit)
8662 SYSCALL(wait)
8663 SYSCALL(pipe)
8664 SYSCALL(read)
8665 SYSCALL(write)
8666 SYSCALL(close)
8667 SYSCALL(kill)
8668 SYSCALL(exec)
8669 SYSCALL(open)
8670 SYSCALL(mknod)
8671 SYSCALL(unlink)
8672 SYSCALL(fstat)
8673 SYSCALL(link)
8674 SYSCALL(mkdir)
8675 SYSCALL(chdir)
8676 SYSCALL(dup)
8677 SYSCALL(getpid)
8678 SYSCALL(sbrk)
8679 SYSCALL(sleep)
8680 SYSCALL(uptime)
8681 SYSCALL(swapread)
8682 SYSCALL(swapwrite)
8683 SYSCALL(getnice)
8684 SYSCALL(setnice)
8685 SYSCALL(ps)
8686 
8687 
8688 
8689 
8690 
8691 
8692 
8693 
8694 
8695 
8696 
8697 
8698 
8699 
