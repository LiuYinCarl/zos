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

	# 零数据段寄存器 DS, ES, SS
	xorw 	%ax, %aax
	movw 	%ax, %ds
	movw 	%ax, %es
	movw 	%ax, %ss

	# TODO 这段汇编的含义
seta20.1:
	inb   	$0x64, %al 	# 等待空闲
	testb 	$0x2, %al
	jnz   	seta20.1

	movb 	$0xd1, %al	# 0xd1 -> port 0x64
	outb 	%al, $0x64

seta20.2
	inb   	$0x64, %al	# 等待空闲
	testb 	$0x2, %al
	jnz   	seta20.2

	movb 	$0xdf, %al	# 0xdf -> port 0x60
	outb 	%al, $0x60

	# 从实模式切换到保护模式。使用 bootstrap GDT 让虚拟地址直接映射到物理地址，
	# 以便在这个过程中实际内存映射不会发生改变。
	lgdt 	gdtdesc
	movl 	%cr0, %eax
	orl  	$CR0_PE, %eax
	movl 	%eax, %cr0

	# 通过使用一个 long jmp 重载 %cs 和 %eip 来完成到 32 位保护模式的转换。
	# 段描述符没有转换，所以映射还是不变的
	ljmp 	$(SEG_KCODE<<3), $start32

.code32 # 告诉汇编器现在生成 32 位代码
start32:
	# 设置保护模式数据段寄存器
	movw 	$(SEG_KDATA<<3), %ax	# 数据段选择器
	movw 	%ax, %ds		# DS: Data Segment
	movw 	%ax, %es		# ES: Extra Segment
	movw 	%ax, %ss		# SS: Stack Segment
	movw 	$0, %ax			# 
	movw 	%ax, %fs		# FS
	movw 	%ax, %gs		# GS

	# 设置栈指针然后调用 C 函数
	movl 	$start, %esp
	call 	bootmain

	# 如果 bootmain 返回了（正常情况不应该返回）并且程序运行在调试器下
	# 则触发一个调试器断点然后循环。
	movw	$0x8a00, %ax
	movw	%ax, %dx
	outw	%ax, %dx
	movw	$0x8ae0, %ax
	outw	%ax, %dx
spin:
	jmp	spin

# bootstrap GDT
.p2align 2				# 强制 4 字节对齐
gdt:
	SEG_NULLASM				# null seg
	SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)	# code seg
	SEG_ASM(STA_W, 0x0, 0xffffffff)		# data seg

gdtdesc:
	.word	(gdtdesc - gdt - 1)		# sizeof(gdt) - 1
	.long 	gdt				# assress gdt
