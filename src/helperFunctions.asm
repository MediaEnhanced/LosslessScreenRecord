;MIT License
;Copyright (c) 2023 Jared Loewenthal
;
;Permission is hereby granted, free of charge, to any person obtaining a copy
;of this software and associated documentation files (the "Software"), to deal
;in the Software without restriction, including without limitation the rights
;to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;copies of the Software, and to permit persons to whom the Software is
;furnished to do so, subject to the following conditions:
;
;The above copyright notice and this permission notice shall be included in all
;copies or substantial portions of the Software.
;
;THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
;SOFTWARE.

;x64 Assembly Code with Functions to be called by the main program
;Utilizes AVX2 and FMA instructions (needs a modernish CPU)
;Assembled by Flat Assembler (FASM) 
;Initial Version 0.1

format ELF64 ;This format is compatible with gcc / ld
;Uses Microsoft x64 Calling Convention
;rax, rcx, rdx, r8-r11 are volatile
;Trying not to directly modify the stack pointer register rsp
;Does Not "Push" or "Pop" Non-Volatile XMM registers
;Does Not do complete / comprehensible error checking (designed to run very fast)

public numToFHexStr ;64-bit number converted to a hexadecimal number UTF-8 string
numToFHexStr:
;rax: return value, rcx: number, rdx: strPtr
;string pointer (strPtr) is valid and has an adequate buffer size of at least 16 bytes
	add rdx, 15
	mov eax, 16
	mov r10, rcx
	mov r11, 0x30
	
	.loop:
		and rcx, 0xF
		mov r8, 0x37
		cmp rcx, 0xA
		cmovl r8, r11
		add rcx, r8
		shr r10, 4
		mov [rdx], cl
		mov rcx, r10
		dec rdx
		
		dec eax
	jnz .loop
	
ret ;numToFHexStr END

public numToPHexStr ;64-bit number converted to a minimal hexadecimal number UTF-8 string
numToPHexStr:
;rax: return value, rcx: number, rdx: strPtr
;string pointer (strPtr) is valid and has an adequate buffer size of at least 16 bytes
	bsr rax, rcx
	jz .zero
	
	shr rax, 2
	add rdx, rax
	add rax, 1
	mov r9, rax
	
	mov r10, rcx
	mov r11, 0x30
	
	.loop:
		and rcx, 0xF
		mov r8, 0x37
		cmp rcx, 0xA
		cmovl r8, r11
		add rcx, r8
		shr r10, 4
		mov [rdx], cl
		mov rcx, r10
		dec rdx
		
		dec eax
	jnz .loop
	mov eax, r9d
ret ;numToPHexStr Exit 1
	
	.zero:
		mov byte [rdx], 0x30
		mov eax, 1
ret ;numToPHexStr Exit 2


public numToUDecStr ;64-bit unsigned number converted to a decimal number UTF-8 string
numToUDecStr:
;rax: num digits (20 max) return value, rcx: strPtr, rdx: number
;string pointer (strPtr) is valid and has an adequate buffer size of at least 20 bytes
	mov r8, 0xCCCCCCCCCCCCCCCD
	mov rax, rdx
	mov r9, rdx
	
	.loop1Init:
	mov r11, 8
	xor r10, r10
	
	.loop1:
		dec r11
		jz .loop2Init
	
		mul r8
		add r9, 0x30
		shl r10, 8
		shr rdx, 3
		mov rax, rdx
		lea rdx, [rdx + rdx*4] ; rdx = rdx * 5
		shl rdx, 1 ; rdx *= 2
		sub r9, rdx 
		
		or r10, r9
		mov r9, rax
		
		test rax, rax
	jnz .loop1
	
	mov [rcx], r10
	mov eax, 8
	sub rax, r11
	
ret ;numToUDecStr Exit 1
	
	.loop2Init:
	push rbx
	mov rbx, r10
	mov r11, 8
	xor r10, r10
	
	.loop2:
		dec r11
		jz .loop3Init
	
		mul r8
		add r9, 0x30
		shl r10, 8
		shr rdx, 3
		mov rax, rdx
		lea rdx, [rdx + rdx*4] ; rdx = rdx * 5
		shl rdx, 1 ; rdx *= 2
		sub r9, rdx 
		
		or r10, r9
		mov r9, rax
		
		test rax, rax
	jnz .loop2
	
	mov [rcx], r10
	mov eax, 8
	sub rax, r11
	add rcx, rax
	mov [rcx], rbx
	
	pop rbx
	add rax, 8
ret ;numToUDecStr Exit 2
	
	.loop3Init:
	push rbp
	mov rbp, r10
	mov r11, 8
	xor r10, r10
	
	.loop3:
		dec r11
		
		mul r8
		add r9, 0x30
		shl r10, 8
		shr rdx, 3
		mov rax, rdx
		lea rdx, [rdx + rdx*4] ; rdx *= 5
		shl rdx, 1 ; rdx *= 2
		sub r9, rdx 
		
		or r10, r9
		mov r9, rax
		
		test rax, rax
	jnz .loop3
	
	mov [rcx], r10
	mov eax, 8
	sub rax, r11
	add rcx, rax
	mov [rcx], rbp
	mov [rcx + 8], rbx
	
	pop rbp
	pop rbx
	add rax, 16
ret ;numToUDecStr Exit 3

public shortToDecStr ;32,767 max unsigned number converted to a decimal number UTF-8 string
shortToDecStr:
;rax: return value, rcx: strPtr, rdx: number
;string pointer (strPtr) is valid and has an adequate buffer size of at least 20 bytes
	mov r8, 0xCCCCCCCD
	mov r9, rdx
	xor eax, eax
	xor r11, r11
	
	.loop:
		imul rdx, r8
		shl r11, 8
		add r9, 0x30
		shr rdx, 35
		lea r10, [rdx + rdx*4] ; r10 = rdx * 5
		shl r10, 1 ; r10 *= 2
		sub r9, r10
		
		or r11, r9
		mov r9, rdx
		
		inc eax
		test rdx, rdx
	jnz .loop
	
	mov [rcx], r11
	
ret ;shortToDecStr END

public roundDouble
roundDouble:
;rax: return value, xmm0: inputValue
	;cvtpd2pi mm0, xmm0
	;movd eax, mm0
	cvtsd2si eax, xmm0
ret ;roundDouble END

public exampleFunction ;forward real to complex FFT that works on powers of 2 N from 64 - 1024
exampleFunction:
;rax: return value, rcx: twiddlePtr, rdx: xRptr, r8: yCptr, r9: N
	
	push rbx
	push rbp
	push rdi
	push rsi
	push r12
	push r13
	
	;jmp .exitEarly
	.exitEarly:
	
	pop r13
	pop r12
	pop rsi
	pop rdi
	pop rbp
	pop rbx
ret ;exampleFunction END

