#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "spinlock.h"

//////////////////////
struct {
    struct spinlock lock;
    struct mmap_area mmap_area[NMMAP];
} mtable;
       
struct file{
	enum {FD_NONE, FD_PIPE, FD_INODE } type;
	int ref;
	char readable;
	char writable;
	struct pipe* pipe;
	struct inode* ip;
	uint off;
};
////////////////////////

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.










/////////////////////////////////////////////
/////////////////////////////////////////////
// implement mmap
// kalloc() 
uint mmap(uint addr, int length, int prot, int flags, int fd, int offset) {
    struct mmap_area* m;
    struct proc* curproc = myproc();

    uint newAddr = MMAPBASE + PGROUNDDOWN(addr);		//mmapbase = 0x4000000
    length = PGROUNDUP(length);
    acquire(&mtable.lock);
    //fixed address 
    if (flags & MAP_FIXED) {
        for (m = mtable.mmap_area; m < &mtable.mmap_area[NMMAP]; m++) {
            if (m->p == curproc && ((m->addr <= newAddr && newAddr < m->addr + m->length) || (newAddr <= m->addr && m->addr < newAddr + length))) {
                cprintf("already allocated memory addr");
                release(&mtable.lock);
                return 0;
            }
        }

    } 
	// if addr is not fixed, find addr that can be mmapped
	else {
        newAddr = MMAPBASE;
        m = mtable.mmap_area;
        for (; m < &mtable.mmap_area[NMMAP]; m++) {
            if (m->p == curproc && ((m->addr <= newAddr && newAddr < m->addr + m->length) || (newAddr <= m->addr && m->addr < newAddr + length))) {
                newAddr = m->addr + m->length;
                m = mtable.mmap_area;
            }
        }
        if (newAddr >= KERNBASE) {
            cprintf("cannot use memory over KERNBASE");
            release(&mtable.lock);
            return 0;
        }
    }
    release(&mtable.lock);

    //check whether file can be read
    struct file* f;
    if (fd != -1) {
		f = curproc->ofile[fd];
		/*
        if (f->readable == 0) {
            cprintf("err: cannot access non-readable file");
            return 0;
        }*/
        filedup(f);
    }
    //if map_populate make page table entry
    char* mem;
    if (flags & MAP_POPULATE) {
		//mmap with file 
        if (fd != -1) {		
				if (flags & MAP_ANONYMOUS) {
                cprintf("err: to use fd, MAP_ANONYMOS must be 0");
                fileclose(f);
                return 0;
            }
            // alloc mem and put file items to it
            int newoff = offset;
            for (uint a = 0; a < PGROUNDUP(length); a += PGSIZE) {
				mem = kalloc();
                if (mem == 0) {
                    cprintf("err: all memory is in used");
                    fileclose(f);
                    return 0;
                }

				if (mappages(curproc->pgdir, (char*)(newAddr + a), PGSIZE, V2P(mem), PTE_U | prot) < 0) {
                    cprintf("err: cannot alloc page table, all memory is in used");
                    deallocuvm(curproc->pgdir, newAddr + a, newAddr);
                    fileclose(f);
                    return 0;
                }
                memset(mem, 0, PGSIZE);

                int readNum = 0;
                int savedOffset = f->off;
                f->off = newoff;
                if ((readNum = fileread(f, mem, PGSIZE)) < 0) {
                    cprintf("err: fail to read file readNum = %d\n",readNum);
                    deallocuvm(curproc->pgdir, newAddr + a, newAddr);
                    f->off = savedOffset;
                    fileclose(f);
                    return 0;
                }
                f->off = savedOffset;
                newoff += readNum;
            }
        } else { 		//mmap without file
            if (!(flags & MAP_ANONYMOUS)) {
                cprintf("err: not to use fd, MAP_ANONYMOS must be 1\n");
                return 0;
            }
			//kallc for populate and anonymous
            for (uint a = 0; a < PGROUNDUP(length); a += PGSIZE) {
                mem = kalloc();
                if (mem == 0) {
                    cprintf("err: all memory is in used");
                    return 0;
                }
                memset(mem, 0, PGSIZE);
    
				if (mappages(curproc->pgdir, (char*)(newAddr + a), PGSIZE, V2P(mem), PTE_U | prot) < 0) {
                    cprintf("err: cannot alloc page table, all memory is in used");
                    deallocuvm(curproc->pgdir, newAddr + a, newAddr);
                    return 0;
                }
            }
        }
    } 
	acquire(&mtable.lock);
    for (m = mtable.mmap_area; m < &mtable.mmap_area[NMMAP]; m++) {
        if (m->addr == 0)
            break;
    }
    if (fd != -1) {
        m->f = f;
    }
    m->addr = newAddr;
    m->length = length;
    m->offset = offset;
    m->prot = prot;
    m->flags = flags;
    m->p = curproc;
	cprintf("inserted mmap_area ==== m->addr: %p, m->length: %d, m->offset: %d, m->prot: %d, m->flags: %d, m->p: %p\n",m->addr, m->length, m->offset, m->prot, m->flags, m->p);
    release(&mtable.lock);
    return newAddr;
}

// 1. write back
// 2. page table  page table entry ?ʱ?ȭ ????
// 3. clean mtable sizeof mmap_area  
int munmap(uint addr) {
    struct proc* curproc = myproc();
    uint newAddr = PGROUNDDOWN(addr);
    struct mmap_area* m;
    acquire(&mtable.lock);
    for (m = mtable.mmap_area; m < &mtable.mmap_area[NMMAP]; m++) {
        if (m->addr != newAddr || m->p != curproc)
            continue;
        if (!(m->flags & MAP_ANONYMOUS) && (m->flags & MAP_SHARED)) {
			cprintf("=== write back ===\n");
            if (m->f->writable == 1 && (m->prot & PROT_WRITE)) {		//writeback
                int savedOffset = m->f->off;
                m->f->off = m->offset;

                pte_t* pte = walkpgdir(curproc->pgdir, (char*)m->addr, 0);				//writeback from physical mem to file
                uint writeAddr = PTE_ADDR(*pte);
                int writeNum;
				cprintf("check before write\n");
				ilock(m->f->ip);
                for (; writeAddr < m->addr + m->length; writeAddr += PGSIZE) {
					cprintf("write check\n");
                    writeNum = writei(m->f->ip, (char*)writeAddr, m->offset, PGSIZE);
                    m->f->off += writeNum;
                }
				iunlock(m->f->ip);
				cprintf("check after write\n");
                m->f->off = savedOffset;
            }
            fileclose(m->f);
        }
        pde_t* pde = &curproc->pgdir[PDX(addr)];					//clear page table, dealloc pages
        if (*pde & PTE_P) {
            deallocuvm(curproc->pgdir, m->addr + m->length, m->addr);
        }

        memset(m, 0, sizeof(struct mmap_area));					//clear mmap_area
        release(&mtable.lock);
        return 1;
    }
    release(&mtable.lock);
    cprintf("err: no matching addr is in used");
    return -1;
}



// pagefault handler
// err == 0 : read fault
// err == 1 : write fault
int handle_pgfault(char* addr, uint err) {
    struct mmap_area* m;
    struct proc* curproc = myproc();
	uint newAddr = (uint)addr;
	cprintf("pgfault addr %p\n",addr);
    newAddr = PGROUNDDOWN(newAddr);
	char* mem; 
    acquire(&mtable.lock);
	cprintf("pgfault newAddr %p\n", newAddr);
	
    for (m = mtable.mmap_area; m < &mtable.mmap_area[NMMAP]; m++) {
		//cprintf("m->addr %p, newAddr %p, m->addr + m->length %p\n",m->addr, newAddr, m->addr + m->length);
        if (curproc == m->p && (m->addr <= newAddr && newAddr < m->addr + m->length)) {
			cprintf("m->addr %p, newAddr %p, m->addr + m->length %p\n",m->addr, newAddr, m->addr + m->length);
            if (err) {
				cprintf("write err\n");
                if (!(m->prot & PROT_WRITE)) {
                    cprintf("err : impossible to write on non-wirtable memory");
                    release(&mtable.lock);
                    return -1;
                }
            }
            mem = kalloc();
            if (mem == 0) {
                cprintf("err: all memory is in used");
                release(&mtable.lock);
                return -1;
            }
			cprintf("mem alloc success\n");
            memset(mem, 0, PGSIZE);
			cprintf("memset success\n");
            //ananoymous == 0 -> fileread
            if (!(m->flags & MAP_ANONYMOUS)) {
                if (readi(m->f->ip, mem, m->offset + newAddr - m->addr, PGSIZE) < 0) {
                    cprintf("err: fail to read file\n");
                    kfree(mem);
                    release(&mtable.lock);
                    return -1;
                }
            }
			cprintf("before mapages\n");
		//	pde_t* pde = &curproc->pgdir[PDX(addr)];	
		//	cprintf("pgdir %p addr %p memAddr %p *pde %x *pde & PTE_P %d \n",curproc->pgdir, addr, mem, *pde, *pde & PTE_P);
            if (mappages(curproc->pgdir, addr, PGSIZE, V2P(mem), PTE_U | m->prot) < 0) {
                cprintf("err: cannot alloc page table, all memory is in used");
                kfree(mem);
                release(&mtable.lock);
                return -1;
            }
			cprintf("success pgfault\n");
            release(&mtable.lock);
            return 1;
        }

    }
    release(&mtable.lock);
    cprintf("err : no matching mmaped area found, plz mmap first to use the address\n");
    return -1;
}
