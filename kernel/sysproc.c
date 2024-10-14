#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "fcntl.h"

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

// Add from file.h
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};


// Add from sysfile.c
int
argfd2(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}


uint64
sys_mmap(void)
{
  uint64 addr; // VA
  int length;
  int prot;
  int flags;
  int fd;
  int offset;
  struct file* vfile;
  
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0 || argint(2, &prot) < 0
   || argint(3, &flags) < 0 || argfd2(4, &fd, &vfile) < 0 || argint(5, &offset) < 0)
    return -1;
  
  // 根据hints, 这两个值在本实验中均为0
  if(addr != 0 || offset != 0)
    return -1;
  
  // 参数检查
  if((prot & (PROT_READ | PROT_WRITE)) == 0)
    return -1;
  
  // 参数检查
  if(flags != MAP_SHARED && flags != MAP_PRIVATE)
    return -1;

  // 权限冲突：文件不可写 但是 要求的VMA是可写且要写回磁盘上的，则冲突
  if((!vfile->writable) && (prot & PROT_WRITE) != 0 && flags == MAP_SHARED)
    return -1;
  
  struct proc *p = myproc();
  if(p->sz + length >= MAXVA) // 不能超过MAXVA
    return -1;
  
  for(int i = 0; i < 16; i++)
  {
    // 成功找到可用的VMA
    if(p->vma[i].addr == 0)
    {
      if(addr == 0)
        p->vma[i].addr = p->sz;
      else
        p->vma[i].addr = addr;
      p->vma[i].length = length;
      p->vma[i].prot = prot;
      p->vma[i].flags = flags;
      p->vma[i].file = vfile;
      p->vma[i].offset = offset;
      p->vma[i].fd = fd;

      p->sz += length;
      filedup(vfile);
      return (uint64)p->vma[i].addr;
    }
  }

  // 没有找到可用的VMA
  return -1;
}

uint64
sys_munmap(void)
{
  uint64 addr; // VA
  int length;
  if(argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;
  
  char flag = 0;
  struct proc *p = myproc();
  struct VMA* v = 0;
  for(int i = 0; i < 16; i++)
  {
    if(p->vma[i].addr && p->vma[i].addr <= addr && (uint64)addr + length <= (uint64)p->vma[i].addr + p->vma[i].length)
    {
      flag = 1;
      v = &p->vma[i];
      break;
    }
  }
  
  if(!flag)
    return -1;
  
  // 因为只有从起始位置开始 或者 从末尾开始，如果是从起始位置0开始取消映射，而只需要+length就代表 0 ~ length-1 都没有被映射了
  v->addr = v->addr + length;
  // 映射的长度 -= length
  v->length -= length;

  // 将MAP_SHARED页面写回文件系统
  if(v->flags == MAP_SHARED && (v->prot & PROT_WRITE))
    filewrite(v->file, (uint64)addr, length);

  // 判断此页面是否存在映射
  uvmunmap(p->pagetable, (uint64)addr, length / PGSIZE, 1);

  return 0;
}