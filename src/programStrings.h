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


//Media Enhanced Program Strings Function Definitions
#ifndef MEDIA_ENHANCED_PROGRAM_STRINGS_H
#define MEDIA_ENHANCED_PROGRAM_STRINGS_H

#include "compatibility.h"	//Includes <stdint.h>

//Seperating string literals from the code makes it easy to translate text to
//other languages while simply refering to the relevant text as line numbers
//The UTF-8 strings can be managed in a line seperated text file which gets
//compiled during the Make process to a string array and linked into the
//final executable under its own initalized data section
//The strings data base pointer can be offset using the index data to find the
//start pointer of a specific text line
//Note that these strings DO NOT have a 0 byte terminating the text
extern uint8_t stringsData[];
extern uint32_t stringsIndices[];

//String Literal Helper Varaibles and Macros:
static char* strs = (char*) stringsData;
static uint32_t* strsOff = stringsIndices;

//inline optional
static void consolePrintDirectLine(uint64_t line) {
	uint64_t numBytes = strsOff[line+1] - strsOff[line] - 1;
	consoleWriteDirectLine(strs + strsOff[line], numBytes);
}

static void consolePrintDirectLineWithNumber(uint64_t line, uint64_t number, uint64_t numFormat) {
	uint64_t numBytes = strsOff[line+1] - strsOff[line] - 1;
	consoleWriteDirectLineWithNumber(strs + strsOff[line], numBytes, number, numFormat);
}


static int consolePrint(uint64_t line, uint64_t consoleModifier) {
	uint64_t numBytes = strsOff[line+1] - strsOff[line] - 1;
	return consoleWrite(strs + strsOff[line], numBytes, consoleModifier);
}

static void consolePrintLine(uint64_t line) {
	uint64_t numBytes = strsOff[line+1] - strsOff[line] - 1;
	consoleWriteLineFast(strs + strsOff[line], numBytes);
}

static int consolePrintWithNumber(uint64_t line, uint64_t number, uint64_t numFormat, uint64_t consoleModifier) {
	uint64_t numBytes = strsOff[line+1] - strsOff[line] - 1;
	return consoleWriteWithNumber(strs + strsOff[line], numBytes, number, numFormat, consoleModifier);
}

static void consolePrintLineWithNumber(uint64_t line, uint64_t number, uint64_t numFormat) {
	uint64_t numBytes = strsOff[line+1] - strsOff[line] - 1;
	consoleWriteLineWithNumberFast(strs + strsOff[line], numBytes, number, numFormat);
}

#endif //MEDIA_ENHANCED_PROGRAM_STRINGS_H

