// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"


//read value = (bitmap[b/8] & (1 << (b%8)) != 0;
//set bitmap[b/8] |= (1 << (b%8));
//clear bitmap[b/8] &= ~(1 << (b%8));
void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

struct page pages[PHYSTOP/PGSIZE];
struct page *page_lru_head;
int num_free_pages;
int num_lru_pages;
char* bitmap;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit3(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
  
  bitmap = kalloc();
  memset(bitmap,0,sizeof(char) * PGSIZE);
  page_lru_head = (struct page*)kalloc();
  memset(page_lru_head, 0, sizeof(struct page));
  cprintf("%p", bitmap);
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;
  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

try_again:
  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(!r && reclaim())
	  goto try_again;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}



//newPg to insert in lru list
//tail insertion
void insert_lru(struct page* newPG){
	cprintf("insert_lru newPG addr %p\n", newPG);
	cprintf("head->next %p head->prev %p, head->pgdir %p, head->vaddr %p\n",page_lru_head->next, page_lru_head->prev, page_lru_head->pgdir, page_lru_head->vaddr);
	if(page_lru_head->next == 0){
		cprintf("first head\n");
		page_lru_head = newPG;
		newPG->next = newPG;
		newPG->prev = newPG;
		return ;
	}

	struct page* curhead = page_lru_head;
	newPG->next = curhead;
	newPG->prev = curhead->prev;
	curhead->prev = newPG;
	newPG->prev->next = newPG;

	cprintf("circulate lru list\n");
	struct page* cur = page_lru_head->next;
/*
	while(cur != page_lru_head){
		cprintf("head->next %p head->prev %p, head->pgdir %p, head->vaddr %p\n",page_lru_head->next, page_lru_head->prev, page_lru_head->pgdir, page_lru_head->vaddr);
		cprintf("head %p, cur %p, cur->next %p, cur->prev %p, cur->pgdir %p, cur->vaddr %p\n",page_lru_head, cur, cur->next, cur->prev, cur->pgdir, cur->vaddr);
		cur = cur->next;
	}
*/
	return ;
}


//evict page with clock algorithm
int reclaim(){
	struct page* cur = page_lru_head;
	while(1){
		if(!check_PTE_A(cur)){
			//find offset of swap sapce & set bitmap
			int offset = SWAPMAX-SWAPBASE; 	//bitmap offset
			for(int i = 0; i < (SWAPMAX - SWAPBASE) / 8; i++){
				if(!(bitmap[i / 8] & (1 << (i % 8)))){
					offset = i;
					bitmap[i / 8] |= (1 << (i % 8));
					break;
				}
			}
			if(offset == SWAPMAX-SWAPBASE){
				cprintf("err: out of memory\n");
			}
			//swap out	
			pde_t* pde;
			pte_t* pgtab;

			pde = &cur->pgdir[PDX(cur->vaddr)];
			pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
	
			pte_t* pte;
			pte = &pgtab[PTX(cur->vaddr)];

			uint phyAddr2 = (*pte) & 0xfffff000;
			char* phyAddr = (char*)phyAddr2;  
			swapwrite(phyAddr,offset);
			cprintf("swap out pte: %p\n", *pte);

			//modify pte to have offset number & clear PTE_P
			*pte &= ~(~offset << 12);
			*pte &= ~PTE_P;
			
			//free the page
			kfree(phyAddr);
			
			//evict from lru list
			cur->prev->next = cur->next;
			cur->next->prev = cur->prev;
			memset(cur, 0, sizeof(struct page));
			break;
		}
		cur = cur->next;
	}
	return 1;
}

// return 1 if PTE_A is valid
// if pte_a is 1 change it to 0
int check_PTE_A(struct page* curPG){
	pde_t* pde;
	pte_t* pgtab;

	pde = &curPG->pgdir[PDX(curPG->vaddr)];
	pgtab = (pte_t*)P2V(PTE_ADDR(*pde));

	pte_t* pte;
	pte = &pgtab[PTX(curPG->vaddr)];
	
	if(*pte & PTE_A){
		*pte &= ~PTE_A; 
		return 1;
	}
	else
		return 0;
}


