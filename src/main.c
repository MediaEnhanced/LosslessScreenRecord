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


//This is the main file for the Lossless Screen Record program for x64
//processor architecture targets and is designed to be compiled by gcc
//This top-level program file contains the entry point and OS independent code
//When OS dependent functionality needs to be used, the linked in compatibility
//functions get called which share the same function definitions (prototypes)
//These functions make their own calls to the OS API and abstract it away from
//the main program (sometimes the majority of the code executed are in these functions)
//The program avoids using any runtime library functions which usually vary
//between OSes and insteads calls specific implementations for relevant functions
// More Details Here in the Future

#include "programEntry.h" //Includes "programStrings.h" & "compatibility.h" & <stdint.h>
#include "math.h" //Includes the math function definitions

//During the Make process the GLSL Vulkan Compute Shader gets compiled to SPIR-V
//and then this binary data gets linked into the program via the following definitons
extern uint64_t shader_size;
extern uint8_t  shader_data[];

//Definition constants used for sRGB loops:
#define SRGB_MAX_VALUE 256
#define NUM_SRGB_VALUES 16777216

double getLinearSRGBChannelValuefromSRGBChannelByte(uint8_t v) {
	//Uses the sRGB transfer function and operates on a sRGB channel byte independently of the others
	double base = ((double) v) / 255.0; // for 8-bit value
	if (base > 0.04045) { // > ? ... Doesn't matter for 8-bit values 0 -> 10
		base = (base + 0.055) / 1.055; // Test for > 1?
		
		double power = 2.4;
		//double result = pow(base, power);
		double result = cr_log2(base);
		result *= power;
		result = cr_exp2(result);
		
		return result;
	}
	else {
		double result = base / 12.92;
		return result;
	}
}

uint32_t getSRGBChannelBytefromLinearSRGBChannelValue(double v) {
	if (v > 1.0) {
		return 0xFF;
	}
	else if (v > 0.0031308) {
		const double power = 1.0 / 2.4;
		
		//double result = pow(v, power);
		double result = cr_log2(v);
		result *= power;
		result = cr_exp2(result);
		
		result = (result * 1.055) - 0.055;
		return roundDouble(result * 255.0);
	}
	else if (v > 0.0) {
		const double mult = 12.92 * 255.0;
		return roundDouble(v * mult);
	}
	
	return 0;
}

void createLinearSRGBcomponentLUT(uint64_t numOfValues, float* dataLUT) {
	//uint64_t numOfValues = 1 << componentBits;
	double maxValue = (double) (numOfValues - 1);
	
	uint64_t firstSectionValues = ((uint64_t) (0.04045 * maxValue)) + 1;
	double multFactor = 1.0 / (12.92 * maxValue);
	for (uint64_t i=0; i<firstSectionValues; i++) {
		double value = (double) i;
		dataLUT[i] = (float) (value * multFactor);
	}
	
	double baseMultFactor = 1.0 / (1.055 * maxValue);
	double baseAddFactor = (0.055 / 1.055);
	double power = 2.4;
	for (uint64_t i=firstSectionValues; i<numOfValues; i++) {
		double base = (double) i;
		double res = (base * baseMultFactor) + baseAddFactor;
		res = cr_log2(res);
		res = res * power;
		res = cr_exp2(res);
		dataLUT[i] = (float) res;
	}
}

void createSRGBcomponentLUT(uint64_t numOfValues, float* dataLUT) {
	//uint64_t numOfValues = 1 << componentBits;
	double maxValue = (double) (numOfValues - 1);
	
	union num32 res;
	
	uint64_t firstSectionValues = ((uint64_t) ((0.04045 * maxValue) - 0.5)) + 1;
	double multFactor = 1.0 / (12.92 * maxValue);
	double multFactorInv = 12.92 * maxValue;
	for (uint64_t i=0; i<firstSectionValues; i++) {
		double value = ((double) i) + 0.5;
		res.f = (float) (value * multFactor);
		
		//*
		double resD = (double) res.f;
		resD = resD * multFactorInv;
		uint32_t result = roundDouble(resD);
		if (result > i) {
			res.i--;
		}
		//*/
		
		dataLUT[i] = (float) res.f;
	}
	
	double baseMultFactor = 1.0 / (1.055 * maxValue);
	double baseAddFactor = (0.055 / 1.055);
	double power = 2.4;
	double baseMultFactorInv = (1.055 * maxValue);
	double powerInv = 1.0 / power;
	for (uint64_t i=firstSectionValues; i<numOfValues; i++) {
		double base = ((double) i) + 0.5;
		double resD = (base * baseMultFactor) + baseAddFactor;
		resD = cr_log2(resD);
		resD = resD * power;
		resD = cr_exp2(resD);
		res.f = (float) resD;
		
		//*
		resD = (double) res.f;
		resD = cr_log2(resD);
		resD = resD * powerInv;
		resD = cr_exp2(resD);
		resD = resD - baseAddFactor;
		resD = resD * baseMultFactorInv;
		uint32_t result = roundDouble(resD);
		if (result > i) {
			res.i--;
		}
		//*/
		
		dataLUT[i] = (float) res.f;
	}
}

#define XYB_BIAS 0.0037930732552754493
#define XYB_BIAS_CBRT 0.15595420054924862
#define XYB_P1_CONST 0.0 //0.0182 //0.0 orginally
#define XYB_P2_CONST 0.5

void getXYBfromLinearSRGB(double r, double g, double b, double* XPtr, double* YPtr, double* BPtr) {	
	const double r0 = 0.3;
	const double g0 = 0.622;
	const double b0 = 0.078;
	const double r1 = 0.23;
	const double g1 = 0.692;
	const double b1 = 0.078;
	const double r2 = 0.24342268924547819;
	const double g2 = 0.20476744424496821;
	const double b2 = 0.5518098665095537;
	
	//Linear sRGB values will ensure that the three mixes are between XYB_BIAS and (XYB_BIAS + 1.0)
	double Lmix = XYB_BIAS + (b * b0) + (r * r0) + (g * g0);
	double Mmix = XYB_BIAS + (b * b1) + (r * r1) + (g * g1);
	double Smix = XYB_BIAS + (g * g2) + (r * r2) + (b * b2);
	
	Lmix = cbrtFast(Lmix) - XYB_BIAS_CBRT;
	Mmix = cbrtFast(Mmix) - XYB_BIAS_CBRT;
	Smix = cbrtFast(Smix) - XYB_BIAS_CBRT;
	
	double p1 = XYB_P1_CONST;
	double p2 = XYB_P2_CONST;
	*XPtr = (Lmix * (p1 + 1.0)) + (Mmix * (p1 - 1.0));
	double Y = Lmix + Mmix;
	*YPtr = Y; // Luma has no contribution from Smix
	*BPtr = Smix - (Y * p2);
}

void getLinearSRGBfromXYB(double X, double Y, double B, double* rPtr, double* gPtr, double* bPtr) {
	const double p1 = XYB_P1_CONST;
	const double p2 = XYB_P2_CONST;
	
	double Lmix = (Y * (1.0 - p1) + X) * 0.5;
	double Mmix = (Y * (1.0 + p1) - X) * 0.5;
	double Smix = B + (Y * p2);
	
	Lmix += XYB_BIAS_CBRT;
	Mmix += XYB_BIAS_CBRT;
	Smix += XYB_BIAS_CBRT;
	
	double LmixC = (Lmix * Lmix * Lmix) - XYB_BIAS;
	double MmixC = (Mmix * Mmix * Mmix) - XYB_BIAS;
	double SmixC = (Smix * Smix * Smix) - XYB_BIAS;
	
	*rPtr = (LmixC * 11.031566901960783) - (MmixC * 9.866943921568629) - (SmixC * 0.16462299647058826);
	*gPtr = (MmixC * 4.418770392156863) - (LmixC * 3.254147380392157) - (SmixC * 0.16462299647058826);
	*bPtr = (MmixC * 2.7129230470588235) - (LmixC * 3.6588512862745097) + (SmixC * 1.9459282392156863);
}

void populateSRGBtoXYBlut(uint32_t* lutData, double* lutHelper) {
	for (uint32_t v = 0; v < SRGB_MAX_VALUE; v++) {
		lutHelper[v] = getLinearSRGBChannelValuefromSRGBChannelByte((uint8_t) v);
	}
	
	double Xmin =  9000;
	double Xmax = -9000;
	double Ymin =  9000;
	double Ymax = -9000;
	double Bmin =  9000;
	double Bmax = -9000;
	
	for (uint32_t red = 0; red < SRGB_MAX_VALUE; red++) {
		double r = lutHelper[(uint8_t) red];
		for (uint32_t green = 0; green < SRGB_MAX_VALUE; green++) {
			double g = lutHelper[(uint8_t) green];
			for (uint32_t blue = 0; blue < SRGB_MAX_VALUE; blue++) {
				double b = lutHelper[(uint8_t) blue];
				
				double X, Y, B;
				getXYBfromLinearSRGB(r, g, b, &X, &Y, &B);
				
				if (X < Xmin) {
					Xmin = X;
				}
				if (X > Xmax) {
					Xmax = X;
				}
				
				if (Y < Ymin) {
					Ymin = Y;
				}
				if (Y > Ymax) {
					Ymax = Y;
				}
				
				if (B < Bmin) {
					Bmin = B;
				}
				if (B > Bmax) {
					Bmax = B;
				}
			}
		}
	}
	
	double maxValueConvert = (double) (1023.0);//(SRGB_MAX_VALUE - 1);
	
	double Xrange = Xmax - Xmin;
	double Yrange = Ymax - Ymin;
	double Brange = Bmax - Bmin;
	
	double Xmult = maxValueConvert / Xrange;
	double Ymult = maxValueConvert / Yrange;
	double Bmult = maxValueConvert / Brange;
	
	double Xadd = 0.0 - (Xmult * Xmin);
	double Yadd = 0.0 - (Ymult * Ymin);
	double Badd = 0.0 - (Bmult * Bmin);
	
	uint32_t* data = lutData;
	for (uint32_t red = 0; red < SRGB_MAX_VALUE; red++) {
		double r = lutHelper[(uint8_t) red];
		for (uint32_t green = 0; green < SRGB_MAX_VALUE; green++) {
			double g = lutHelper[(uint8_t) green];
			for (uint32_t blue = 0; blue < SRGB_MAX_VALUE; blue++) {
				double b = lutHelper[(uint8_t) blue];
				
				double X, Y, B;
				getXYBfromLinearSRGB(r, g, b, &X, &Y, &B);
				
				X = (X * Xmult) + Xadd;
				Y = (Y * Ymult) + Yadd;
				B = (B * Bmult) + Badd;
				
				uint32_t XConvert = roundDouble(X);
				uint32_t YConvert = roundDouble(Y);
				uint32_t BConvert = roundDouble(B);
				
				//uint32_t XYBresult = (XConvert << 16) | (YConvert << 8) | (BConvert);
				uint32_t XYBresult = (YConvert << 20) | (XConvert << 10) | (BConvert);
				
				*data = XYBresult;
				data++;				
			}
		}
	}
}

void populateSRGBtoYCbCr8BitLUT(uint32_t* lutData) {
	uint32_t* YCbCr8 = lutData;
	
	for (uint32_t r = 0; r < SRGB_MAX_VALUE; r++) {
		double red = ((double) r) / 255.0;
		double Y1 = 0.299 * red;
		double Cb1 = (0.299 / 1.772) * red;
		double Cr1 = 0.5 * red;
		for (uint32_t g = 0; g < SRGB_MAX_VALUE; g++) {
			double green = ((double) g) / 255.0;
			double Y2 = 0.587 * green;
			double Cb2 = (0.587 / 1.772) * green;
			double Cr2 = (0.587 / 1.402) * green;
			for (uint32_t b = 0; b < SRGB_MAX_VALUE; b++) {
				double blue = ((double) b) / 255.0;
				double Y3 = 0.114 * blue;
				double Cb3 = 0.5 * blue;
				double Cr3 = (0.114 / 1.402) * blue;
				
				double Y = (Y1 + Y2 + Y3) * 255.0;
				double Cb = (Cb3 - Cb1 - Cb2) * 255.0 + 128.0;
				double Cr = (Cr1 - Cr2 - Cr3) * 255.0 + 128.0;
				
				uint32_t Yint = roundDouble(Y);
				uint32_t Cbint = roundDouble(Cb);
				uint32_t Crint = roundDouble(Cr);
				
				*YCbCr8 = (Yint << 16) | (Cbint << 8) | Crint;
				YCbCr8++;
			}
		}
	}
}

void populateSRGBtoYCbCr10BitLUT(uint32_t* lutData) {
	uint32_t* YCbCr10 = lutData;
	
	for (uint32_t r = 0; r < SRGB_MAX_VALUE; r++) {
		double red = ((double) r) / 255.0;
		double Y1 = 0.299 * red;
		double Cb1 = (0.299 / 1.772) * red;
		double Cr1 = 0.5 * red;
		for (uint32_t g = 0; g < SRGB_MAX_VALUE; g++) {
			double green = ((double) g) / 255.0;
			double Y2 = 0.587 * green;
			double Cb2 = (0.587 / 1.772) * green;
			double Cr2 = (0.587 / 1.402) * green;
			for (uint32_t b = 0; b < SRGB_MAX_VALUE; b++) {
				double blue = ((double) b) / 255.0;
				double Y3 = 0.114 * blue;
				double Cb3 = 0.5 * blue;
				double Cr3 = (0.114 / 1.402) * blue;
				
				double Y = (Y1 + Y2 + Y3) * 1023.0;
				double Cb = (Cb3 - Cb1 - Cb2) * 1023.0 + 512.0;
				double Cr = (Cr1 - Cr2 - Cr3) * 1023.0 + 512.0;
				
				uint32_t Yint = roundDouble(Y);
				uint32_t Cbint = roundDouble(Cb);
				uint32_t Crint = roundDouble(Cr);
				
				*YCbCr10 = (Yint << 20) | (Cbint << 10) | Crint;
				YCbCr10++;
			}
		}
	}
}

void populateSRGBtoYCbCr10Bit709LUT(uint32_t* lutData) {
	uint32_t* YCbCr10 = lutData;
	
	for (uint32_t r = 0; r < SRGB_MAX_VALUE; r++) {
		double red = ((double) r) / 255.0;
		double Y1 = 0.2126 * red;
		double Cb1 = 0.1146 * red;
		double Cr1 = 0.5 * red;
		for (uint32_t g = 0; g < SRGB_MAX_VALUE; g++) {
			double green = ((double) g) / 255.0;
			double Y2 = 0.7152 * green;
			double Cb2 = 0.3854 * green;
			double Cr2 = 0.4542 * green;
			for (uint32_t b = 0; b < SRGB_MAX_VALUE; b++) {
				double blue = ((double) b) / 255.0;
				double Y3 = 0.0722 * blue;
				double Cb3 = 0.5 * blue;
				double Cr3 = 0.0458 * blue;
				
				double Y = (Y1 + Y2 + Y3) * 1023.0;
				if (Y > 1023.0) {
					Y = 1023.0;
				}
				else if (Y < 0.0) {
					Y = 0.0;
				}
				double Cb = (Cb3 - Cb1 - Cb2) * 1023.0 + 512.0;
				if (Cb > 1023.0) {
					Cb = 1023.0;
				}
				else if (Cb < 0.0) {
					Cb = 0.0;
				}
				double Cr = (Cr1 - Cr2 - Cr3) * 1023.0 + 512.0;
				if (Cr > 1023.0) {
					Cr = 1023.0;
				}
				else if (Cr < 0.0) {
					Cr = 0.0;
				}
				
				uint32_t Yint = roundDouble(Y);
				uint32_t Cbint = roundDouble(Cb);
				uint32_t Crint = roundDouble(Cr);
				
				*YCbCr10 = (Yint << 20) | (Cbint << 10) | Crint;
				YCbCr10++;
			}
		}
	}
}

//sRGB to xvYCbCr LUT generation for both 601 (sYCC) and 709 (version > 0)
//Using ITU-T H.273 as a reference
//Color Primaries are always from 709:
//    x       y    primary
// 0.300   0.600   green
// 0.150   0.060   blue
// 0.640   0.330   red
// 0.3127  0.3290  white D65
//10-bit version when bits > 0, otherwise 8-bit version
void populateSRGBtoXVYCbCrLUT(uint32_t* lutData, uint32_t version, uint32_t bits) {
	double Kr = 0.299;
	double Kb = 0.114;
	if (version > 0) {
		Kr = 0.2126;
		Kb = 0.0722;
	}
	double Kg = (1.0 - Kr) - Kb;	
	double CbMult = 0.5 / (1.0 - Kb);
	double CrMult = 0.5 / (1.0 - Kr);
	
	double sRGBranged = 1.0 / 255.0;
	
	double bitFactor = 255.0;
	if (bits > 0) {
		bitFactor = 1023.0;
	}
	
	uint32_t* xvYCbCr = lutData;
	
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
				Cb += 0.5;
				Cr += 0.5;
				
				Y *= bitFactor;
				if (Y > bitFactor) {
					Y = bitFactor;
				}
				else if (Y < 0.0) {
					Y = 0.0;
				}
				
				Cb *= bitFactor;
				Cb += 0.5; //Needed only for FFMPEG almost perfect conversion
				if (Cb > bitFactor) {
					Cb = bitFactor;
				}
				else if (Cb < 0.0) {
					Cb = 0.0;
				}
				
				Cr *= bitFactor;
				Cr += 0.5; //Needed only for FFMPEG almost perfect conversion
				if (Cr > bitFactor) {
					Cr = bitFactor;
				}
				else if (Cr < 0.0) {
					Cr = 0.0;
				}
				
				int32_t Yint = roundDouble(Y);
				int32_t Cbint = roundDouble(Cb);
				int32_t Crint = roundDouble(Cr);
				
				if (bits == 0) {
					*xvYCbCr = (Yint << 16) | (Cbint << 8) | Crint;
				}
				else {
					*xvYCbCr = (Yint << 20) | (Cbint << 10) | Crint;
				}
				xvYCbCr++;
			}
		}
	}
}

void populateSRGBtoXVYCbCrLUT2(uint32_t* lutData, uint32_t version, uint32_t bits, double* lutHelper) {
	for (uint32_t v = 0; v < SRGB_MAX_VALUE; v++) {
		lutHelper[v] = getLinearSRGBChannelValuefromSRGBChannelByte((uint8_t) v);
	}
	
	double Kr = 0.299;
	double Kb = 0.114;
	if (version > 0) {
		Kr = 0.2126;
		Kb = 0.0722;
	}
	double Kg = (1.0 - Kr) - Kb;	
	double CbMult = 0.5 / (1.0 - Kb);
	double CrMult = 0.5 / (1.0 - Kr);
	
	double bitFactor = 255.0;
	if (bits > 0) {
		bitFactor = 1023.0;
	}
	
	uint32_t* xvYCbCr = lutData;
	
	for (uint32_t red = 0; red < SRGB_MAX_VALUE; red++) {
		double R = lutHelper[red];
		double Yr = Kr * R;
		for (uint32_t green = 0; green < SRGB_MAX_VALUE; green++) {
			double G = lutHelper[green];
			double Yrg = (Kg * G) + Yr;
			for (uint32_t blue = 0; blue < SRGB_MAX_VALUE; blue++) {
				double B = lutHelper[blue];
				double Y = (Kb * B) + Yrg;
				
				double Cb = B - Y;
				double Cr = R - Y;
				Cb *= CbMult;
				Cr *= CrMult;
				Cb += 0.5;
				Cr += 0.5;
				
				Y *= bitFactor;
				if (Y > bitFactor) {
					Y = bitFactor;
				}
				else if (Y < 0.0) {
					Y = 0.0;
				}
				
				Cb *= bitFactor;
				if (Cb > bitFactor) {
					Cb = bitFactor;
				}
				else if (Cb < 0.0) {
					Cb = 0.0;
				}
				
				Cr *= bitFactor;
				if (Cr > bitFactor) {
					Cr = bitFactor;
				}
				else if (Cr < 0.0) {
					Cr = 0.0;
				}
				
				uint32_t Yint = roundDouble(Y);
				uint32_t Cbint = roundDouble(Cb);
				uint32_t Crint = roundDouble(Cr);
				
				if (bits == 0) {
					*xvYCbCr = (Yint << 16) | (Cbint << 8) | Crint;
				}
				else {
					*xvYCbCr = (Yint << 20) | (Cbint << 10) | Crint;
				}
				xvYCbCr++;
			}
		}
	}
}

//FFMPEG Conversion for 10-bit
void populateSRGBtoYCbCr709fullLUT(uint32_t* lutData, uint32_t bits) {	
	uint32_t* YCbCr = lutData;
	
	if (bits > 0) {
		for (uint32_t r = 0; r < SRGB_MAX_VALUE; r++) {
			for (uint32_t g = 0; g < SRGB_MAX_VALUE; g++) {
				for (uint32_t b = 0; b < SRGB_MAX_VALUE; b++) {
					int64_t Yint = ((871 * r) + (2929 * g) + (296 * b) + 2048) >> 10;
					if (Yint > 1023) {
						Yint = 1023;
					}
					
					int64_t Cbint = (((-469 * r) + (-1579 * g) + (2048 * b) + 2047) >> 10) + 512;
					int64_t Crint = (((2048 * r) + (-1860 * g) + (-188 * b) + 2047) >> 10) + 512;
					
					*YCbCr = (Yint << 20) | (Cbint << 10) | Crint;
					YCbCr++;
				}
			}
		}
	}
	else {
		for (uint32_t r = 0; r < SRGB_MAX_VALUE; r++) {
			for (uint32_t g = 0; g < SRGB_MAX_VALUE; g++) {
				for (uint32_t b = 0; b < SRGB_MAX_VALUE; b++) {
					int64_t Yint = ((218 * r) + (732 * g) + (74 * b) + 512) >> 10;
					if (Yint > 255) {
						Yint = 255;
					}
					
					int64_t Cbint = (((-117 * r) + (-395 * g) + (512 * b) + 511) >> 10) + 128;
					int64_t Crint = (((512 * r) + (-465 * g) + (-47 * b) + 511) >> 10) + 128;
					
					*YCbCr = (Yint << 16) | (Cbint << 8) | Crint;
					YCbCr++;
				}
			}
		}
	}
	
}

//Program Main Function
int programMain() {
	//consoleControl(CON_NEW_LINE, 0);
	consolePrintLine(25);
	
	//Start-up Desktop Duplication
	consolePrintLine(26);
	size_t shaderSize = shader_size;
	//consolePrintLineWithNumber(24, shaderSize, NUM_FORMAT_UNSIGNED_INTEGER);
	uint32_t* shaderData = (uint32_t*) shader_data;
	//consolePrintLineWithNumber(24, (uint64_t) shaderData[0], NUM_FORMAT_PARTIAL_HEXADECIMAL);
	uint32_t* lutBufferPtr = NULL;
	int error = desktopDuplicationSetup(shaderSize, shaderData, (void**) &lutBufferPtr);
	RETURN_ON_ERROR(error);
	
	populateSRGBtoXVYCbCrLUT(lutBufferPtr, 1, 1);
	
	
	//populateSRGBtoYCbCr709fullLUT(lutBufferPtr, 1);
	
	
	/*
	void* lutMemoryVoid = NULL;
	size_t lutMemorySize = SRGB_MAX_VALUE * sizeof(double);
	error = allocCompatibilityMemory(&lutMemoryVoid, (uint64_t) lutMemorySize, 0);
	RETURN_ON_ERROR(error);
	
	populateSRGBtoXVYCbCrLUT2(lutBufferPtr, 1, 1, (double*) lutMemoryVoid);
	
	error = deallocCompatibilityMemory(&lutMemoryVoid);
	RETURN_ON_ERROR(error);
	//*/
	
	/*
	void* lutMemoryVoid = NULL;
	size_t lutMemorySize = (NUM_SRGB_VALUES * sizeof(uint32_t)) + (SRGB_MAX_VALUE * sizeof(double));
	error = allocCompatibilityMemory(&lutMemoryVoid, (uint64_t) lutMemorySize, 0);
	RETURN_ON_ERROR(error);
	
	uint32_t* lutData = (uint32_t*) lutMemoryVoid;
	double* lutHelper = (double*) (lutMemoryVoid + (NUM_SRGB_VALUES * sizeof(uint32_t)));
	
	//populateSRGBtoXYBlut(lutData, lutHelper);
	
	//uint32_t sRGBvalue = 0x242C37;
	//uint32_t XYBvalue = lutData[sRGBvalue];
	
	//consolePrintLineWithNumber(24, XYBvalue, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	populateSRGBtoXVYCbCrLUT(lutData, 1, 1);
	//consolePrintLineWithNumber(24, lutData[1], NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	error = deallocCompatibilityMemory(&lutMemoryVoid);
	RETURN_ON_ERROR(error);
	//*/
	
	error = desktopDuplicationLoadLUT();
	RETURN_ON_ERROR(error);
	
	//error = desktopDuplicationGetFrame();
	//RETURN_ON_ERROR(error);
	
	/*
	Load Up Output Bitstream File
	void* h265File = NULL;
	error = openFile(&h265File, "bitstream0.h265", 15, IO_FILE_WRITE_NORMAL);
	RETURN_ON_ERROR(error);
	
	void* imgFile = NULL;
	error = openFile(&imgFile, 1, "image0.rgb", 10);
	RETURN_ON_ERROR(error);
	
	
	error = desktopDuplicationTestFrame(imgFile, h265File);
	RETURN_ON_ERROR(error);
	
	
	error = closeFile(&imgFile);
	RETURN_ON_ERROR(error);
	//*/
	
	//*
	void* h265File = NULL;
	error = ioOpenFile(&h265File, "bitstream.h265", 14, IO_FILE_WRITE_ASYNC);
	RETURN_ON_ERROR(error);
	
	consolePrintLine(27);
	consolePrintLine(28);
	consoleBufferFlush();
	consoleWaitForEnter();
	consolePrintLine(29);
	
	uint64_t fps = 60;
	uint64_t recordSeconds = 60;
	
	int ddError = desktopDuplicationStart(fps);
	RETURN_ON_ERROR(ddError);
	consoleBufferFlush();
	
	uint64_t numOfFrames = fps * recordSeconds;
	uint64_t numWrittenFrames = 0;
	uint64_t sleepTotal = 0;
	while (numWrittenFrames < numOfFrames) {
		ddError = desktopDuplicationRun(h265File, &numWrittenFrames);
		if (ddError > 1000) {
			break; //Need to handle the possible errors in the future
		}
		else if (ddError > 1) {
			//compatibilitySleepFast(1);//error-1);
			sleepTotal++;
		}
	}
	//Close (and Save) Output Bitstream File
	error = ioCloseFile(&h265File);
	RETURN_ON_ERROR(error);
	
	desktopDuplicationStop();
	
	if (ddError <= 1000) {
		consolePrintLine(30);
	}
	else {
		consolePrintLine(31);
		RETURN_ON_ERROR(ddError);
	}
	
	//Desktop Duplication Clean
	desktopDuplicationCleanup();
	
	//consoleBufferFlush();
	return 0;
}

