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

;x64 Assembly Math Functions
;Utilizes AVX2 and FMA instructions (needs a modernish CPU)
;Assembled by Flat Assembler (FASM) 

format ELF64 ;This format is compatible with ld
;Uses Microsoft x64 Calling Convention
;rax, rcx, rdx, r8-r11 are volatile
;Trying not to directly modify the stack pointer register rsp
;Does Not "Push" or "Pop" Non-Volatile XMM registers

public roundDouble
roundDouble:
;rax: return value, xmm0: inputValue
	;cvtpd2pi mm0, xmm0
	;movd eax, mm0
	cvtsd2si eax, xmm0
ret ;roundDouble END

public fmaDouble ;A = (A * B) + C
fmaDouble:
;xmm0: return value and input A, xmm1: input B, xmm2: input C
	vfmadd132pd xmm0, xmm2, xmm1
ret ;fmaDouble END
