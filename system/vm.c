#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];
pde_t *kpgdir;

// 设置CUP 的内核段描述符，在进入每个 CPU 的时候运行一次。
void
seginit(void) {
  struct cpu *c;

  // 将逻辑地址映射到虚拟地址
  // 因为 CPU 禁止从 CPL=0 到 DPL=3 的中断，所以内核和用户空间不能共享代码段
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// 返回页表 pgdir 中对应与虚拟地址 va 的 PTE 的地址
// 如果 alloc != 0, 则创建需要的页表页
static pte_t*
walkpgdir(pde_t *pgdir, const void *va, int32 alloc) {
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if (*pde & PTE_P) {
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if (!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;

    memset(pgtab, 0, PGSIZE);

    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}


// 为指向物理地址 pa 处的虚拟地址 va 创建 PTE, va 和 size 可能不是页对齐的
static int32
mappages(pde_t *pgdir, void *va, uint32 size, uint32 pa, int32 perm) {
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint32)va);
  last = (char*)PGROUNDDOWN(((uint32)va) + size - 1);
  for (;;) {
    if ((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if (*pte & PTE_P)
      panic("remap");
    *pte = pa | perm | PTE_P;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

static struct kmap {
  void *virt;
  uint32 phys_start;
  uint32 phys_end;
  int32  perm;
} kmap[] = {
  { (void*)KERNBASE, 0, EXTMEM, PTE_W }, // IO space
  { (void*)KERNBASE, V2P(KERNLINK), V2P(data), 0 }, // kerntext+rodata
  { (void*)data, V2P(data), PHYSTOP, PTE_W }, // kerndata+memory
  { (void*)DEVSPACE, DEVSPACE, 0, PTE_W }, // more devices
};

// 设置页表的内核部分
pde_t*
setupkvm(void) {
  pde_t *pgdir;
  struct kmap *k;

  if ((pgdir = (pde_t*)kalloc()) == 0)
    return 0;
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for (k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if (mappages(pgdir, k->virt, k->phys_end - k->phys_start,
		 (uint32)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

void
kvmalloc(void) {
  kpgdir = setupkvm();
  switchkvm();
}

void
switchkcm(void) {
  lcr3(V2P(kpgdir));
}

void
switchuvm(struct proc *p) {
  if (p == 0)
    panic("switchuvm: no process");
  if (p->kstack == 0)
    panic("switchuvm: no kstack");
  if (p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
				sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint32)p->kstack + KSTACKSIZE;
  mycpu()->ts.iomb = (uint16) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));
  popcli();
}

void
inituvm(pde_t *pgdir, char *init, uint32 sz) {
  char *mem;
  if (sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// 加载一个内存段到 pgdir, addr 必须是页对齐的，addr 到 addr+sz 的页面必须是早就被映射的。
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint32 offset, uint32 sz) {
  uint32 i, pa, n;
  pte_t *pte;

  if ((uint32) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for (i = 0; i < sz; i += PGSIZE) {
    if ((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if (sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if (readi(ip, P2V(pa), offset + i, n) != n)
      return -1;
  }
  return 0;
}

int32
allocuvm(pde_t *pgdir, uint32 oldsz, uint32 newsz) {
  char *mem;
  uint32 a;

  if (newsz >= KERNBASE)
    return 0;
  if (newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for (; a < newsz; a += PGSIZE) {
    mem = kalloc();
    if (mem == 0) {
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0) {
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

int32
deallocuvm(pde_t *pgdir, uint32 oldsz, uint32 newsz) {
  pte_t *pte;
  uint32 a, pa;

  if (newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for (; a < oldsz; a += PGSIZE) {
    pte = walkpgdir(pgdir, (char*)a, 0);
    if (!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if ((*pte & PTE_P) != 0) {
      pa = PTE_ADDR(*pte);
      if (pa == 0)
	panic("kfree");
      char *v = P2V(pa);
      kfree(v);
      *pte = 0;
    }
  }
  return newsz;
}

// 释放一个内存页以及用户部分的所有物理内存
void
freevm(pde_t *pgdir) {
  uint32 i;

  if (pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0);
  for (i = 0; i < NPDENTRIES; i++) {
    if (pgdir[i] & PTE_P) {
      char *v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// 在一个页面山清除 PTE_U。创建一个在用户栈下的不可访问的页。
void
clearpteu(pde_t *pgdir, char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if (pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// 给定一个父进程的页表，为它的子进程创建一个页表的复制
pde_t*
copyuvm(pde_t *pgdir, uint32 sz) {
  pde_t *d;
  pte_t *pte;
  uint32 pa, i, flags;
  char *mem;

  if ((d = setupkvm()) == 0)
    return 0;
  for (i = 0; i < sz; i+= PGSIZE) {
    if ((pte = walkdir(pgdir, (void*)i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if (!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if (mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

 bad:
  freevm(d);
  return 0;
}

// 将用户虚拟地址映射到内核地址
char*
uva2ka(pde_t *pgdir, char *uva) {
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if ((*pte & PTE_U) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// 从 p 地址处复制 len 字节到 用户地址 va
// uva2ka 保证只在 PTE_U 页山生效。
int32
copyout(pde_t *pgdir, uint32 va, void *p, uint32 len) {
  char *buf, *pa0;
  uint32 n, va0;

  buf = (char*)p;
  while (len > 0) {
    va0 = (uint32)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if (n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

