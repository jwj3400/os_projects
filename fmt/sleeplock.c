4700 // Sleeping locks
4701 
4702 #include "types.h"
4703 #include "defs.h"
4704 #include "param.h"
4705 #include "x86.h"
4706 #include "memlayout.h"
4707 #include "mmu.h"
4708 #include "proc.h"
4709 #include "spinlock.h"
4710 #include "sleeplock.h"
4711 
4712 void
4713 initsleeplock(struct sleeplock *lk, char *name)
4714 {
4715   initlock(&lk->lk, "sleep lock");
4716   lk->name = name;
4717   lk->locked = 0;
4718   lk->pid = 0;
4719 }
4720 
4721 void
4722 acquiresleep(struct sleeplock *lk)
4723 {
4724   acquire(&lk->lk);
4725   while (lk->locked) {
4726     sleep(lk, &lk->lk);
4727   }
4728   lk->locked = 1;
4729   lk->pid = myproc()->pid;
4730   release(&lk->lk);
4731 }
4732 
4733 void
4734 releasesleep(struct sleeplock *lk)
4735 {
4736   acquire(&lk->lk);
4737   lk->locked = 0;
4738   lk->pid = 0;
4739   wakeup(lk);
4740   release(&lk->lk);
4741 }
4742 
4743 
4744 
4745 
4746 
4747 
4748 
4749 
4750 int
4751 holdingsleep(struct sleeplock *lk)
4752 {
4753   int r;
4754 
4755   acquire(&lk->lk);
4756   r = lk->locked && (lk->pid == myproc()->pid);
4757   release(&lk->lk);
4758   return r;
4759 }
4760 
4761 
4762 
4763 
4764 
4765 
4766 
4767 
4768 
4769 
4770 
4771 
4772 
4773 
4774 
4775 
4776 
4777 
4778 
4779 
4780 
4781 
4782 
4783 
4784 
4785 
4786 
4787 
4788 
4789 
4790 
4791 
4792 
4793 
4794 
4795 
4796 
4797 
4798 
4799 
