.686
.MODEL FLAT, C
_text SEGMENT
static_call_impl PROC FAR C EXPORT
	.SAFESEH static_call_impl
	push ebp
	mov ebp, esp
	; set up stack args
    mov eax, [ebp + 12]
    mov edx, [ebp + 16]
	stack_copy:  cmp eax, edx
		jz fcall
		sub eax, 4
		push [eax]
		jmp stack_copy
	fcall:
	call dword ptr [ebp + 8]
    mov ecx, [ebp + 20]
	cmp ecx, 1
	je ret_void
	cmp ecx, 2
	je ret_int
	cmp ecx, 3
	je ret_float
	cmp ecx, 4
	je ret_final
	ud2
	ret_void:
		xor eax, eax
		jmp ret_final
	ret_int:
		mov ecx, [ebp + 24]
		mov [ecx], eax
		mov eax, ecx
		jmp ret_final
	ret_float:
		mov eax, [ebp + 24]
		fst qword ptr [eax]
		jmp ret_final
	ret_final:
	mov esp, ebp
	pop ebp
	ret
static_call_impl ENDP

EXTERN C wrapper_inner : PROC

wrapper_call_impl PROC FAR C EXPORT
	.SAFESEH wrapper_call_impl
	push ebp
	mov ebp, esp
    sub esp, 16
    push esp
    lea eax, dword ptr [ebp + 8]
    push eax
    xor eax, eax
    push eax
    push dword ptr [ebp + 8]

    mov ecx, [ebp + 8]
	mov ecx, [ecx]
	mov ecx, [ecx + 8]
	mov ecx, [ecx + 8]
	mov ecx, [ecx]
	cmp ecx, 5
	jz ret_single
	cmp ecx, 6
	jz ret_double
	call wrapper_inner
	jmp final
	ret_single:
		call wrapper_inner
		fld dword ptr [eax]
		jmp final
	ret_double:
		call wrapper_inner
		fld qword ptr [eax]
	final:
	mov esp, ebp
	pop ebp
	ret
wrapper_call_impl ENDP
_text ENDS
END