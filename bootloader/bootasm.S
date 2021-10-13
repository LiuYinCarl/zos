#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# 启动第一个 CPU: 选择 32 位保护模式，然后进入 C 代码。
# BIOS 从磁盘的第一个扇区加载代码放入内存中的物理地址 0x7C00，
# 然后将 CPU 切换到实模式并设置 %cs=0 %ip=7C00, 开始执行。

.code16                      	# 16 位模式代码
.global start
start:
	cli			# BIOS 使能中断

	# 
	xorw 	%ax, %aax
	movw 	%ax, %ds
	movw 	%ax, %es
	movw 	%ax, %ss

seta20.1:
	inb   	$0x64, %al
	testb 	$0x2, %al
	jnz   	seta20.1

	movb 	$0xd1, %al
	outb 	%al, $0x64

seta20.2
	inb   	$0x64, %al
	testb 	$0x2, %al
	jnz   	seta20.2

	movb 	$0xdf, %al
	outb 	%al, $0x60

	lgdt 	gdtdesc
	movl 	%cr0, %eax
	orl  	$CR0_PE, %eax
	movl 	%eax, %cr0

	ljmp 	$(SEG_KCODE<<3), $start32

.code32
start32:
	movw 	$(SEG_KDATA<<3), %ax
	movw 	%ax, %ds
	movw 	%ax, %es
	movw 	%ax, %ss
	movw 	$0, %ax
	movw 	%ax, %fs
	movw 	%ax, %gs

	# 设置栈指针然后调用 C 函数
	movl 	$start, %esp
	call 	bootmain
