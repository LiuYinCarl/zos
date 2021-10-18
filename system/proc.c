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

  // 复制进程栈
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // 清理 eax 寄存器，以便让子进程 fork 返回 0
  np->tf->eax = 0;

  for (i = 0; i < NOFILE, i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

// 退出当前进程，不返回。
// 退出的进程状态变为 zombie，直到
// 父进程调用 wait() 来处理。
void
exit(void) {
  struct proc *curproc = myproc();
  struct proc *p;
  int32 fd;

  if (curproc == initproc)
    panic("init exiting");

  // 关闭所有打开的文件
  for (fd = 0; fd < NOFILE; fd++) {
    if (curproc->ofile[fd]) {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  input(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);
  // 父进程可能在睡眠
  wakeup1(curproc->parent);

  // 将该进程的子进程交给 init 进程管理
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->parent == curproc) {
      p->parent = initproc;
      if (p->state == ZOMBIE)
	wakeup1(initproc);
    }
  }

  // 进入 scheduler, 不返回
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// 等待一个子进程退出，返回子进程的 pid
// 如果这个进程没有子进程则返回 -1。
int32
wait(void) {
  struct proc *p;
  int32 havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;) {
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->parent != curproc)
	continue;
      havekids = 1;
      if (p->state == ZOMBIE) {
	pid = p->pid;
	kfree(p->kstack);
	p->kstack = 0;
	freevm(p->pgdir);
	p->pid = 0;
	p->parent = 0;
	p->name[0] = 0;
	p->killed = 0;
	p->state = UNUSED;
	release(&ptable.lock);
	return pid;
      }
    }

    if (!havekids || curpeoc->killed) {
      release(&ptable.lock);
      return -1;
    }

    // 等待子进程退出
    sleep(curproc, &ptable.lock);
  }
}

void
scheduler(void) {
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;) {
    // 在这个 cpu 中开启中断
    sti();

    // 寻找一个可以运行的进程
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
      if (p->state != RUNNABLE)
	continue;

      // 控制权转移到被选中的进程。被选中的进程要负责释放
      // ptable.lock。然后在回到本函数之前再获取 ptable.lock。
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      c->proc = 0;
    }
    release(&ptable.lock);
  }
}


void
sched(void) {
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// 放弃 CUP
void
yield(void) {
  acquire(&ptable.lock);
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

void
forkret(void) {
  static int first = 1;

  release(&ptable.lock);

  if (first) {
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // 返回到调用者，通常是 trapret
}

void
sleep(void *chan, struct spinlock *lk) {
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // 为了改变 p->state 和调用 sched，必须要持有 ptable.lock
  // 一旦持有了 ptable.lock, 可以保证不会错过任何 wakeup(因为要
  // wakeup 要执行必须要持有 ptable.lock)
  if (lk == &ptable.lock) {
    acquire(&ptable.lock);
    release(lk);
  }

  p->chan = chan;
  p->state = SLEEPING;

  sched();

  p->chan = 0;

  if (lk != &ptable.lock) {
    release(&pta)
  }
}

