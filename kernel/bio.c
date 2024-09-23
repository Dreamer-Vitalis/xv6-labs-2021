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
  struct spinlock global_lock; // Just for bget() when Not cached and can't find a LRU buffer.
  struct spinlock lock[13]; // Modify
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head[13]; // Modify
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

  
  release(&bcache.lock[bno]);
  acquire(&bcache.global_lock);

  for (int i = 0; i < 13; i++)
  {
    if (i == bno) continue;

    if(holding(&bcache.lock[i])) continue;
    acquire(&bcache.lock[i]);

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
        
        b->prev = &bcache.head[bno];
        b->next = bcache.head[bno].next;

        bcache.head[bno].next->prev = b;
        bcache.head[bno].next = b;

        release(&bcache.lock[i]);
        release(&bcache.global_lock);
        acquiresleep(&b->lock);

        return b;
      }
    }

    release(&bcache.lock[i]);
  }

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
