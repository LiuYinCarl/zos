#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[];

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int32 use_lock;
  struct run *freelist;
} kmem;

void
kinit1(void *vstart, void *vend) {
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend) {
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend) {
  char *p;
  p = (char*)PGROUNDUP((uint32)vstart);
  for (; p + PGSIZE <= (char*)vend; p += PGSIZE)
    kfree(p);
}

void
kfree(char *v) {
  struct run *r;

  if ((uint32)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // 填充无效数据，用来尽早 catch，避免悬垂引用
  memset(v, 1, PGSIZE);

  if (kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if (kmem.use_lock)
    release(&kmem.lock);
}

char*
kalloc(void) {
  struct run *r;

  if (kmem.use_lock)
    acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  if (kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}
