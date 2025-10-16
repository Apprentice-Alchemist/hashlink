#ifdef __USER_LABEL_PREFIX__
#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

#define CSYM(name) CONCAT1(__USER_LABEL_PREFIX__, name)
#else
#define CSYM(name) name
#endif

.global CSYM(static_call_impl)
.seh_proc CSYM(static_call_impl)
CSYM(static_call_impl):
	push   %rbp
	.seh_pushreg %rbp
	mov    %rsp,%rbp
	.seh_setframe %rbp, 0
	.seh_endprologue
	mov    %rcx,%r10
	mov    %rdx,%rax
	mov    %r8,%r11

	mov %r9, 40(%rbp)

	mov    (%rax),%rcx
	mov    0x8(%rax),%rdx
	mov    0x10(%rax),%r8
	mov    0x18(%rax),%r9
	movsd  0x20(%rax),%xmm0
	movsd  0x28(%rax),%xmm1
	movsd  0x30(%rax),%xmm2
	movsd  0x38(%rax),%xmm3
	2:  cmp    %r11,%rax
		je     3f
		sub    $0x8,%rax
		push   (%rax)
		jmp    2b
	3:
	# set up a home area for rcx, rdx, r8 and r9
	sub $0x20, %rsp
	call   *%r10
	mov    0x28(%rbp),%r10
	mov    0x30(%rbp),%r11
	# void
	cmpl $0x1, %r10d
	je 4f
	# int
	cmpl $0x2, %r10d
	je 5f
	# float
	cmpl $0x3, %r10d
	je 6f
	# double
	cmpl $0x4, %r10d
	je 7f
	# ptr
	cmpl $0x5, %r10d
	je 8f
	# int64
	cmpl $0x6, %r10d
	je 5f
	4:
		xor %rax, %rax
		jmp 8f
	5:
		mov %rax, (%r11)
		mov %r11, %rax
		jmp 8f
	6:
		movss %xmm0, (%r11)
		mov %r11, %rax
		jmp 8f
	7:
		movsd %xmm0, (%r11)
		mov %r11, %rax
	8:
	movq    %rbp,%rsp
	popq    %rbp
	retq
	.seh_endproc

.global CSYM(wrapper_call_impl)
.seh_proc CSYM(wrapper_call_impl)
CSYM(wrapper_call_impl):
	push   %rbp
	.seh_pushreg %rbp
	mov    %rsp,%rbp
	.seh_setframe %rbp, 0
	.seh_endprologue
	subq    $0x20, %rsp
	movsd   %xmm3, 0x18(%rsp)
	movsd   %xmm2, 0x10(%rsp)
	movsd   %xmm1, 0x8(%rsp)
	movsd   %xmm0, (%rsp)
	push %r9
	push %r8
	push %rdx
	push %rcx
	movq    %rsp, %rdx
	leaq    0x30(%rbp), %r8
	movq    %rsp, %r9
	subq    $0x10, %rsp
	movq    (%rcx), %r10
	movq    0x8(%r10), %r10
	movq    0x8(%r10), %r10
	movl    (%r10), %r10d
	cmpl    $0x5, %r10d
	je      2f
	cmpl    $0x6, %r10d
	je      2f
	callq   CSYM(wrapper_inner)
	jmp     3f
	2: 
	callq   CSYM(wrapper_inner)
	movsd   (%rax), %xmm0
	3:
	movq    %rbp, %rsp
	popq    %rbp
	retq
	.seh_endproc
