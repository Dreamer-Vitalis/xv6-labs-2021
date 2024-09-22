// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

/*
Task1 -- Memory allocator
本实验完成的任务是为每个CPU都维护一个空闲列表。
【初始时】将所有的空闲内存【全部分配】到某个CPU，此后各个CPU需要内存时，如果当前CPU的空闲列表上没有，则窃取其他CPU的。

例如，所有的空闲内存初始分配到CPU0，当CPU1需要内存时就会窃取CPU0的，
而使用完成后就挂在CPU1的空闲列表，此后CPU1再次需要内存时就可以从自己的空闲列表中取。
而被窃取的这块内存，就不能再被CPU0使用了。
*/

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
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

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off(); // 关闭中断
  int cpu_id = cpuid();
  pop_off(); // 打开中断

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off(); // 关闭中断
  int cpu_id = cpuid();
  pop_off(); // 打开中断

  acquire(&kmem[cpu_id].lock);

  r = kmem[cpu_id].freelist;
  if(r)
    kmem[cpu_id].freelist = r->next;
  else // 在其他CPU核心中，steal 一块内存 
  {
    for(int i = 0; i < NCPU; i++)
    {
      if(cpu_id == i) continue;

      acquire(&kmem[i].lock);

      struct run *rr = kmem[i].freelist;
      if(rr) // 找到了可用的内存
      {
        r = rr;
        kmem[i].freelist = rr->next; // 该核心将会不能再使用该块内存了，所以需要 next
        r->next = 0; //? 有的博主不加这行也过了
      }

      release(&kmem[i].lock);

      if(r) break;
    }
  }

  release(&kmem[cpu_id].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk

  return (void*)r;
}
