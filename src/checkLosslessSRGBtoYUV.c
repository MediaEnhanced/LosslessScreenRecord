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


//Mini helper program that checks that the various sRGB (8-bit) to YCbCr (10-bit)
//conversions have inverses

//Include C runtime library headers for simple portable mini helper program
#include <stdint.h>	//Defines Data Types: https://en.wikipedia.org/wiki/C_data_types
#include <stdlib.h>	//Needed for easy dynamic memory operations malloc & free
#include <stdio.h>	//Needed for printf statements and general file operations
#include <string.h> //Needed for memset

#include "math.h"

#define SRGB_MAX_VALUE 256
#define NUM_SRGB_VALUES 16777216 //SRGB_MAX_VALUE ^ 3
#define NUM_POSSIBLE_RESULTS (1 << 30)

void testSRGBtoYCbCr709(uint32_t* conversionLUT, uint16_t* conversionResults) {
	double Kr = 0.2126;
	double Kb = 0.0722;
	double Kg = (1.0 - Kr) - Kb;	
	double CbMult = 0.5 / (1.0 - Kb);
	double CrMult = 0.5 / (1.0 - Kr);
	
	double sRGBranged = 1.0 / 255.0;
	
	const double bitFactor = 1023.0;
	const int32_t checkFactor = 1023;
	const int32_t addFactor = 512;
	
	uint32_t* YCbCrLUT = conversionLUT;
	for (uint32_t red = 0; red < SRGB_MAX_VALUE; red++) {
		double R = ((double) red) * sRGBranged;
		double Yr = Kr * R;
		for (uint32_t green = 0; green < SRGB_MAX_VALUE; green++) {
			double G = ((double) green) * sRGBranged;
			double Yrg = (Kg * G) + Yr;
			for (uint32_t blue = 0; blue < SRGB_MAX_VALUE; blue++) {
				double B = ((double) blue) * sRGBranged;
				double Y = (Kb * B) + Yrg;
				
				double Cb = B - Y;
				double Cr = R - Y;
				Cb *= CbMult;
				Cr *= CrMult;
				
				Y *= bitFactor;
				if (Y > bitFactor) {
					Y = bitFactor;
				}
				else if (Y < 0.0) {
					Y = 0.0;
				}
				
				Cb *= bitFactor;
				Cr *= bitFactor;
				
				int32_t Yint = roundDouble(Y);
				int32_t Cbint = roundDouble(Cb);
				int32_t Crint = roundDouble(Cr);
				
				Cbint += addFactor;
				if (Cbint > checkFactor) {
					Cbint = checkFactor;
				}
				else if (Cbint < 0) {
					Cbint = 0;
				}
				
				Crint += addFactor;
				if (Crint > checkFactor) {
					Crint = checkFactor;
				}
				else if (Crint < 0) {
					Crint = 0;
				}
					
				*YCbCrLUT = (Yint << 20) | (Cbint << 10) | Crint;
				
				conversionResults[*YCbCrLUT]++;
				
				YCbCrLUT++;
				
				
			}
		}
	}
}

void analyzeSRGBtoYCbCr(uint64_t red, uint64_t green, uint64_t blue) {
	printf("Analyzing sRGB: %lld, %lld, %lld:\n", red, green, blue);
	
	const double sRGBranged = 1.0 / 255.0;
	
	const double Kr = 0.2126;
	const double Kb = 0.0722;
	double Kg = (1.0 - Kr) - Kb;	
	double CbMult = 0.5 / (1.0 - Kb);
	double CrMult = 0.5 / (1.0 - Kr);
	
	const double bitFactor = 1023.0;
	const int32_t checkFactor = 1023;
	const int32_t addFactor = 512;
	const double bitFactorInv = 1.0 / 1023.0;
	
	double R = ((double) red) * sRGBranged;
	double Yr = Kr * R;
	double G = ((double) green) * sRGBranged;
	double Yrg = (Kg * G) + Yr;
	double B = ((double) blue) * sRGBranged;
	double Y = (Kb * B) + Yrg;
	
	double Cb = B - Y;
	double Cr = R - Y;
	Cb *= CbMult;
	Cr *= CrMult;
	
	Y *= bitFactor;
	if (Y > bitFactor) {
		Y = bitFactor;
	}
	else if (Y < 0.0) {
		Y = 0.0;
	}
	int32_t Yint = roundDouble(Y);
	double Yinv = (double) Yint;
	Yinv *= bitFactorInv;
	
	//FFMPEG inverse check
	int32_t Cbint0 = roundDouble(Cb * bitFactor);
	int32_t Crint0 = roundDouble(Cr * bitFactor);
	Cbint0 += addFactor;
	if (Cbint0 > checkFactor) {
		Cbint0 = checkFactor;
	}
	else if (Cbint0 < 0) {
		Cbint0 = 0;
	}
	
	Crint0 += addFactor;
	if (Crint0 > checkFactor) {
		Crint0 = checkFactor;
	}
	else if (Crint0 < 0) {
		Crint0 = 0;
	}
	printf("FFMPEG 709 YCbCr (YUV): %d, %d, %d:\n", Yint, Cbint0, Crint0);
	
	
	double Cbinv0 = (double) (Cbint0 - addFactor);
	double Crinv0 = (double) (Crint0 - addFactor);
	Cbinv0 *= bitFactorInv;
	Crinv0 *= bitFactorInv;
	//Cbinv0 -= 0.5;
	//Crinv0 -= 0.5;
	
	double Binv0 = (Cbinv0 / CbMult) + Yinv;
	double Rinv0 = (Crinv0 / CrMult) + Yinv;
	double Ginv0 = (Yinv - (Binv0 * Kb) - (Rinv0 * Kr)) / Kg;
	
	int32_t Bint0 = roundDouble(Binv0 * 255.0);
	int32_t Rint0 = roundDouble(Rinv0 * 255.0);
	int32_t Gint0 = roundDouble(Ginv0 * 255.0);
	
	printf("FFMPEG 709 sRGB inverse: %d, %d, %d:\n", Rint0, Gint0, Bint0);
	
}

void yuvCreateTestFile(uint64_t yValue, uint64_t uValue, uint64_t vValue) {
	FILE* yuvFile = fopen("yuvTest.yuv", "wb");
	if (yuvFile == NULL) {
		printf("YUV output file could NOT be opened for writing\n");
		return;
	}
	
	uint64_t yMin = yValue-1;
	if (yValue == 0) {
		yMin = 1023;
	}
	uint64_t yMax = yValue+1;
	if (yValue == 1023) {
		yMax = 0;
	}
	
	uint64_t uMin = uValue-1;
	if (uValue == 0) {
		uMin = 1023;
	}
	uint64_t uMax = uValue+1;
	if (uValue == 1023) {
		uMax = 0;
	}
	
	uint64_t vMin = vValue-1;
	if (vValue == 0) {
		vMin = 1023;
	}
	uint64_t vMax = vValue+1;
	if (vValue == 1023) {
		vMax = 0;
	}
	
	
	//yMin <<= 6;
	//yValue <<= 6;
	//yMax <<= 6;
	for(uint64_t c=0; c<4; c++) {
		fwrite(&yMin, 2, 1, yuvFile);
		fwrite(&yValue, 2, 1, yuvFile);
		fwrite(&yMax, 2, 1, yuvFile);
		fwrite(&yValue, 2, 1, yuvFile);
	}
	
	//uMin <<= 6;
	//uValue <<= 6;
	//uMax <<= 6;
	for(uint64_t c=0; c<4; c++) {
		fwrite(&uMin, 2, 1, yuvFile);
		fwrite(&uValue, 2, 1, yuvFile);
		fwrite(&uMax, 2, 1, yuvFile);
		fwrite(&uValue, 2, 1, yuvFile);
	}
	
	//vMin <<= 6;
	//vValue <<= 6;
	//vMax <<= 6;
	for(uint64_t c=0; c<4; c++) {
		fwrite(&vMin, 2, 1, yuvFile);
		fwrite(&vValue, 2, 1, yuvFile);
		fwrite(&vMax, 2, 1, yuvFile);
		fwrite(&vValue, 2, 1, yuvFile);
	}
	
	
	fclose(yuvFile);
}

//Main C runtime entry point
int main(int argc, char* argv[]) {
	printf("\nConfirming the Lossless sRGB to YCbCr Conversion\n");
	
	//*
	uint32_t* conversionLUT = malloc(NUM_SRGB_VALUES * sizeof(uint32_t));
	uint16_t* conversionResults = malloc(NUM_POSSIBLE_RESULTS * sizeof(uint16_t));
	memset((void*) conversionResults, 0, NUM_POSSIBLE_RESULTS * sizeof(uint16_t));
	
	printf("Testing FFMPEG 709 sRGB to YCbCr (YUV) Conversion:\n");
	testSRGBtoYCbCr709(conversionLUT, conversionResults);
	uint32_t uniqueValues = 0;
	uint32_t failures = 0;
	for (uint32_t r=0; r<NUM_POSSIBLE_RESULTS; r++) {
		uint16_t result = conversionResults[r];
		if (result == 1) {
			uniqueValues++;
		}
		else if (result > 1) {
			failures++;
			//printf("Failure of %d for YUV value of %#08X\n", result, r);
		}
		
	}
	if (failures == 0) {
		printf("No Failures!\n");
	}
	else {
		printf("Failure Count: %d\n", failures);
	}
	printf("Number of Unique Values: %d\n", uniqueValues);
	
	free(conversionResults);
	free(conversionLUT);
	//*/
	
	//analyzeSRGBtoYCbCr(255, 0, 0);
	//yuvCreateTestFile(217, 395, 1023);
	
	printf("Program Successfully Finished!\n");
  return 0;
}

