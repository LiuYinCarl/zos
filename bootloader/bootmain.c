// 这个文件时 boot 块的一部分，在 bootmain.S 调用 bootmain() 后执行。
// bootasm.S 会将处理器设置为 32 位保护模式。
// bootmain() 从磁盘的第一个扇区加载一个 ELF 镜像，然后进入内核的正常加载流程。

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE 512

void readseg(uchar*, uint32, uint32);

void
bootmain(void) {
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  elf = (struct elfhdr*)0x10000; // TODO 这个地址是什么

  // 读取磁盘上的第一页
  readseg((uchar*)elf, 4096, 0);

  // 检查 ELF 文件是否合法
  if (elf->magic != ELF_MAGIC)
    return;

  // 加载程序段 (忽略 ph 标志位)
  ph = (struct proghdr*)((uchar*)elf + elf->phoff);
  eph = ph + elf->phnum;
  for (; ph < eph; ph++) {
    pa = (uchar*)ph->paddr;
    readseg(pa, ph->filesz, ph->off);
    if (ph->memsz > ph->filesz)
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // 从 ELF 文件头中调用入口点（不返回）
  entry = (void(*)(void))(elf->entry);
  entry();
}

void
waitdisk(void) {
  // 等待磁盘就绪
  while((inb(0x1F7) & 0xC0) != 0x40)
    ;
}

// 读入一个 offset 偏移位置的扇区到目标位置 dst
void
readsect(void *dst, uint32 offset) {
  waitdisk();
  outb(0x1F2, 1);
  outb(0x1F3, offset);
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20);

  // 读取数据
  waitdisk();
  insl(0x1F0, dst, SECTSIZE/4);
}

// 从内核的 offset 偏移处读取 count 字节的数据到物理地址 pa
// 复制的数据可能比 count 字节多
void
readseg(uchar* pa, uint32 count, uint32 offset) {
  uchar* epa;

  epa = pa + count;

  pa -= offset % SECTSIZE;

  offset = (offset / SECTSIZE) + 1;

  for (; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}
