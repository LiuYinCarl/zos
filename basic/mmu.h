// x86 内存管理单元 MMU

// Eflags 寄存器
#define FL_IF 0x00000200  // 中断使能

// 控制寄存器标志位
#define CR0_PE 0x00000001  // 保护使能 
#define CR0_WP 0x00010000  // 写保护
#define CR0_PG 0x80000000  // TODO Paging 怎么翻译？

#define CR4_PSE 0x00000010 // 页尺寸拓展

// 各类段选择器
#define SEG_KCODE 1 // 内核代码
#define SEG_KDATA 2 // 内核数据，内核栈
#define SEG_UCODE 3 // 用户代码
#define SEG_UDATA 4 // 用户数据，用户栈
#define SEG_TSS   5 // 处理器任务状态

// cpu->gdt[NSEGS] 保存上述段
#define NSEGS     6

#ifndef __ASSEMBLER__
// 段描述符
struct segdesc {
  uint32 lim_15_0	: 16;
  uint32 base_15_0	: 16;
  uint32 base_23_16	: 8;
  uint32 type		: 4;
  uint32 s		: 1;
  uint32 dpl		: 2;
  uint32 p		: 1;
  uint32 lim_19_16	: 4;
  uint32 avl		: 1;
  uint32 rsv1		: 1;
  uint32 db		: 1;
  uint32 g		: 1;
  uint32 base_31_24	: 8;
};

// 正常段
#define SEG(type, base, lim, dpl) (struct segdesc)         \
{ ((lim) >> 12) & 0xffff, (uint32)(base) & 0xffff,         \
  ((uint32)(base) >> 16) & 0xff, type, 1, dpl, 1,          \
  (uint32)(lim) >> 28, 0, 0, 1, 1, (uint32)(base) >> 24 }

#define SEG16(type, base, lim, dpl) (struct segdesc)       \
{ (lim) & 0xffff, (uint32)(base) & 0xffff,                 \
    ((uint32)(base) >> 16) & 0xff, type, 1, dpl, 1,        \
    (uint32)(lim) >> 16, 0, 0, 1, 0, (uint32)(base) >> 24 }
#endif

#define DPL_USER 0x3

// 应用段类型标志位
#define STA_X 0x8 // 可执行段
#define STA_W 0x2 // 可写（不可执行段）
#define STA_R 0x2 // 可读（不可执行段）

// 系统段类型标志位
#define STS_T32A 0x9 // 32 位 TSS 有效
#define STS_IG32 0xE // 32 位中断门
#define STS_TG32 0xF // 32 位陷阱门

// 页目录索引
#define PDX(va) (((uint32)(va) >> PDXSHIFT) & 0x3FF)

// 页表索引
#define PTX(va) (((uint32)(va) >> PTXSHIFT) & 0x3FF)

// 用索引和偏移构造虚拟地址
#define PGADDR(d, t, o) ((uint32)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// 页目录和页表常数
#define NPDENTRIES 1024 // 每个页目录的目录条目数
#define NPTENTRIES 1024 // 每个页表的 PTE 数量
#define PGSIZE     4096 // 页映射大小

#define PTXSHIFT   12  // 线性地址中 PTX 的偏移
#define PDXSHIFT   22  // 线性地址中 PDX 的偏移

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) ((a) & ~(PGSIZE-1))

