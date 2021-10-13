#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# 启动第一个 CPU: 选择 32 位保护模式，然后进入 C 代码。
# BIOS 从磁盘的第一个扇区加载代码放入内存中的物理地址 0x7C00，
# 然后将 CPU 切换到实模式并设置 %cs=0 %ip=7C00, 开始执行。

