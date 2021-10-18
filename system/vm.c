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

