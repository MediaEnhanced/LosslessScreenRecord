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


//Media Enhanced Program Entry Function Definition for Console Programs
#ifndef MEDIA_ENHANCED_PROGRAM_ENTRY_H
#define MEDIA_ENHANCED_PROGRAM_ENTRY_H

#include "programStrings.h"	//Includes: "compatibility.h" & <stdint.h>

int programMain();

// Program Entry Function
// Expects timeFunctionSetup and consoleSetupMinimum to complete successfully
// Calls the main function and exits under all conditions properly
void programEntry() {
	int error = compatibilitySetup();
	if (error == 0) {
		consolePrintLine(4);
		uint64_t startTime = getCurrentTime();
		
		error = programMain(); //Run the main program
		if (error == 0) {
			uint64_t stopTime = getCurrentTime();
			
			consoleControl(CON_NEW_LINE, 0);
			consolePrintLine(11);
			consolePrint(12, 0); // Display total run time in relevant units
			uint64_t runTime = getDiffTimeMicroseconds(startTime, stopTime);
			if (runTime < 1000) {
				consolePrintWithNumber(13, runTime, NUM_FORMAT_UNSIGNED_INTEGER, CON_FLIP_ORDER_NEW_LINE);
			}
			else {
				runTime = getDiffTimeMilliseconds(startTime, stopTime);
				if (runTime < 1000) {
					consolePrintWithNumber(14, runTime, NUM_FORMAT_UNSIGNED_INTEGER, CON_FLIP_ORDER_NEW_LINE);
				}
				else {
					runTime = getDiffTimeSeconds(startTime, stopTime);
					if (runTime < 60) {
						consolePrintWithNumber(15, runTime, NUM_FORMAT_UNSIGNED_INTEGER, CON_FLIP_ORDER_NEW_LINE);
					}
					else {
						uint64_t runTimeMinutes = runTime / 60;
						runTime %= 60;
						consolePrintWithNumber(16, runTimeMinutes, NUM_FORMAT_UNSIGNED_INTEGER, CON_FLIP_ORDER);
						consoleControl(CON_CURSOR_ADVANCE, 1);
						consolePrintWithNumber(15, runTime, NUM_FORMAT_UNSIGNED_INTEGER, CON_FLIP_ORDER_NEW_LINE);
					}
				}
			}
		}
		else {
			consolePrintLineWithNumber(1, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
			if ((error >= ERROR_TIMER_BAD) && (error <= ERROR_TBD)) {
				compatibilityGetExtraError(&error);
				consolePrintLineWithNumber(2, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
			}
			else if ((error >= ERROR_VULKAN_EXTRA_INFO) && (error <= ERROR_VULKAN_TBD)) {
				vulkanGetError(&error);
				consolePrintLineWithNumber(5, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
			}
			else if ((error >= ERROR_NETWORK_WRONG_STATE) && (error <= ERROR_NETWORK_TBD)) {
				compatibilityGetNetworkError(&error);
				consolePrintLineWithNumber(6, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
			}
			else if ((error >= ERROR_DESKDUPL_CREATE_FACTORY) && (error <= ERROR_DESKDUPL_KEYEDMUTEX_QUERY)) {
				desktopDuplicationGetError(&error);
				consolePrintLineWithNumber(7, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
			}
			else if ((error >= ERROR_CUDA_NO_INIT) && (error <= ERROR_NVENC_TBD)) {
				nvidiaGetError(&error);
				consolePrintLineWithNumber(8, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
			}
		}
		consoleBufferFlush();
	}
	else {
		consolePrintDirectLineWithNumber(1, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		if ((error >= ERROR_TIMER_BAD) && (error <= ERROR_TBD)) {
			compatibilityGetExtraError(&error);
			consolePrintDirectLineWithNumber(2, error, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		}
	}
	
	consolePrintDirectLine(3);
	consoleWaitForEnter();
	
	compatibilityCleanup();
	compatibilityExit(error);
}

#endif //MEDIA_ENHANCED_PROGRAM_ENTRY_H

