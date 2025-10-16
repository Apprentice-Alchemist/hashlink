#ifdef __USER_LABEL_PREFIX__
#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

#define CSYM(name) CONCAT1(__USER_LABEL_PREFIX__, name)
#else
#define CSYM(name) name
#endif

.global CSYM(static_call_impl)
#ifdef __ELF__
.type CSYM(static_call_impl),@function
.hidden CSYM(static_call_impl)
#endif
CSYM(static_call_impl):
	.cfi_startproc
	push   %ebp
	.cfi_adjust_cfa_offset 8
	.cfi_offset ebp, -8
	mov    %esp,%ebp
	.cfi_def_cfa_register ebp
	mov 0xc(%ebp), %eax
	2:  cmp    0x10(%ebp), %eax
		je     3f
		sub    $0x4,%eax
		push   (%eax)
		jmp    2b
	3: call *0x8(%ebp)
	mov 0x14(%ebp), %ecx
	cmpl $0x1, %ecx
	je .Lret_void
	cmpl $0x2, %ecx
	je .Lret_int
	cmpl $0x3, %ecx
	je .Lret_float
	cmpl $0x4, %ecx
	je .Lret_double
	cmpl $0x5, %ecx
	je .Lret_ptr
	cmpl $0x6, %ecx
	je .Lret_int64
	.Lret_void:
		xor %eax, %eax
		jmp .Lret_final
	.Lret_int:
		mov 0x18(%ebp), %ecx 
		mov %eax, (%ecx)
		mov %ecx, %eax
		jmp .Lret_final
	.Lret_float:
		mov 0x18(%ebp), %ecx
		fsts (%ecx)
		mov %ecx, %eax
		jmp .Lret_final
	.Lret_double:
		mov 0x18(%ebp), %ecx
		fstl (%ecx)
		mov %ecx, %ecx
		jmp .Lret_final
	.Lret_int64:
		mov 0x18(%ebp), %ecx 
		mov %eax, (%ecx)
		mov %edx, 0x4(%ecx)
		mov %ecx, %eax
	.Lret_ptr:
	.Lret_final:
	mov    %ebp,%esp
	pop    %ebp
	.cfi_def_cfa esp, 4
	ret
	.cfi_endproc

.global CSYM(wrapper_call_impl)
#ifdef __ELF__
.type CSYM(wrapper_call_impl),@function
.hidden CSYM(wrapper_call_impl)
#endif
CSYM(wrapper_call_impl):
	.cfi_startproc
	push   %ebp
	.cfi_adjust_cfa_offset 8
	.cfi_offset ebp, -8
	mov    %esp,%ebp
	.cfi_def_cfa_register ebp
	sub $0x10, %esp
	push %esp
	lea 0x8(%ebp), %eax
	push %eax
	xor %eax, %eax
	push %eax
	push 0x8(%ebp)
	mov 0x8(%ebp), %ecx # v
	mov (%ebp), %ecx # v->cl.t
	mov 0x8(%ecx), %ecx # v->cl.t->fun
	mov 0x8(%ecx), %ecx # v->cl.t->fun->ret
	mov (%ecx), %ecx # v->cl.t->fun->ret->kind
	cmpl    $0x5, %ecx
	je      2f
	cmpl    $0x6, %ecx
	je      3f
	cmpl $0x4, %ecx
	je 4f
	cmpl $0x17, %ecx
	je 4f
	call   CSYM(wrapper_inner)@plt
	jmp     5f
	2:
	call   CSYM(wrapper_inner)@plt
	flds (%eax)
	jmp 5f
	3: 
	call   CSYM(wrapper_inner)@plt
	fldl (%eax)
	jmp 5f
	4:
	call CSYM(wrapper_inner)@plt
	mov 0x4(%eax), %edx
	mov (%eax), %eax
	5:
	mov    %ebp,%esp
	pop    %ebp
	.cfi_def_cfa esp, 4
	ret
	.cfi_endproc
