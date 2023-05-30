// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

uint64 page_count[(PHYSTOP - KERNBASE) / PGSIZE];
struct spinlock page_counter_lock;
// get page count from pa
uint64 get_page_count(uint64 pa){
  acquire(&page_counter_lock);
  int cnt = page_count[(pa-KERNBASE)/PGSIZE];
  release(&page_counter_lock);
  return cnt;
}

// add page count by pa
void add_page_count(uint64 pa){
  acquire(&page_counter_lock);
  page_count[(pa-KERNBASE)/PGSIZE]++;
  release(&page_counter_lock);
}

// minus page count by pa
void minus_page_count(uint64 pa){
  acquire(&page_counter_lock);
  page_count[(pa-KERNBASE)/PGSIZE]--;
  release(&page_counter_lock);
}

void set_page_count(uint64 pa,int val){
  acquire(&page_counter_lock);
  page_count[(pa-KERNBASE)/PGSIZE]=val;
  release(&page_counter_lock);
}
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_counter_lock, "page_counter_lock");
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  if(get_page_count((uint64)pa)>1){
    minus_page_count((uint64)pa);
    return;
  }
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    set_page_count((uint64)r,1);
  }
  return (void*)r;
}
