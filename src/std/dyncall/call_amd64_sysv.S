// sysv x64_64:
// - call regs: rdi, rsi, rdx, rcx, r8, and r9
// - fpu call regs: xmm0 through xmm7
// - preserved regs: rbx, rsp, rbp, r12, r13, r14, and r15
// - scratch regs: rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11
// - return in rax:

#ifdef __APPLE__
.global _static_call_impl
_static_call_impl:
#else
.global static_call_impl
.type static_call_impl,@function
static_call_impl:
#endif
	.cfi_startproc
	push   %rbp
	.cfi_adjust_cfa_offset 8
	.cfi_offset rbp, -16
	mov    %rsp,%rbp
	.cfi_def_cfa_register rbp
	pushq  %rcx
	pushq  %r8
	mov    %rdi,%r10
	mov    %rsi,%rax
	mov    %rdx,%r11
	mov    (%rax),%rdi
	mov    0x8(%rax),%rsi
	mov    0x10(%rax),%rdx
	mov    0x18(%rax),%rcx
	mov    0x20(%rax),%r8
	mov    0x28(%rax),%r9
	movsd  0x30(%rax),%xmm0
	movsd  0x38(%rax),%xmm1
	movsd  0x40(%rax),%xmm2
	movsd  0x48(%rax),%xmm3
	movsd  0x50(%rax),%xmm4
	movsd  0x58(%rax),%xmm5
	movsd  0x60(%rax),%xmm6
	movsd  0x68(%rax),%xmm7
	2:  cmp    %r11,%rax
		je     3f
		sub    $0x8,%rax
		push   (%rax)
		jmp    2b
	3: call   *%r10
	mov    -0x8(%rbp),%r10
	mov    -0x10(%rbp),%r11
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
	.cfi_def_cfa rsp, 8
	retq
	.cfi_endproc

#ifdef __APPLE__
#define CSYM(name) _ ##name
.global _wrapper_call_impl
_wrapper_call_impl:
#else
#define CSYM(name) name
.global wrapper_call_impl
.type wrapper_call_impl,@function
wrapper_call_impl:
#endif
	.cfi_startproc
	pushq   %rbp
	.cfi_adjust_cfa_offset 8
	.cfi_offset rbp, -16
	mov    %rsp,%rbp
	.cfi_def_cfa_register rbp
	subq    $0x40, %rsp
	movsd   %xmm7, 0x38(%rsp)
	movsd   %xmm6, 0x30(%rsp)
	movsd   %xmm5, 0x28(%rsp)
	movsd   %xmm4, 0x20(%rsp)
	movsd   %xmm3, 0x18(%rsp)
	movsd   %xmm2, 0x10(%rsp)
	movsd   %xmm1, 0x8(%rsp)
	movsd   %xmm0, (%rsp)
	pushq   %r9
	pushq   %r8
	pushq   %rcx
	pushq   %rdx
	pushq   %rsi
	pushq   %rdi
	leaq    0x10(%rbp), %rdx
	movq    %rsp, %rsi
	subq    $0x10, %rsp
	movq    %rsp, %rcx
	movq    (%rdi), %r10
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
	.cfi_def_cfa rsp, 8
	retq
	.cfi_endproc
