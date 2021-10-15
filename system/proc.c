#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int32 nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
print(void) {
  initlock(&ptable.lock, "ptable");
}


// 必须要在关闭中断的情况下调用
// TODO 为什么是 - 号
int32
cpuid() {
  return mycpu()-cpus;
}

struct cpu*
mycpu(void) {
  int32 apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();

  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

static struct proc*
allocproc(void) {
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;

  release(&ptable.lock);

  // 分配内核栈
  if ((p->kstack = kalloc()) == 0) {
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // 为 trapframe 腾出空间
  sp -= sizeof(*p->tf);
  p->tf = (struct trapframe*)sp;

  // 设置新上下文用来执行 forkret, forkret 会返回到 trapret
  sp -= 4;
  *(uint32*)sp = (uint32)trapret;

  sp -= sizeof(*p->context);
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof(*p->context));
  p->context->eip = (uint32)forkret;

  return p;
}

// 设置第一个用户进程
void
userinit(void) {
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int32)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // initcode.S 的开始处

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // 对 p->state 的赋值让其他 CPU 核心运行这个进程。
  // acquire 调用会让上面的赋值代码先执行。
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  release(&ptable.lock);
}

// 当前进程的内存增加 n 字节。
// 成功时返回 0，失败返回 -1。
int32
growproc(int32 n) {
  uint32 sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0) {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if (n < 0) {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }

  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// 以当前进程为父进程，复制父进程创建一个新进程
//
int32
fork(void) {
  int32 i, pid;

  struct proc *np;
  struct proc *curproc = myproc();

  if ((np = allocproc()) == 0) {
    return -1;
  }

  if (())
  
}
