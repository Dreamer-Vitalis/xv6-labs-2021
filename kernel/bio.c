// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct
{
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[13]; // Modify

  struct spinlock global_lock; // Just for bget() when Not cached and can't find a LRU buffer.
  struct spinlock lock[13]; // Modify
  
  struct buf buf[NBUF];
} bcache;

void binit(void)
{
  struct buf *b;
  initlock(&bcache.global_lock, "bcache");
  for (int i = 0; i < 13; i++)
  {
    char lockname[10];
    snprintf(lockname, sizeof(lockname), "bcache_%d", i);
    initlock(&bcache.lock[i], lockname);
  }

  // 这里让每个哈希桶都有相等数量的buffer cache，后续在bget()中可能会变动
  // 头插法，把所有 buf 插入到链表当中
  // Create linked list of buffers
  for (int i = 0; i < 13; i++)
  {
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    for (b = bcache.buf + i; b < bcache.buf + NBUF; b += 13)
    {
      b->next = bcache.head[i].next;
      b->prev = &bcache.head[i];
      initsleeplock(&b->lock, "buffer");
      bcache.head[i].next->prev = b;
      bcache.head[i].next = b;
    }
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint bno = blockno % 13;
  acquire(&bcache.lock[bno]);

  // Is the block already cached?
  for (b = bcache.head[bno].next; b != &bcache.head[bno]; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.lock[bno]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.head[bno].prev; b != &bcache.head[bno]; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[bno]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 不应该在这里释放bcache.lock[bno]，因为下面还会用到的（下面的情况为特例，需要释放）

  // 【特例】：假设当前进程为P1，已经获取了bcache.global_lock，那么为了避免在
  // if(holding(&bcache.lock[i])) continue;
  // else acquire(&bcache.lock[i]);
  // if和else 中间 有另外一个进程P2 进入了bget()而获取到对应的bcache.lock[i]，他也在上面没有找到可用的buffer cache，也进入到下面，就会造成死锁
  // 所以就需要先释放掉它，让原本的进程P1先处理
  if(holding(&bcache.global_lock))
    release(&bcache.lock[bno]);
  
  // global 有什么用?
  // 保证同一时刻，只能有一个进程进入下面的区域
  // 否则如果有多个进程同时进入下面的区域，极端情况下所有进程都在下面，那么所有进程都不会acquire成功，造成panic
  acquire(&bcache.global_lock);

  // 在【特例】情况下，P1完成后，P2进来就需要重新获取锁
  if(!holding(&bcache.lock[bno]))
    acquire(&bcache.lock[bno]);

  for (int i = 0; i < 13; i++)
  {
    if (i == bno) continue;

    if(holding(&bcache.lock[i])) continue;
    else acquire(&bcache.lock[i]);

    for (b = bcache.head[i].prev; b != &bcache.head[i]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->prev->next = b->next;
        b->next->prev = b->prev;
        
        // 把b对应的块，插回到[bno]中去
        b->prev = &bcache.head[bno];
        b->next = bcache.head[bno].next;

        bcache.head[bno].next->prev = b;
        bcache.head[bno].next = b;

        release(&bcache.lock[i]);
        release(&bcache.lock[bno]);
        release(&bcache.global_lock);
        acquiresleep(&b->lock);

        return b;
      }
    }

    // 别忘了没找到的时候需要释放
    release(&bcache.lock[i]);
  }

  release(&bcache.lock[bno]);
  release(&bcache.global_lock);

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");
  
  uint bno = b->blockno % 13;

  releasesleep(&b->lock);

  acquire(&bcache.lock[bno]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[bno].next;
    b->prev = &bcache.head[bno];
    bcache.head[bno].next->prev = b;
    bcache.head[bno].next = b;
  }
  release(&bcache.lock[bno]);
}

void bpin(struct buf *b)
{
  uint bno = b->blockno % 13;
  acquire(&bcache.lock[bno]);
  b->refcnt++;
  release(&bcache.lock[bno]);
}

void bunpin(struct buf *b)
{
  uint bno = b->blockno % 13;
  acquire(&bcache.lock[bno]);
  b->refcnt--;
  release(&bcache.lock[bno]);
}
