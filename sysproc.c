#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.

int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


extern struct pt ptable;
int 
sys_getnice(void)
{
 int pid;
 if(argint(0,&pid) < 0)
	 return -1;
 struct proc* p;
 for(p = ptable.proc; p < &ptable.proc[NPROC];p++){
 	if(p->pid == pid)
	   return p->priority;		
 }
 return -1;
}

int
sys_setnice(void)
{
	int pid, new_priority;
	argint(0,&pid);
	argint(1,&new_priority);

	if(new_priority < 0 || new_priority > 40)
		return -1;
	struct proc* p;
	for(p = ptable.proc; p < &ptable.proc[NPROC];p++){
 		if(p->pid == pid)
	    p->priority = new_priority;
		return 1;		
 	}
	return -1;
}


int
sys_ps(void)
{
	int a;
	a = 1;
	return a;
}


