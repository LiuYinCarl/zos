#define EXTMEM   0x100000   // 拓展内存开始地址
#define PHYSTOP  0xE000000  // 最高物理内存地址
#define DEVSPACE 0xFE000000 // 设备地址


#define KERNBASE 0x80000000        // 内核虚拟地址起始位置
#define KERNLINK (KERNBASE+EXTMEM) // 内核链接地址

#define V2P(a) (((uint) (a)) - KERNBASE)
#define P2V(a) ((void *)(((char *) (a)) + KERNBASE))

#define V2P_WO(x) ((x) - KERNBASE)
#define P2V_WO(x) ((x) + KERNBASE)
