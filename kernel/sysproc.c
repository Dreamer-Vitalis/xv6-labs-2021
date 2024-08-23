#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

// 声明引用了vm.c中的walk
pte_t * walk(pagetable_t pagetable, uint64 va, int alloc);

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
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

uint64
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

/*
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) { // 合法的
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else { // 非法的
      if(!alloc || (pagetable = (pde_t*)kalloc()) == 0) // 如果不需要分配物理内存，则报错，返回0； 或者如果需要分配物理内存，但分配空间失败，也报错，返回0
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V | PTE_A;
    }
  }
  return &pagetable[PX(0, va)];
}
*/

#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  struct proc *p = myproc();
  //vmprint(p->pagetable);

  uint64 base;
  int len;
  uint64 mask;
  if(argaddr(0, &base) < 0 || argint(1, &len) || argaddr(2, &mask) < 0)
    return -1;
  
  if(base >= MAXVA)
    return -1;

  uint64 ret = 0;
  
  for(uint64 i = 0; i < len; i++)
  {
    pte_t *pte = walk(p->pagetable, base + i * PGSIZE, 0);
    if(pte && (*pte & PTE_V) && (*pte & PTE_A)) {
      ret |= (1 << i);
      // 清除访问位
      *pte &= ~PTE_A;
    }
  }

  /*
  for(uint64 i = 0; i < len; i++)
    if(ret & ((uint64)1 << i))
      printf("1 ");
    else
      printf("0 ");
  printf("\n");
  */
  
  if(copyout(p->pagetable, mask, (char *)&ret, sizeof(ret)))
    return -1;
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}