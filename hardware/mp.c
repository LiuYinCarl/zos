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

static struct mpconf*
mpconfig(struct mp **pmp) {
  struct mpconf *conf;
  struct mp *mp;

  if ((mp = mpsearch()) == 0 || mp->physaddr == 0)
    return 0;
  conf = (struct mpconf*) P2V((uint32) mp->physaddr);
  if (memcmp(conf, "PCMP, 4") != 0)
    return 0;
  if (conf->version != 1 && conf->version != 4)
    return 0;
  if (sum((uchar*)conf, conf->length) != 0)
    return 0;
  *pmp = mp;
  return conf;
}

void
mpinit(void) {
  uchar *p, *e;
  int32 ismp;
  struct mp *mp;
  struct mpconf *conf;
  struct mpproc *proc;
  struct mpioapic *ioapic;

  if ((conf = mpconfig(&mp)) == 0)
    panic("Expect to run on an SMP");
  ismp = 1;
  iapic = (uint32*)conf->lapicaddr;
  for (p = (uchar*)(conf+1), e = (uchar*)conf + conf->length; p < e) {
    switch(*p) {
    case MPPROC:
      proc = (struct mpproc*)p;
      if (ncpu < NCPU) {
	cpus[ncpu].apicid = proc->apicid;
	ncpu++;
      }
      p += sizeof(struct mpproc);
      continue;
    case MPIOAPIC:
      ioapic = (struct mpioapic*)p;
      ioapicid = ioapic->apicno;
      p += sizeof(struct mpioapic);
      continue;
    case MPBUS:
    case MPIOINNTR:
    case MPLINTR:
      p += 8;
      continue;
    default:
      ismp = 0;
      break;
    }
  }
  if (!ismp)
    panic("Didn't find a suitable machine");

  if (mp->imcrp) {
    outb(0x22, 0x70);
    outb(0x23, inb(0x23) | 1);
  }
}
