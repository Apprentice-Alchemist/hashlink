#ifdef __USER_LABEL_PREFIX__
#define CONCAT1(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

#define CSYM(name) CONCAT1(__USER_LABEL_PREFIX__, name)
#else
#define CSYM(name) name
#endif

#ifdef __ARM_EABI__
#define UNWIND(...) __VA_ARGS__
#else
#define UNWIND(...)
#endif

#ifdef __APPLE__
#define FP r7
#else
#define FP r11
#endif

.syntax unified

.global CSYM(static_call_impl)
#ifdef __ELF__
.type CSYM(static_call_impl),function
.hidden CSYM(static_call_impl)
#endif
#ifdef __APPLE__
.private_extern CSYM(static_call_impl)
#endif
CSYM(static_call_impl):
	UNWIND(.fnstart)
	UNWIND(.save	{FP, lr})
	push	{FP, lr}
	UNWIND(.setfp	FP, sp)
	mov	FP, sp
	// a1: fn_ptr
	// a2: stack_top
	// a3: stack_bottom
	// a4: ret_flags
	// [fp + 12]: ret_ptr
	push {a1, a2, a3, a4}
	mov a4, a2
	0: cmp a4, a3
		bls 1f
		ldrd a1, a2, [a4, #-8]!
		strd a1, a2, [a4, #-8]!
		b 0b
	1:
	ldr ip, [FP, #-8]
	ldm ip, {a1, a2, a3, a4}
	#ifdef __ARM_PCS_VFP
	add	ip, ip, #16
	vldmgt ip, {d0-d7}
	#endif
	ldr ip, [FP, #-4]
	blx ip

	ldr a3, [FP, #-12]
	ldr a4, [FP, #12] // return ptr

	#void
	cmp a3, 1
	beq 4f
	# int
	cmp a3, 2
	beq 5f
	# float
	cmp a3, 3
	#ifdef __ARM_PCS_VFP
	beq 7f
	#else
	beq 5f
	#endif
	# double
	cmp a3, 4
	#ifdef __ARM_PCS_VFP
	beq 8f
	#else
	beq 6f
	#endif
	# ptr
	cmp a3, 4
	beq 5f
	# int64
	cmp a3, 4
	beq 6f
	# void
	4:
		eor r0, r0, r0
		b 9f
	# single word
	5:
		str r0, [a4]
		mov r0, a4
		b 9f
	# double word
	6:
		strd r0, r1, [a4]
		mov r0, a4
		b 9f
	#ifdef __ARM_PCS_VFP
	# float
	7:
		vstr s0, [a4]
		mov r0, a4
		b 9f
	# double
	8:
		vstr d0, [a4]
		mov r0, a4
		b 9f
	#endif
	9:
	mov sp, FP
	pop	{FP, pc}
	UNWIND(.cantunwind)
	UNWIND(.fnend)

.global CSYM(wrapper_call_impl)
#ifdef __ELF__
.type CSYM(wrapper_call_impl),function
.hidden CSYM(wrapper_call_impl)
#endif
#ifdef __APPLE__
.private_extern CSYM(wrapper_call_impl)
#endif
CSYM(wrapper_call_impl):
	UNWIND(.fnstart)
	UNWIND(.save	{FP, lr})
	push	{FP, lr}
	UNWIND(.setfp	FP, sp)
	mov	FP, sp

	#ifdef __ARM_PCS_VFP
	vpush {d0-d7}
	#endif
	push {a1, a2, a3, a4}
	# struct regs *regs
	mov a2, sp
	# char *stack
	add a3, FP, 12
	# allocate space for vdynamic *ret
	sub sp, sp, 16
	mov a4, sp

	blx CSYM(wrapper_inner)

	ldr ip, [sp] // ->t
	ldr ip, [sp] // ->kind
	cmp ip, 5
	beq 0f
	cmp ip, 6
	beq 1f
	cmp ip, 4
	beq 2f
	cmp ip, 23
	beq 2f
	b 3f
	0: // float
	vldr s0, [r0]
	beq 3:
	1: // double
	vldr d0, [r0]
	beq 3:
	2: // int64
	ldrd r0, r1, [r0]
	3:
	mov sp, FP
	pop	{FP, pc}
	UNWIND(.cantunwind)
	UNWIND(.fnend)