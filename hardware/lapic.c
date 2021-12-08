#include "param.h"
#include "types.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "traps.h"
#include "mmu.h"
#include "x86.h"

#define ID       (0x0020/4)
#define VER      (0x0030/4)
#define TPR      (0x0080/4)
#define EOI      (0x00B0/4)
#define SVR      (0x00F0/4)
#define ENABLE   0x00000100
#define ESR      (0x0280/4)
#define ICRLO    (0x0300/4)
#define INIT     0x00000500
#define STARTUP  0x00000600
#define DELIVS   0x00001000
#define ASSERT   0x00004000
#define DEASSERT 0x00000000
#define LEVEL    0x00008000
#define BCAST    0x00080000
#define BUSY     0x00001000
#define FIXED    0x00000000
#define ICRHI    (0x0310/4)
#define TIMER    (0x0320/4)
#define X1       0x0000000B
#define PERIODIC 0x00020000
#define PCINT    (0x0340/4)
#define LINT0    (0x0350/4)
#define LINT1    (0x0360/4)
#define ERROR    (0x0370/4)
#define MASKED   0x00010000
#define TICR     (0x0380/4)
#define TCCR     (0x0390/4)
#define TDCR     (0x03E0/4)

volatile uiint32 *lapic; // 在 mp.c 中被初始化

static void
lapicw(int32 index, int32 value) {
  lapic[index] = value;
  lapic[ID];
}

void
lapicinit(void) {
  if (!lapic)
    return;

  lapicw(SVR, ENABLE | (T_IRQ0 + IRQ_SPURIOUS));

  lapicw(TDCR, X1);
  lapicw(TIMER, PERIODIC | (T_IRQ0 + IRQ_TIMER));
  lapicw(TICR, 10000000);

  lapicw(LINT0, MASKED);
  lapicw(LINE1, MASKED);

  if (((lapic[VER]>>16) & 0xFF) >= 4)
    lapicw(PCINT, MASKED);

  lapicw(ERROR, T_IRQ0 + IRQ_ERROR);

  // todo 这里为什么写两次
  lapicw(ESR, 0);
  lapicw(ESR, 0);

  lapicw(EOI, 0);

  lapicw(ICRHI, 0);
  lapicw(ICRLO, BCAST | INIT | LEVEL);
  while (lapic[ICRLO] & DELIVS)
    ;

  lapicw(TPR, 0);
}

int
lapicid(void) {
  if (!lapic)
    return 0;
  return lapic[ID] >> 24;
}

void
lapiceoi(void) {
  if (lapic)
    lapicw(EOI, 0);
}

void
microdelay(int us) {
  
}

#define CMOS_PORT   0x70
#define CMOS_RETURN 0x71

void
lapicstartap(uchar apicid, uint32 addr) {
  int32 i;
  uint16 *wrv;

  outb(CMOS_PORT, 0xF);
  
}
