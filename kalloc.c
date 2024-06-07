// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

// Project 4
// array for counting the numbers of references for each physical page
uint pgrefcnt[PHYSTOP / PGSIZE]; // PHYSTOP is the top of the physical memory that xv6 OS uses
uint freepagecnt; // Count the number of total free pages in the system

struct spinlock pgrefcnt_lock; // define a spinlock for pgrefcnt array

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  // Project 4
  initlock(&pgrefcnt_lock, "refcnt"); // initialize for page reference counts
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  // Project 4
  // memory allocated during kinit1 includes areas such as text, data, and BSS segments
  freepagecnt = ((char *)vend - (char *)vstart) / PGSIZE; // initialize the number of free physical pages
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  uint pa; // physical address
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    pa = V2P(p);
    pgrefcnt[pa / PGSIZE] = 0; // initialize page ref count to 0
    kfree(p);
}
//PAGEBREAK: 21
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

  uint pa = V2P(v); // physical address of v

  if(get_refc(pa) >= 1) { // first, just decrement the number of ref count
    decr_refc(pa);
    if(get_refc(pa) == 0) { // free the page if ref count is 0
      r = (struct run*)v;
      r->next = kmem.freelist;
      kmem.freelist = r;
      freepagecnt++;
    }
  }
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

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

// Project4

void incr_refc(uint pa)
{
  acquire(&pgrefcnt_lock); 
  pgrefcnt[pa / PGSIZE]++;
  release(&pgrefcnt_lock); 
}

void decr_refc(uint pa)
{
  acquire(&pgrefcnt_lock); 
  pgrefcnt[pa / PGSIZE]--;
  release(&pgrefcnt_lock); 
}

int get_refc(uint pa)
{
  acquire(&pgrefcnt_lock); 
  int refcnt = (int)pgrefcnt[pa / PGSIZE];
  release(&pgrefcnt_lock); 
  return refcnt;
}
