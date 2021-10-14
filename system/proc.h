// 每个 CPU 的状态
struct cpu {
  uchar apicid;               // local APIC ID
  struct context *scheduler;  // 在此处调用 swtch() 进入 scheduler()
  struct taskstate ts;        // x86 使用，用来找到中断栈
  struct segdesc gdt[NSEGS];  // x86 全局符号表 GDT
  volatile uint32 started;    // CPU 是否启动标志位
  int32 ncli;                 // pushcli 嵌套的深度
  int32 intena;               // 在 pushcli 之前中断是否被开启
  struct proc *proc;          // 当前在此 CPU 上运行的进程，没有则为 null
};

extern struct cpu cpus[NCPU];
extern int ncpu;


// 进程上下文切换时需要保存的寄存器。
struct context {
  uint32 edi;
  uint32 esi;
  uint32 ebx;
  uint32 ebp;
  uint32 eip;
};

enum procstate {
		UNUSED,
		EMBRYO,
		SLEEPING,
		RUNNABLE,
		RUNNING,
		ZOMBIE
};

// 每个进程的状态
struct proc {
  uint32 sz;                  // 进程内存大小 bytes
  pde_t *pgdir;               // 页表
  char *kstack;               // 进程内核栈底部
  enum procstate state;       // 进程状态
  int32 pid;                  // 进程 ID
  struct proc *parent;        // 父进程
  struct trapframe *tf;       // 当前系统调用的 trap frame
  struct context *context;    // 进程切换时保存的上下文
  void *chan;                 // 如果非 0，那么当前进程在 chan 上等待
  int32 killed;               // 如果非 0，那么当前进程已经死亡
  struct file *ofile[NOFILE]; // 打开文件列表
  struct inode *cwd;          // 当前目录
  char name[16];              // 进程名字
};
