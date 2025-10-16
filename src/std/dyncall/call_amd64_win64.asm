_text SEGMENT
static_call_impl PROC FRAME
	push rbp
	.PUSHREG rbp
	mov rbp, rsp
	.SETFRAME rbp, 0
	.ENDPROLOG
	; Move function ptr, stack begin and stack end
	mov r10, rcx
	mov rax, rdx
	mov r11, r8
	; mov [rbp + 16], rcx
	; mov [rbp + 24], rdx
	; mov [rbp + 32], r8
	mov [rbp + 40], r9
	; set up call regs
	mov rcx, [rax]
	mov rdx, [rax + 8]
	mov r8, [rax + 16]
	mov r9, [rax + 24]
	; set up fpu call regs
	movsd xmm0, qword ptr [rax + 32]
	movsd xmm1, qword ptr [rax + 40]
	movsd xmm2, qword ptr [rax + 48]
	movsd xmm3, qword ptr [rax + 56]
	; set up stack args
	@@:  cmp rax, r11
		jz @F
		sub rax, 8
		push [rax]
		jmp @B
	; set up home area for rcx, rdx, r8 and r9
	@@:
	sub rsp, 32
	call r10
	mov r10, [rbp + 40]
	cmp r10, 1
	je ret_void
	cmp r10, 2
	je ret_int
	cmp r10, 3
	je ret_float
	cmp r10, 4
	je ret_double
	cmp r10, 5
	je ret_final
	cmp r10, 6
	je ret_int64
	ud2
	ret_void:
		xor rax, rax
		jmp ret_final
	ret_int:
	ret_int64:
		mov r11, [rbp + 48]
		mov [r11], rax
		mov rax, r11
		jmp ret_final
	ret_float:
		mov rax, [rbp + 48]
		movss dword ptr [rax], xmm0
		jmp ret_final
	ret_double:
		mov rax, [rbp + 48]
		movsd qword ptr [rax], xmm0
		jmp ret_final
	ret_final:
	mov rsp, rbp
	pop rbp
	ret
static_call_impl ENDP

EXTERN wrapper_inner : PROC

wrapper_call_impl PROC FRAME
	push rbp
	.PUSHREG rbp
	mov rbp, rsp
	.SETFRAME rbp, 0
	.ENDPROLOG
	sub rsp, 32
	movsd qword ptr [rsp + 24], xmm3
	movsd qword ptr [rsp + 16], xmm2
	movsd qword ptr [rsp + 8], xmm1
	movsd qword ptr [rsp + 0], xmm0
	push r9
	push r8
	push rdx
	push rcx
	mov rdx, rsp
	lea r8, [rbp + 48]
	sub rsp, 16
	mov r9, rsp
	sub rsp, 32
	mov r10, [rcx]
	mov r10, [r10 + 8]
	mov r10, [r10 + 8]
	mov r10d, [r10]
	cmp r10d, 5
	jz @F
	cmp r10d, 6
	jz @F
	call wrapper_inner
	jmp final
	@@: call wrapper_inner
	movsd xmm0, qword ptr [rax]
	final:
	mov rsp, rbp
	pop rbp
	ret
wrapper_call_impl ENDP
_text ENDS
END