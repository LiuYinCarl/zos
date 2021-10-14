# 操作系统内核从该文件开始执行。这个文件和内核 C 代码链接在一起。
# 所以可以引用到内核符号例如 main()。
# boot 块(bootasm.S bootmain.c)之后会进入下面的代码。

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

	# Multiboot 头部
.p2align 2
.text
.globl multiboot_header
multiboot_header:
	.long	magic
	.long	flags
	.long	(-magic-flags)

# 按照惯例，_start 符号代表 ELF 入口点。
# 因为还没有设置虚拟内存，所以入口点仍然是物理地址表示的入口点。
.globl	_start
_start	= V2P_WO(entry)

.globl	entry
entry:
	# 为 4 M 字节页打开页大小拓展
	movl	%cr4, %eax
	orl	$(CR4_PSE), %eax
	movl	%eax, %cr4

	# 设置页目录
	movl	$(V2P_WO(entrypgdir)), %eax
	movl	%eax, %cr3

	# 打开分页
	movl	%cr0, %eax
	orl	$(CR0_PG|CR0_WP), %eax
	movl	%eax, %cr0

	# 设置栈指针
	movl	$(stack + KSTACKSIZE), %esp

	# 跳入 main 函数并且选择从高地址开始执行。
	# 需要这个间接调用的原因是如果使用直接调用，
	# 汇编器会对直接 jump 指令生成和机器相关的指令。
	mov	$main, %eax
	jmp	*%eax

.comm stack, KSTACKSIZE
