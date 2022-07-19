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

////////////////////////////////////
struct page pages[PHYSTOP/PGSIZE];
struct page *page_lru_head;
int num_free_pages;
int num_lru_pages;
char* bitmap;
struct spinlock swaplock;
////////////////////////////////////

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
  memset(bitmap, 0, sizeof(char) * PGSIZE);
  page_lru_head = (struct page*)kalloc();
  num_free_pages = (vend - vstart) / PGSIZE;
  num_lru_pages = 0;
  memset(page_lru_head, 0, sizeof(struct page));
  initlock(&swaplock, "swap");
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



//struct page newPG to insert in lru list
//tail insertion
void insert_lru(struct page* newPG){
	cprintf("insert_lru newPG addr %p newPG->pgdir: %p, newPG->vaddr %p\n", newPG, newPG->pgdir, newPG->vaddr);
	//	cprintf("head->next %p head->prev %p, head->pgdir %p, head->vaddr %p\n",page_lru_head->next, page_lru_head->prev, page_lru_head->pgdir, page_lru_head->vaddr);
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

//			cprintf("circulate lru list\n");
//			struct page* cur = page_lru_head->next;
/*		
		   while(cur != page_lru_head){
		   //cprintf("head->next %p head->prev %p, head->pgdir %p, head->vaddr %p\n",page_lru_head->next, page_lru_head->prev, page_lru_head->pgdir, page_lru_head->vaddr);
		   cprintf("head %p, cur %p, cur->next %p, cur->prev %p, cur->pgdir %p, cur->vaddr %p\n",page_lru_head, cur, cur->next, cur->prev, cur->pgdir, cur->vaddr);
		   cur = cur->next;
		   }
*/
		//check bitmap
		/* 
		for(int i = 0; i < 4096; i++)
		{
		cprintf("%d : %x \n",i, bitmap[i]);
		}
		cprintf("\n");
		*/
	num_free_pages--;
	num_lru_pages++;	
	return ;
}

// evict from lru list
//
int delete_lru(pde_t* pgdir, uint va){
	struct page* cur = page_lru_head;
	struct page* prev;
	//if only 1 left in lru list
	if(cur->next == cur){
		memset(cur, 0, sizeof(struct page));
	} 
	else{
		cur = cur->next;
		cprintf("del pgdir %p, va %p\n", pgdir, va);
		for(;;){
			cprintf("cur pgdir %p, vaddr %p\n", cur->pgdir, cur->vaddr);
			if(cur->pgdir == pgdir && (uint)cur->vaddr == va){
				prev = cur->prev;
				break;
			}
			if(cur == page_lru_head){
				return -1;
			}
			cur = cur->next;
		}
		if(cur == page_lru_head)
			page_lru_head = cur->next;
		prev->next = cur->next;
		cur->next->prev = prev;
		memset(cur, 0, sizeof(struct page));
	}
	num_free_pages++;
	num_lru_pages--;
	return 1;
}



//evict page with clock algorithm
int reclaim(){
	cprintf("================ evict page with clock algorithm %d\n", num_lru_pages);
	struct page* cur = page_lru_head;
	while(1){
		if(!check_PTE_A(cur)){
			cprintf("reclaim start\n");


			struct page* cur1 = page_lru_head->next;
		
		   while(cur1 != page_lru_head){
		   //cprintf("head->next %p head->prev %p, head->pgdir %p, head->vaddr %p\n",page_lru_head->next, page_lru_head->prev, page_lru_head->pgdir, page_lru_head->vaddr);
		   cprintf("head %p, cur %p, cur->next %p, cur->prev %p, cur->pgdir %p, cur->vaddr %p\n",page_lru_head, cur1, cur1->next, cur1->prev, cur1->pgdir, cur1->vaddr);
		   cur1 = cur1->next;
		   }


			//find offset of swap sapce & set bitmap
			uint offset = SWAPMAX-SWAPBASE + 1; 	
			//find bitmap offset which is 0
			for(int i = 1; i < (SWAPMAX - SWAPBASE + 1) / 8; i++){
				if(!(bitmap[i / 8] & (1 << (i % 8)))){
					offset = i;
					bitmap[i / 8] |= (1 << (i % 8));
					break;
				}
			}
			cprintf("check point 1\n");
			if(offset == SWAPMAX-SWAPBASE + 1){
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
			
			cprintf("phyAddr: %p offset: %d\n", P2V(phyAddr), offset);
			
			release(&kmem.lock);
			swapwrite(P2V(phyAddr),offset);
			//acquire(&kmem.lock);
			
			cprintf("check point 3\n");
			cprintf("swap out pte: %p\n", *pte);

			//modify pte to have offset number & clear PTE_P
			*pte &= ~(~offset << 12);
			*pte &= ~PTE_P;
			
			//free the page
			kfree(P2V(phyAddr));
			
			//evict from lru list
			if(cur == page_lru_head)
				page_lru_head = cur->next;
			cur->prev->next = cur->next;
			cur->next->prev = cur->prev;
			memset(cur, 0, sizeof(struct page));
			break;
		}
		cur = cur->next;
	}
	cprintf("=============reclaim end\n");
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

void clearbitmap(uint offset){
	bitmap[offset / 8] &= ~(1 << (offset % 8)); 
	return;
}
