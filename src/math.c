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


//Media Enhanced x64 Math Functions
#include "math.h" //Include Math Function Definitions


double ldexp(double x, int exp) { //assumes exp range is fine
	union num64 num;
	num.i = exp + 1023;
	num.u <<= 52;
	return x * num.f;
}

int isnan(double x) { //good for all inputs?
  union num64 num;
	num.f = x;
	num.i &= 0x7fffffffffffffff;
	num.i = 0x7ff0000000000000 - num.i;
	num.u >>= 63;
	return (int) num.u;
}

double cbrtFast(double x0) {
	//Quick cubic root function that expects the input to be between 0.001953125 and 2.0
	//Uses a small lookup table defined below:
	
	const double CBRT2 = 1.2599210498948731648; //2^(1/3)
	const double SQR_CBRT2 = 1.5874010519681994748; // 2^(2/3)
	
	const double cbrtFactors[10] = {
		CBRT2 * 0.125,
		SQR_CBRT2 * 0.125,
		1.0 * 0.25,
		CBRT2 * 0.25,
		SQR_CBRT2 * 0.25,
		1.0 * 0.5,
		CBRT2 * 0.5,
		SQR_CBRT2 * 0.5,
		1.0,
		CBRT2
	};
	
	union num64 xCon0;
	xCon0.f = x0;
	
	uint64_t expFactor0 = xCon0.i;
	expFactor0 >>= 52;
	expFactor0 -= 1014;
	double f0 = cbrtFactors[expFactor0];
	
	xCon0.i &=    0xFFFFFFFFFFFFF; //Clear Exponential and Sign Bits of Number
	xCon0.i |= 0x3FE0000000000000; //Set value to be in range [0.5, 1.0)
	
	double a0 = xCon0.f;
	
	double b0 = -0.134661104733595206551;
	b0 *= a0;
	b0 += 0.546646013663955245034;
	b0 *= a0;
	b0 += -0.954382247715094465250;
	b0 *= a0;
	b0 += 1.13999833547172932737;
	b0 *= a0;
	b0 += 0.402389795645447521269;
	
	double y0 = b0 * f0;
	
	double c0, d0;
	
	//Do three iterations of Newton's Method to be safe
	c0 = y0 * y0 * 3.0;
	d0 = x0 / c0;
	y0 *= 2.0 / 3.0;
	y0 += d0;
	
	c0 = y0 * y0 * 3.0;
	d0 = x0 / c0;
	y0 *= 2.0 / 3.0;
	y0 += d0;
	
	c0 = y0 * y0 * 3.0;
	d0 = x0 / c0;
	y0 *= 2.0 / 3.0;
	y0 += d0;
	
	return y0;
}

uint32_t greatestCommonDivisor(uint32_t a, uint32_t b) {
	while ((a > 0) && (b > 0)) {
		if (a > b) {
			a = a % b;
			if (a == 0) {
				return b;
			}
		}
		else {
			b = b % a;
			if (b == 0) {
				return a;
			}
		}
	}
	return 0;
}

