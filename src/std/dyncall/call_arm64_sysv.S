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
#ifdef __APPLE__
.private_extern CSYM(static_call_impl)
#endif
CSYM(static_call_impl):
	.cfi_startproc
	#ifdef __arm64e__
	.cfi_b_key_frame
	pacibsp
	#elif !defined(__APPLE__)
	paciasp
	.cfi_negate_ra_state
	#endif
	stp fp, lr, [sp, #-16]!
	.cfi_adjust_cfa_offset 16
	mov fp, sp
	.cfi_def_cfa_register fp
	# Move function ptr, stack begin and stack end
	mov x9, x0
	mov x10, x1
	mov x11, x2
	stp x3, x4, [sp, #-16]!
	# set up call regs
	ldp x0, x1, [x10]
	ldp x2, x3, [x10, 16]
	ldp x4, x5, [x10, 32]
	ldp x6, x7, [x10, 48]
	# set up fpu call regs,
	ldp d0, d1, [x10, 64]
	ldp d2, d3, [x10, 80]
	ldp d4, d5, [x10, 96]
	ldp d6, d7, [x10, 112]
	# set up stack args
	0:  cmp x10, x11
		bls 1f
	# the stack is always aligned to 16 bytes
		ldp x12, x13, [x10, #-16]!
		stp x12, x13, [sp, #-16]!
		b 0b
	1:
	#ifdef __arm64e__
	blraaz x9
	#else
	blr x9
	#endif
	ldp x3, x4, [fp, -16]
	# void
	cmp x3, 1
	beq 4f
	# int
	cmp x3, 2
	beq 5f
	# float
	cmp x3, 3
	beq 6f
	# double
	cmp x3, 4
	beq 7f
	# ptr
	cmp x3, 4
	beq 8f
	# int64
	cmp x3, 4
	beq 5f
	4:
		mov x0, xzr
		b 8f
	5:
		str x0, [x4]
		mov x0, x4
		b 8f
	6:
		str s0, [x4]
		mov x0, x4
		b 8f
	7:
		str d0, [x4]
		mov x0, x4
	8:
	mov sp, fp
	ldp fp, lr, [sp], #16
	.cfi_def_cfa sp, 0
	#ifdef __arm64e__
	retab
	#else
	#if !defined(__APPLE__)
	autiasp
	.cfi_negate_ra_state
	ret
	#endif
	ret
	#endif
	.cfi_endproc

.global CSYM(wrapper_call_impl)
#ifdef __ELF__
.type CSYM(wrapper_call_impl),@function
.hidden CSYM(wrapper_call_impl)
#endif
#ifdef __APPLE__
.private_extern CSYM(wrapper_call_impl)
#endif
CSYM(wrapper_call_impl):
	.cfi_startproc
	#ifdef __arm64e__
	.cfi_b_key_frame
	pacibsp
	#else
	paciasp
	#endif
	.cfi_negate_ra_state
	stp fp, lr, [sp, #-16]!
	.cfi_adjust_cfa_offset 16
	mov fp, sp
	.cfi_def_cfa_register fp
	sub sp, sp, 128
	stp x0, x1, [sp]
	stp x2, x3, [sp, 16]
	stp x4, x5, [sp, 32]
	stp x6, x7, [sp, 48]
	stp d0, d1, [sp, 64]
	stp d2, d3, [sp, 80]
	stp d4, d5, [sp, 96]
	stp d6, d7, [sp, 112]
	add x2, fp, 16
	mov x1, sp
	sub sp, sp, 16
	mov x3, sp
	ldr x9, [x0] // ->t
	ldr x9, [x9, 8] // ->fun
	ldr x9, [x9, 8] // ->ret
	ldr w9, [x9] // ->kind
	cmp w9, 5
	beq 0f
	cmp w9, 6
	beq 0f
	bl CSYM(wrapper_inner)
	b 1f
	0: bl CSYM(wrapper_inner)
	ldr d0, [x0]
	1:
	mov sp, fp
	ldp fp, lr, [sp], #16
	.cfi_def_cfa sp, 0
	#ifdef __arm64e__
	retab
	#else
	#if !defined(__APPLE__)
	autiasp
	.cfi_negate_ra_state
	#endif
	ret
	#endif
	.cfi_endproc

#if defined __ELF__ && defined __linux__
#define GNU_PROPERTY_AARCH64_BTI (1 << 0)
#define GNU_PROPERTY_AARCH64_POINTER_AUTH (1 << 1)
	.section .note.GNU-stack,"",%progbits

	.pushsection .note.gnu.property, "a";
	.balign 8;
	.long 4;
	.long 0x10;
	.long 0x5;
	.asciz "GNU";
	.long 0xc0000000; /* GNU_PROPERTY_AARCH64_FEATURE_1_AND */
	.long 4;
	.long GNU_PROPERTY_AARCH64_BTI | GNU_PROPERTY_AARCH64_POINTER_AUTH;
	.long 0;
	.popsection;
#endif
