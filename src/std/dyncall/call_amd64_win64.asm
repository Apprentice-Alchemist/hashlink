_text SEGMENT
static_call_impl PROC FRAME
	push rbp
	.PUSHREG rbp
	mov rbp, rsp
	.SETFRAME rbp, 0
	.ENDPROLOG
	COMMENT # Move function ptr, stack begin and stack end #
	mov r10, rcx
	mov rax, rdx
	mov r11, r8
	COMMENT # set up call regs #
	mov rcx, [rax]
	mov rdx, [rax + 8]
	mov r8, [rax + 16]
	mov r9, [rax + 24]
	COMMENT # set up fpu call regs, #
	movsd xmm0, [rax + 32]
	movsd xmm1, [rax + 40]
	movsd xmm2, [rax + 48]
	movsd xmm3, [rax + 56]
	et up stack args
	@@:  cmp rax, r11
		jz @F
		sub rax, 8
		push [rax]
		jmp @B
	COMMENT # set up home area for rcx, rdx, r8 and r9 #
	@@:
	sub rsp, 32
	call r10
	cmp dword ptr [rbp + 40], 1
	je ret_void
	ret_void:
		xor rax, rax
		jmp ret_final
	ret_int:
		mov r11, [rbp + 48]
		mov [r11], rax
		mov rax, r11
		jmp ret_final
	ret_float:
		mov rax, [rbp + 48]
		movsd [rax], xmm0
		jmp ret_final
	ret_final:
	mov rsp, rbp
	pop rbp
	ret
static_call_impl ENDP
_text ENDS
END