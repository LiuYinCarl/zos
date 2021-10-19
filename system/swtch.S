# 上下文切换
# void swtch(struct context **old, struct context *new) ;

.global swtch
swtch:
	movl 4(%esp), %eax
	movl 8(%esp), %edx

	# 保存进程寄存器数据
	pushl %ebp
	pushl %ebx
	pushl %esi
	push %edi

	# 却换栈
	movl %esp, (%eax)
	movl %edx, %esp

	# 加载新进程寄存器数据
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
