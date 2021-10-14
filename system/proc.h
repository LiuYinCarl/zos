// pre-CPU state
struct cpu {
  uchar apicid;
  struct context *scheduler;
  struct taskstate ts;
  struct segdesc gdt[NSEGS];
  volatile uint32 started;
  int32 ncli;
  int32 intena;
  struct proc *proc;
};
