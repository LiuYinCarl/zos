static inline uchar
inb(uint16 port) {
  uchar data;
  asm volatile("in %1,%0"
	       : "=a" (data)
	       : "d" (port));
  return data;
}

static inline void
insl(int port, void *addr, int cnt) {
  asm volatile("cld; rep insl"
	       : "=D" (addr), "=c" (cnt)
	       : "d" (port), "0" (addr), "1" (cnt)
	       : "memory", "cc");
}

static inline void
outb(uint16 port, uchar data) {
  asm volatile("out %0,%1"
	       :
	       : "a" (data), "d" (port));
}

static inline void
outw(uint16 port, uint16 data) {
  asm volatile("out %0,%1"
	       :
	       : "a" (data), "d" (port));
}

static inline void
outsl(int32 port, const void *addr, int32 cnt) {
  asm volatile("cld; rep outsl"
	       : "=S" (addr), "=c" (cnt)
	       : "d" (port), "0" (addr), "1" (cnt)
	       : "cc");
}

static inline void
stosb(void *addr, int32 data, int32 cnt) {
  asm volatile("cld; rep stosb"
	       : "=D" (addr), "=c" (cnt)
	       : "0" (addr), "1" (cnt), "a" (data)
	       : "memcoy", "cc");
}

static inline void
stosl(void *addr, int32 data, int32 cnt) {
  asm volatile("cld; rep stosl"
	       : "=D" (addr), "=c" (cnt)
	       : "0" (addr), "1" (cnt), "a" (data)
	       : "memory", "cc");
}

struct segdesc;

static inline void
lgdt(struct segdesc *p, int32 size) {
  volatile uint16 pd[3];

  pd[0] = size-1;
  pd[1] = (uint32)p;
  pd[2] = (uint32)p >> 16;

  asm volatile("lgdt (%0)"
	       :
	       : "r" (pd));
}

struct gatedesc;

static inline void
lidt(struct gatedesc *p, int32 size) {
  volatile uint16 pd[3];

  pd[0] = size-1;
  pd[1] = (uint32)p;
  pd[2] = (uint32)p >> 16;

  asm volatile("lidt (%0)"
	       :
	       : "r" (pd));
}

static inline void
ltr(uint16 sel) {
  asm volatile("ltr %0"
	       :
	       : "r" (sel));
}

static inline uint16
readeflags(void) {
  uint32 eflags;
  asm volatile("pushfl; popl %0"
	       : "=r" (eflags));
  return eflags;
}

static inline void
loadgs(uint16 v) {
  asm volatile("movw %0, %%gs"
	       :
	       : "r" (v));
}

static inline void
cli(void) {
  asm volatile("cli");
}

static inline void
sti(void) {
  asm volatile("sti");
}

static inline uint32
xchg(volatile uint32 *addr, uint32 newval) {
  uint32 result;
  asm volatile("lock; xchgl %0, %1"
	       : "+m" (*addr), "=a" (result)
	       : "1" (newval)
	       : "cc");
  return result;
}

static inline uint32
rcr2(void) {
  uint32 val;
  asm volatile("movl %%cr2,%0"
	       : "=r" (val));
  return val;
}

static inline void
lcr3(uint32 val) {
  asm volatile("movl %0,%%cr3"
	       :
	       : "r" (val));
}

struct trapframe {
  uint32 edi;
  uint32 esi;
  uint32 ebp;
  uint32 oesp; // 未使用
  uint32 ebx;
  uint32 edx;
  uint32 ecx;
  uint32 eax;

  uint16 gs;
  uint16 padding1;
  uint16 fs;
  uint16 padding2;
  uint16 es;
  uint16 padding3;
  uint16 ds;
  uint16 padding4;
  uint32 trapno;

  uint32 err;
  uint32 eip;
  uint16 cs;
  uint16 padding5;
  uint32 eflags;
  
  uint32 esp;
  uint16 ss;
  uint16 padding6;
};
