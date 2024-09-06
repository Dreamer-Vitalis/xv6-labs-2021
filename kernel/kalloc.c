// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define IX(pa) ((uint64)pa-KERNBASE)/PGSIZE // Index for reference count

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

int ref[(PHYSTOP - KERNBASE) / PGSIZE]; // reference count (Add for Lab COW) 

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

int ref_add(uint64 pa, int n)
{
  acquire(&kmem.lock);
  if(pa >= PHYSTOP || ref[IX(pa)] < 0)
    panic("ref_add() : ref Wrong1");
  ref[IX(pa)] += n;
  if(pa >= PHYSTOP || ref[IX(pa)] < 0)
    panic("ref_add() : ref Wrong2");
  release(&kmem.lock);
  return 0;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    acquire(&kmem.lock);
    ref[IX(p)] = 1;
    release(&kmem.lock);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if(ref[IX(pa)] < 1)
    panic("kfree() : Wrong");
  int flags = --ref[IX(pa)];
  release(&kmem.lock);

  if(flags > 0)
    return;
  
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r){
    kmem.freelist = r->next;
    if(ref[IX(r)] != 0)
      panic("kalloc() : Wrong!");
    ref[IX(r)] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
