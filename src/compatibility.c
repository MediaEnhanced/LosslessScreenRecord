//MIT License
//Copyright (c) 2023 Jared Loewenthal
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

//Media Enhanced Shared x64 Compatibility Implementation
#include "compatibility.h" 

//Make these assembly in the future
void* memcpyBasic(void* dest, const void* src, uint64_t count) {
	uint64_t count8 = count >> 3;
	
	uint64_t* destCpy8 = (uint64_t*) dest;
	uint64_t* srcCpy8 = (uint64_t*) src;
	while (count8 > 0) {
		*destCpy8 = *srcCpy8;
		destCpy8++;
		srcCpy8++;
		count8--;
	}
	
	uint64_t count1 = (uint64_t) (count & 0x7);
	uint8_t* destCpy1 = (uint8_t*) destCpy8;
	uint8_t* srcCpy1 = (uint8_t*) srcCpy8;
	while (count1 > 0) {
		*destCpy1 = *srcCpy1;
		destCpy1++;
		srcCpy1++;
		count1--;
	}
	
	return dest;
}

void* memzeroBasic(void* ptr, uint64_t size) {
	uint64_t count8 = size >> 3;
	uint64_t* destZero8 = (uint64_t*) ptr;
	for (uint64_t c8=0; c8<count8; c8++) {
		destZero8[c8] = 0;
	}
	
	uint64_t count1 = (uint64_t) (size & 0x7);
	uint8_t* destZero1 = (uint8_t*) destZero8;
	for (uint64_t c1=0; c1<count1; c1++) {
		destZero1[c1] = 0;
	}
	
	return ptr;
}



