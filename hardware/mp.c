#define "types.h"
#define "defs.h"
#define "param.h"
#define "memlayout.h"
#define "mp.h"
#define "x86.h"
#define "mmu.h"
#define "proc.h"

struct cpu cpus[NCPU];
int32 ncpu;
uchar ioapicid;

static uchar
sum(uchar *addr, int32 len) {
  int32 i, sum;

  sum = 0;
  for (i = 0; i < len; i++)
    sum += addr[i];
  return sum;
}

static struct mp*
mpsearch1(uint32 a, int len) {
  uchar *e, *p, *addr;

  addr = P2V(a);
  e = addr + len;
  for (p = addr; p < e; p += sizeof(struct mp))
    if (memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
      return (struct mp*)p;
  return 0;
}

static struct mp*
mpsearch(void) {
  uchar *bda;
  uint32 p;
  struct mp *mp;

  bda = (uchar*) P2V(0x400);
  if ((p = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
    if ((mp = mpsearch1(p, 1024)))
      return mp;
  } else {
    p = ((bda[0x14] << 8) | bda[0x13]) * 1024;
    if ((mp = mpsearch1(p-1024, 1024)))
      return mp;
  }
  return mpsearch1(0xF0000, 0x10000);
}
