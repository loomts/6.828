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

struct {
  struct spinlock lock;
  struct buf buf[NBUF]; // 30

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.

  struct buf bucket[NBUCKET]; // 13
  struct spinlock block[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.block[i], "block");

    // Create linked list of buffers
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.bucket[i].next = &bcache.bucket[i];
  }
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bucket[0].next;
    b->prev = &bcache.bucket[0];
    initsleeplock(&b->lock, "buffer");
    bcache.bucket[0].next->prev = b;
    bcache.bucket[0].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  // printf("blockno:%d\n", blockno);
  int hash = blockno % NBUCKET;
  acquire(&bcache.block[hash]);
  // Is the block already cached?
  for(b = bcache.bucket[hash].next; b != &bcache.bucket[hash]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.block[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.bucket[hash].prev; b != &bcache.bucket[hash]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.block[hash]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  for(int i = 0; i < NBUCKET; i++) {
    if(i == hash)continue;
    acquire(&bcache.block[i]);
    int flag = 0;
    for(b = bcache.bucket[i].prev; b != &bcache.bucket[i]; b = b->prev){
      if(b->refcnt == 0) {
        // printf("%d steal from %d\n",hash, i);
        flag = 1;
        b->next->prev = b->prev;
        b->prev->next = b->next;
        b->next = bcache.bucket[hash].next;
        b->prev = &bcache.bucket[hash];
        bcache.bucket[hash].next->prev = b;
        bcache.bucket[hash].next = b;
        release(&bcache.block[i]);
        break;
      }
    }
    if(!flag)
    {
      release(&bcache.block[i]);
      continue;
    }
    for(b = bcache.bucket[hash].prev; b != &bcache.bucket[hash]; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.block[hash]);
        acquiresleep(&b->lock);
        return b;
      }
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  // printf("brelse\n");
  releasesleep(&b->lock);

  int hash = b->blockno % NBUCKET;
  acquire(&bcache.block[hash]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[hash].next;
    b->prev = &bcache.bucket[hash];
    bcache.bucket[hash].next->prev = b;
    bcache.bucket[hash].next = b;
  }
  
  release(&bcache.block[hash]);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


