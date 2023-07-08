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


//Media Enhanced x64 Math Library Definitions
#ifndef MEDIA_ENHANCED_MATH_H
#define MEDIA_ENHANCED_MATH_H

#include <stdint.h> //Defines Data Types: https://en.wikipedia.org/wiki/C_data_types

//Unions for 32-bit and 64-bit numbers
union num32 {
	uint32_t u;
	int32_t i;
	float f;
};

union num64 {
	uint64_t u;
	int64_t i;
	double f;
};

double ldexp(double x, int exp);
int isnan(double x);
double cbrtFast(double x0);
uint32_t greatestCommonDivisor(uint32_t a, uint32_t b);

//The Microsoft x64 calling convention is used in all of the assembly functions:
//https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170 
#define ASM_CALLING_CONVENTION __attribute__((ms_abi))

int32_t ASM_CALLING_CONVENTION roundDouble(double value);
double ASM_CALLING_CONVENTION fmaDouble(double inputA, double inputB, double inputC);

//Correctly rounded exp2 and log2 functions for binary64 values
//Copied from the CORE-MATH project: https://core-math.gitlabpages.inria.fr/
double cr_exp2(double x); //Exponential of 2
double cr_log2(double x); //Base-2 Logarithm
//When both functions an equivalent power can be calculated


#endif //MEDIA_ENHANCED_MATH_H

