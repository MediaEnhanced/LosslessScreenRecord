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


//Media Enhanced OS Compatibility Implementation for Console-Based Programs
//Windows Version that uses UTF-8 for the console but converts to UTF-16
//when neccessary to communicate with the win32 API
//Error Checking is a mixed bag... especially for the non-fast functions
#include "compatibility.h"

// Compatibility State Codes:
#define COMPATIBILITY_STATE_UNDEFINED 0
#define COMPATIBILITY_STATE_BARE 1
#define COMPATIBILITY_STATE_FULL 2
static uint64_t compatibilityState = COMPATIBILITY_STATE_UNDEFINED;

void* memcpyBasic(void* dest, const void* src, size_t count) {
	uint64_t count8 = (uint64_t) (count >> 3);
	
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

//Assumes that computer operates with little-endianess (reasonable assumption)
static uint64_t shortByteSwap(uint64_t value) {
	return ((value & 0xFF00) >> 8) | ((value & 0xFF) << 8);
}

#define UNICODE //defined before any includes, dictates that Windows function defaults will work with Unicode UTF-16(LE) encoded strings
#define _UNICODE //similar definition
#define WIN32_LEAN_AND_MEAN //Excludes several unnecessary includes when using windows.h
#include <windows.h> //Includes win32 functions and helper macros (uses UNICODE define)

#include <dxgi1_6.h> //Needed to get adapters (GPU devices)
#include <d3d11.h>   //Windows DirectX 11: Version 11.1 is needed for Desktop Duplication


#define VK_USE_PLATFORM_WIN32_KHR //Define Vulkan To Use 
#include "include/vulkan/vulkan.h"

#include "include/cudart/cuda.h"
#include "include/nvEncodeAPI.h"

#include <winsock2.h> //Windows Networking Header
#include <ws2tcpip.h> //Needed for additional windows networking functions and definitions (IPv6)
#include <mswsock.h> //Needed for (better) recv and send socket operations and some socket option defines

#include <Commdlg.h> //Includes win32 command dialog "pop-up" boxes used for choosing an input file

//#include <tchar.h> //needed for _tcscat_s
//#include <shobjidl.h>

#define RETURN_ON_ERROR(error) ({if (error != 0) { return error; }})

//Basic Local version of the wcscpy CRT library function
static WCHAR* wcscpyBasic(WCHAR* dest, const WCHAR* src) {
	WCHAR* destCpy = dest;
	WCHAR value;
	do {
		value = *src;
		*destCpy = value;
		src++;
		destCpy++;
	} while(value != 0);
	return dest;
}

static HANDLE consoleOut = NULL;
static HANDLE consoleIn = NULL;
static UINT originalConsoleOutCP = 0;
static UINT originalConsoleCP = 0;
static DWORD defaultPageSize = 0; //Expecting to be set to 4KB (1024 * 4 bytes)
static uint64_t timeCounterFrequency = 0;
static uint64_t secondDivider = 0;
static uint64_t millisecondDivider = 0;
static uint64_t microsecondDivider = 0;

static void startBareCompatibility() { //Does no error checking... assumes that the bare minimum will always be completed successfully
	if (compatibilityState >= COMPATIBILITY_STATE_BARE) {
		return;
	}
	
	consoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	consoleIn = GetStdHandle(STD_INPUT_HANDLE);
	
	originalConsoleOutCP = GetConsoleOutputCP();
	originalConsoleCP = GetConsoleCP();
	
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	
	SYSTEM_INFO sSysInfo;
	GetSystemInfo(&sSysInfo);
	defaultPageSize = sSysInfo.dwPageSize;
	
	LARGE_INTEGER performanceCounter;
	BOOL result = QueryPerformanceFrequency(&performanceCounter);
	if (result != 0) {
		timeCounterFrequency = (uint64_t) performanceCounter.QuadPart;
	}
	secondDivider = timeCounterFrequency / SECOND_FREQUENCY;
	millisecondDivider = timeCounterFrequency / MILLISECOND_FREQUENCY;
	microsecondDivider = timeCounterFrequency / MICROSECOND_FREQUENCY;
	
	compatibilityState = COMPATIBILITY_STATE_BARE;
}

void consoleWriteLineDirect(char* strUTF8, uint64_t strBytes) {
	if (compatibilityState < COMPATIBILITY_STATE_BARE) {
		return;
	}
	
	WriteConsoleA(consoleOut, strUTF8, (DWORD) strBytes, NULL, NULL);
	char newLine = '\n';
	WriteConsoleA(consoleOut, &newLine, 1, NULL, NULL);
}

void consoleWriteLineWithNumberDirect(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat) {
	if (compatibilityState < COMPATIBILITY_STATE_BARE) {
		return;
	}
	
	WriteConsoleA(consoleOut, strUTF8, (DWORD) strBytes, NULL, NULL);
	
	char strBuffer[32];
	DWORD numCharacters = 0;
	char newLine = '\n';
	
	if (numberFormat == NUM_FORMAT_FULL_HEXADECIMAL) {
		numToFHexStr(number, &strBuffer[2]);
		strBuffer[0] = '0';
		strBuffer[1] = 'x';
		strBuffer[18] = newLine;
		numCharacters = 19;
	}
	else if (numberFormat == NUM_FORMAT_PARTIAL_HEXADECIMAL) {
		uint64_t digits = numToPHexStr(number, &strBuffer[2]);
		strBuffer[0] = '0';
		strBuffer[1] = 'x';
		strBuffer[digits + 2] = newLine;
		numCharacters = ((DWORD) digits) + 3;
	}
	else if (numberFormat == NUM_FORMAT_UNSIGNED_INTEGER) {
		uint64_t digits = numToUDecStr(strBuffer, number);
		strBuffer[digits] = newLine;
		numCharacters = ((DWORD) digits) + 1;
	}
	
	WriteConsoleA(consoleOut, strBuffer, numCharacters, NULL, NULL);
}

void getCompatibilityExtraError(int* error) {
	if (compatibilityState < COMPATIBILITY_STATE_BARE) {
		return;
	}
	
	*error = (int) GetLastError();
}

void consoleWaitForEnter() {
	if (compatibilityState < COMPATIBILITY_STATE_BARE) {
		return;
	}
	
	//If there is an enter event already "waiting" clear it first
	BOOL res = FlushConsoleInputBuffer(consoleIn);
	if (res == 0) {
		return;
	}
	
	INPUT_RECORD inRecord;
	DWORD recordsRead = 0;
	
	uint64_t enterPressed = 0;
	while (enterPressed == 0) {
		ReadConsoleInputA(consoleIn, &inRecord, 1, &recordsRead);
		if (inRecord.EventType == KEY_EVENT) {
			if (inRecord.Event.KeyEvent.bKeyDown == FALSE) {
				if (inRecord.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
					enterPressed = 1;
				}
			}
		}
	}
}


uint64_t getCurrentTime() {
	LARGE_INTEGER performanceCounter;
	QueryPerformanceCounter(&performanceCounter);
	return (uint64_t) performanceCounter.QuadPart;
}

void incDiffTime(uint64_t* countTime, uint64_t startTime, uint64_t endTime) {
	*countTime += (endTime - startTime);
}

uint64_t getDiffTimeMicroseconds(uint64_t startTime, uint64_t endTime) {
	return ((endTime - startTime) / microsecondDivider);
}

uint64_t getDiffTimeMilliseconds(uint64_t startTime, uint64_t endTime) {
	return ((endTime - startTime) / millisecondDivider);
}

uint64_t getDiffTimeSeconds(uint64_t startTime, uint64_t endTime) {
	return ((endTime - startTime) / secondDivider);
}

uint64_t getEndTimeFromMicroDiff(uint64_t startTime, uint64_t usDiff) {
	return (startTime + (usDiff * microsecondDivider));
}

uint64_t getEndTimeFromMilliDiff(uint64_t startTime, uint64_t msDiff) {
	return (startTime + (msDiff * millisecondDivider));
}


const WCHAR desiredConsoleFont[] = L"Courier New";
static CONSOLE_FONT_INFOEX originalConsoleFont = {0};
static COORD originalConsoleScreenBufferSize = {0, 0};
static SMALL_RECT originalConsoleScreenBufferCoordinates = {0, 0, 0, 0};
static DWORD originalConsoleOutMode = 0;
static DWORD originalConsoleMode = 0;

static char* cmdCurrentArgument = NULL;
static uint64_t largePageSupport = 0;

int startFullCompatibility() { //add more error checking...
	if (compatibilityState >= COMPATIBILITY_STATE_FULL) {
		return ERROR_ALREADY_STARTED;
	}
	
	startBareCompatibility(); //Check some set values from this and throw errors if neccessary
	if (defaultPageSize != 4096) {
		return ERROR_INVALID_PAGE_SIZE;
	}
	
	originalConsoleFont.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	BOOL res = GetCurrentConsoleFontEx(consoleOut, FALSE, &originalConsoleFont);
	if (res == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	
	CONSOLE_SCREEN_BUFFER_INFO conInfo;
	res = GetConsoleScreenBufferInfo(consoleOut, &conInfo);
	if (res == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	originalConsoleScreenBufferSize.X = conInfo.dwSize.X;
	originalConsoleScreenBufferSize.Y = conInfo.dwSize.Y;
	originalConsoleScreenBufferCoordinates.Left = conInfo.srWindow.Left;
	originalConsoleScreenBufferCoordinates.Top = conInfo.srWindow.Top;
	originalConsoleScreenBufferCoordinates.Right = conInfo.srWindow.Right;
	originalConsoleScreenBufferCoordinates.Bottom = conInfo.srWindow.Bottom;
	
	GetConsoleMode(consoleOut, &originalConsoleOutMode);
	GetConsoleMode(consoleIn, &originalConsoleMode);
	
	
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(cfi);
	cfi.nFont = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = 20;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;
	wcscpyBasic(cfi.FaceName, desiredConsoleFont);
	res = SetCurrentConsoleFontEx(consoleOut, FALSE, &cfi);
	if (res == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	
	//What effect does this have on the background console "buffer" memory?
	SMALL_RECT sRect = {0, 0, 1, 1};
	res = SetConsoleWindowInfo(consoleOut, TRUE, &sRect);
	if (res == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	
	COORD cSize = {80, 100};
	res = SetConsoleScreenBufferSize(consoleOut, cSize);
	if (res == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	
	sRect.Right = 79;
	sRect.Bottom = 20;
	res = SetConsoleWindowInfo(consoleOut, TRUE, &sRect);
	if (res == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	
	DWORD consoleOutMode = originalConsoleOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	DWORD consoleMode = ENABLE_PROCESSED_INPUT;//originalConsoleMode & (~ENABLE_ECHO_INPUT);
	
	SetConsoleMode(consoleOut, consoleOutMode);
	SetConsoleMode(consoleIn, consoleMode);
	
	cmdCurrentArgument = (char*) GetCommandLineA();
	
	largePageSupport = 0; //To be implemented using: https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support
	
	compatibilityState = COMPATIBILITY_STATE_FULL;
	
	return 0;
}


// Memory Operations:
int getCompatibilityNewMemoryPage(void** memoryPtr, uint64_t* memoryBytes) {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	*memoryBytes = (uint64_t) defaultPageSize;
	*memoryPtr = VirtualAlloc(NULL, (SIZE_T) defaultPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (*memoryPtr == NULL) {
		return ERROR_GET_EXTRA_INFO;
	}
	return 0;
}

int tryCompatibilitySetupLargeMemoryPages() {
	/*
	//HANDLE tokenHandle GetCurrentProcessToken();
	//BOOL adjustTokenResult = AdjustTokenPrivileges(tokenHandle, FALSE, 
	
	HANDLE hToken = NULL;
	TOKEN_PRIVILEGES tp;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken)) {
		//printf("OpenProcessToken #2 failed. GetLastError returned: %ld\n", GetLastError());
		return -20;
	}

	tp.PrivilegeCount = 1;
	tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &tp.Privileges[0].Luid)) {
		//printf("LookupPrivilegeValue failed. GetLastError returned: %ld\n", GetLastError());
		return -21;
	}

	BOOL adjResult = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
	DWORD lastError = GetLastError();

	if (!adjResult || (lastError != ERROR_SUCCESS)) {
		//printf("AdjustTokenPrivileges failed. GetLastError returned: %ld\n", lastError);
		return -22;
	}
	
	CloseHandle(hToken);
	
	largePageSupport = 1;
	//*/
	return 0;
}

int allocCompatibilityMemory(void** memoryPtr, uint64_t memoryBytes, uint64_t largePage) {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	DWORD allocationType = MEM_COMMIT | MEM_RESERVE;
	if (largePage > 0) { // do checks
		SIZE_T largePageMinBytes = GetLargePageMinimum();
		if ((memoryBytes % largePageMinBytes) != 0) {
			return ERROR_LARGE_PAGE_PROBLEM;
		}
		if (largePageSupport == 0) {
			return ERROR_LARGE_PAGE_PROBLEM;
		}
		allocationType |= MEM_LARGE_PAGES;
	}
	*memoryPtr = VirtualAlloc(NULL, (SIZE_T) memoryBytes, allocationType, PAGE_READWRITE);
	if (*memoryPtr == NULL) {
		return ERROR_GET_EXTRA_INFO;
	}
	return 0;
}

int deallocCompatibilityMemory(void** memoryPtr) {
	BOOL freeResult = VirtualFree(*memoryPtr, 0, MEM_RELEASE);
	if (freeResult == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	*memoryPtr = NULL;
	return 0;
}


// Console (With Buffer) State and Functions:
static uint64_t consoleState = CONSOLE_STATE_UNDEFINED;
static void* consoleBuffer = NULL;
static char* consoleBufferPos = NULL;
static uint64_t consoleByteSize = 0;
static uint64_t consoleBytesRemaining = 0;
static uint64_t consoleLastFlushTime = 0;

int consoleBufferSetup() {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	if (consoleState >= CONSOLE_STATE_SETUP) {
		return ERROR_CON_BUFFER_ALREADY_DEFINED;
	}
	
	int error = getCompatibilityNewMemoryPage(&consoleBuffer, &consoleByteSize);
	if (error != 0) {
		return error;
	}
	
	consoleBufferPos = (char*) consoleBuffer;
	consoleBytesRemaining = consoleByteSize;
	consoleLastFlushTime = getCurrentTime();
	
	consoleState = CONSOLE_STATE_SETUP;
	return 0;
}

int consoleBufferFlush() {
	if (consoleState < CONSOLE_STATE_SETUP) {
		return ERROR_CON_BUFFER_UNDEFINED;
	}
	
	DWORD bytesToWrite = (DWORD) (consoleByteSize - consoleBytesRemaining);
	if (bytesToWrite > 0) {
		DWORD bytesWritten = 0;
		BOOL res = WriteConsoleA(consoleOut, consoleBuffer, (DWORD) bytesToWrite, &bytesWritten, NULL);
		if (res == 0) {
			return ERROR_GET_EXTRA_INFO;
		}
		if (bytesWritten != bytesToWrite) {
			return ERROR_INCORRECT_WRITE_SIZE;
		}
		
		consoleBufferPos = (char*) consoleBuffer;
		consoleBytesRemaining = consoleByteSize;
	}
	
	consoleLastFlushTime = getCurrentTime();
	
	return 0;
}

int consoleBufferTerminate(uint64_t flush) {
	if (consoleState < CONSOLE_STATE_SETUP) {
		return 0;
	}
	
	if (flush != 0) {
		DWORD bytesToWrite = (DWORD) (consoleByteSize - consoleBytesRemaining);
		DWORD bytesWritten = 0;
		BOOL res = WriteConsoleA(consoleOut, consoleBuffer, (DWORD) bytesToWrite, &bytesWritten, NULL);
		if (res == 0) {
			//return ERROR_GET_EXTRA_INFO;
		}
		if (bytesWritten != bytesToWrite) {
			//return ERROR_INCORRECT_WRITE_SIZE;
		}
	}
	
	int error = deallocCompatibilityMemory(&consoleBuffer);
	if (error != 0) {
		//return error;
	}
	consoleBuffer = NULL;
	consoleBufferPos = NULL;
	consoleByteSize = 0;
	consoleBytesRemaining = 0;
	
	consoleState = CONSOLE_STATE_UNDEFINED;
	return 0;
}

//Throws no error on invalid extra info
int consoleWrite(char* strUTF8, uint64_t strBytes, uint64_t conExtraInfo) {
	if (consoleState < CONSOLE_STATE_SETUP) {
		return ERROR_CON_BUFFER_UNDEFINED;
	}
	
	if (strUTF8 == NULL) {
		return ERROR_NULL_POINTER;
	}
	
	if (strBytes > consoleBytesRemaining) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	//Write Direct if strBytes is too much for buffer
	if (strBytes > consoleByteSize) {		
		DWORD bytesWritten = 0;
		BOOL res = WriteConsoleA(consoleOut, consoleBuffer, (DWORD) strBytes, &bytesWritten, NULL);
		if (res == 0) {
			return ERROR_GET_EXTRA_INFO;
		}
		if (bytesWritten != ((DWORD) strBytes)) {
			return ERROR_INCORRECT_WRITE_SIZE;
		}
	}
	else {
		memcpyBasic(consoleBufferPos, strUTF8, strBytes);
		consoleBufferPos += strBytes;
		consoleBytesRemaining -= strBytes;
	}
	
	if (consoleBytesRemaining < 64) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	if (conExtraInfo == CON_NEW_LINE) {
		char newLine = '\n';
		*consoleBufferPos = newLine;
		consoleBufferPos++;
		consoleBytesRemaining--;
	}
	
	uint64_t currentTime = getCurrentTime();
	uint64_t diffTimeMS = getDiffTimeMilliseconds(consoleLastFlushTime, currentTime);
	
	if ((consoleBytesRemaining < 256) || (diffTimeMS > CON_FLUSH_MS)) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	return 0;
}

void consoleWriteLineFast(char* strUTF8, uint64_t strBytes) {
	//if (consoleState < CONSOLE_STATE_SETUP) {
	//	return;
	//}
		
	if ((strBytes+1) > consoleBytesRemaining) {
		consoleBufferFlush();
	}
	
	memcpyBasic(consoleBufferPos, strUTF8, strBytes);
	consoleBufferPos += strBytes;
	consoleBytesRemaining -= strBytes;
	
	char newLine = '\n';
	*consoleBufferPos = newLine;
	consoleBufferPos++;
	consoleBytesRemaining--;
	
	uint64_t currentTime = getCurrentTime();
	uint64_t diffTimeMS = getDiffTimeMilliseconds(consoleLastFlushTime, currentTime);
	
	if (diffTimeMS > CON_FLUSH_MS) {
		consoleBufferFlush();
	}
}

void consoleWriteLineSlow(char* strUTF8) {
	if (consoleState < CONSOLE_STATE_SETUP) {
		return;
	}
	
	while (*strUTF8 != 0) {
		*consoleBufferPos = *strUTF8;
		consoleBufferPos++;
		consoleBytesRemaining--;
		strUTF8++;
	}
	
	char newLine = '\n';
	*consoleBufferPos = newLine;
	consoleBufferPos++;
	consoleBytesRemaining--;
	
	uint64_t currentTime = getCurrentTime();
	uint64_t diffTimeMS = getDiffTimeMilliseconds(consoleLastFlushTime, currentTime);
	
	if (diffTimeMS > CON_FLUSH_MS) {
		consoleBufferFlush();
	}
}

int consoleWriteWithNumber(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat, uint64_t conExtraInfo) {
	if (consoleState < CONSOLE_STATE_SETUP) {
		return ERROR_CON_BUFFER_UNDEFINED;
	}
	
	if (strUTF8 == NULL) {
		return ERROR_NULL_POINTER;
	}
	
	if ((conExtraInfo == CON_FLIP_ORDER) || (conExtraInfo == CON_FLIP_ORDER_NEW_LINE)) {
		if (consoleBytesRemaining < 64) {
			int error = consoleBufferFlush();
			if (error != 0) {
				return error;
			}
		}
		
		if (numberFormat == NUM_FORMAT_FULL_HEXADECIMAL) {
			consoleBufferPos[0] = '0';
			consoleBufferPos[1] = 'x';
			consoleBufferPos += 2;
			numToFHexStr(number, consoleBufferPos);
			consoleBufferPos += 16;
			consoleBytesRemaining -= 18;
		}
		else if (numberFormat == NUM_FORMAT_PARTIAL_HEXADECIMAL) {
			consoleBufferPos[0] = '0';
			consoleBufferPos[1] = 'x';
			consoleBufferPos += 2;
			uint64_t numBytes = numToPHexStr(number, consoleBufferPos);
			consoleBufferPos += numBytes;
			numBytes += 2;
			consoleBytesRemaining -= numBytes;
		}
		else if (numberFormat == NUM_FORMAT_UNSIGNED_INTEGER) {
			uint64_t numBytes = numToUDecStr(consoleBufferPos, number);
			consoleBufferPos += numBytes;
			consoleBytesRemaining -= numBytes;
		}
		
		if ((strBytes+1) > consoleBytesRemaining) {
			int error = consoleBufferFlush();
			if (error != 0) {
				return error;
			}
		}
		
		//Write Direct if strBytes is too much for buffer
		if (strBytes > consoleByteSize) {		
			DWORD bytesWritten = 0;
			BOOL res = WriteConsoleA(consoleOut, consoleBuffer, (DWORD) strBytes, &bytesWritten, NULL);
			if (res == 0) {
				return ERROR_GET_EXTRA_INFO;
			}
			if (bytesWritten != ((DWORD) strBytes)) {
				return ERROR_INCORRECT_WRITE_SIZE;
			}
		}
		else {
			memcpyBasic(consoleBufferPos, strUTF8, strBytes);
			consoleBufferPos += strBytes;
			consoleBytesRemaining -= strBytes;
		}
		
		if (conExtraInfo == CON_FLIP_ORDER_NEW_LINE) {
			char newLine = '\n';
			*consoleBufferPos = newLine;
			consoleBufferPos++;
			consoleBytesRemaining--;
		}
	}
	else {
		if (strBytes > consoleBytesRemaining) {
			int error = consoleBufferFlush();
			if (error != 0) {
				return error;
			}
		}
		
		//Write Direct if strBytes is too much for buffer
		if (strBytes > consoleByteSize) {		
			DWORD bytesWritten = 0;
			BOOL res = WriteConsoleA(consoleOut, consoleBuffer, (DWORD) strBytes, &bytesWritten, NULL);
			if (res == 0) {
				return ERROR_GET_EXTRA_INFO;
			}
			if (bytesWritten != ((DWORD) strBytes)) {
				return ERROR_INCORRECT_WRITE_SIZE;
			}
		}
		else {
			memcpyBasic(consoleBufferPos, strUTF8, strBytes);
			consoleBufferPos += strBytes;
			consoleBytesRemaining -= strBytes;
		}
		
		if (consoleBytesRemaining < 64) {
			int error = consoleBufferFlush();
			if (error != 0) {
				return error;
			}
		}
		
		if (numberFormat == NUM_FORMAT_FULL_HEXADECIMAL) {
			consoleBufferPos[0] = '0';
			consoleBufferPos[1] = 'x';
			consoleBufferPos += 2;
			numToFHexStr(number, consoleBufferPos);
			consoleBufferPos += 16;
			consoleBytesRemaining -= 18;
		}
		else if (numberFormat == NUM_FORMAT_PARTIAL_HEXADECIMAL) {
			consoleBufferPos[0] = '0';
			consoleBufferPos[1] = 'x';
			consoleBufferPos += 2;
			uint64_t numBytes = numToPHexStr(number, consoleBufferPos);
			consoleBufferPos += numBytes;
			numBytes += 2;
			consoleBytesRemaining -= numBytes;
		}
		else if (numberFormat == NUM_FORMAT_UNSIGNED_INTEGER) {
			uint64_t numBytes = numToUDecStr(consoleBufferPos, number);
			consoleBufferPos += numBytes;
			consoleBytesRemaining -= numBytes;
		}
		
		char newLine = '\n';
		*consoleBufferPos = newLine;
		consoleBufferPos++;
		consoleBytesRemaining--;
	}
	
	uint64_t currentTime = getCurrentTime();
	uint64_t diffTimeMS = getDiffTimeMilliseconds(consoleLastFlushTime, currentTime);
	
	if ((consoleBytesRemaining < 256) || (diffTimeMS > CON_FLUSH_MS)) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	return 0;
}

void consoleWriteLineWithNumberFast(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat) {
	//if (consoleState < CONSOLE_STATE_SETUP) {
	//	return;
	//}
	
	if (strBytes > consoleBytesRemaining) {
		consoleBufferFlush();
	}
	
	memcpyBasic(consoleBufferPos, strUTF8, strBytes);
	consoleBufferPos += strBytes;
	consoleBytesRemaining -= strBytes;
	
	if (consoleBytesRemaining < 64) {
		consoleBufferFlush();
	}
	
	if (numberFormat == NUM_FORMAT_FULL_HEXADECIMAL) {
		consoleBufferPos[0] = '0';
		consoleBufferPos[1] = 'x';
		consoleBufferPos += 2;
		numToFHexStr(number, consoleBufferPos);
		consoleBufferPos += 16;
		consoleBytesRemaining -= 18;
	}
	else if (numberFormat == NUM_FORMAT_PARTIAL_HEXADECIMAL) {
		consoleBufferPos[0] = '0';
		consoleBufferPos[1] = 'x';
		consoleBufferPos += 2;
		uint64_t numBytes = numToPHexStr(number, consoleBufferPos);
		consoleBufferPos += numBytes;
		numBytes += 2;
		consoleBytesRemaining -= numBytes;
	}
	else if (numberFormat == NUM_FORMAT_UNSIGNED_INTEGER) {
		uint64_t numBytes = numToUDecStr(consoleBufferPos, number);
		consoleBufferPos += numBytes;
		consoleBytesRemaining -= numBytes;
	}
	
	char newLine = '\n';
	*consoleBufferPos = newLine;
	consoleBufferPos++;
	consoleBytesRemaining--;
	
	uint64_t currentTime = getCurrentTime();
	uint64_t diffTimeMS = getDiffTimeMilliseconds(consoleLastFlushTime, currentTime);
	
	if (diffTimeMS > CON_FLUSH_MS) {
		consoleBufferFlush();
	}
}

int consoleControl(uint64_t conInstruction, uint64_t conExtraValue) {
	if (consoleState < CONSOLE_STATE_SETUP) {
		return ERROR_CON_BUFFER_UNDEFINED;
	}
	
	char newLine = '\n';
	char escape = 0x1B;
	char leftBraket = '['; //0x5B
	//char semicolon = ';';
	//char question = '?';
	//char space = ' ';
	
	
	if (conInstruction == CON_NEW_LINE) {
		*consoleBufferPos = newLine;
		consoleBufferPos++;
		consoleBytesRemaining--;
	}
	else if (conInstruction == CON_CURSOR_ADVANCE) {
		consoleBufferPos[0] = escape;
		consoleBufferPos[1] = leftBraket;
		consoleBufferPos += 2;
		uint64_t digits = shortToDecStr(consoleBufferPos, conExtraValue);
		consoleBufferPos += digits;
		*consoleBufferPos = 'C';
		consoleBufferPos++;
		digits += 3;
		consoleBytesRemaining -= digits;
	}
	
	uint64_t currentTime = getCurrentTime();
	uint64_t diffTimeMS = getDiffTimeMilliseconds(consoleLastFlushTime, currentTime);
	
	if ((consoleBytesRemaining < 256) || (diffTimeMS > CON_FLUSH_MS)) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	return 0;
}


int getCompatibilityArgument(uint64_t argumentNumber, char** argumentUTF8, uint64_t* argumentByteLength) {
	LPSTR commandLineStr = GetCommandLineA();
	char* charIterator = (char*) commandLineStr;
	*argumentUTF8 = charIterator;
	uint64_t byteLength = 0;
	uint64_t argumentCounter = 0;
	while (*charIterator != 0) { // NULL character
		if (*charIterator == 32) { // SPACE (' ') character
			if (byteLength != 0) {
				if (argumentCounter < argumentNumber) {
					charIterator++;
					*argumentUTF8 = charIterator;
					byteLength = 0;
					argumentCounter++;
				}
				else {
					*charIterator = 0;
				}
			}
			else {
				charIterator++;
				*argumentUTF8 = charIterator;
			}
		}
		else if (*charIterator == 34) { // " character
			while (*charIterator != 34) { //Not double check looking for NULL character
				byteLength++;
				charIterator++;
			}
			byteLength++;
			charIterator++;
		}
		else if (*charIterator == 39) { // ' character
			while (*charIterator != 39) { //Not double check looking for NULL character
				byteLength++;
				charIterator++;
			}
			byteLength++;
			charIterator++;
		}
		else {
			byteLength++;
			charIterator++;
		}
	}
	*argumentByteLength = byteLength;
	if (argumentCounter < argumentNumber) {
			return ERROR_ARGUMENT_DNE;
	}
	return 0;
}

int getCompatibilityNextArgument(char** argumentUTF8, uint64_t* argumentByteLength) {
	if (cmdCurrentArgument == 0) {
			return ERROR_ARGUMENT_DNE;
	}
	char* charIterator = cmdCurrentArgument;
	*argumentUTF8 = charIterator;
	uint64_t byteLength = 0;
	while (*charIterator != 0) { // NULL character
		if (*charIterator == 32) { // SPACE (' ') character
			if (byteLength != 0) {
				*argumentByteLength = byteLength;
				charIterator++;
				cmdCurrentArgument = charIterator;
				return 0;
			}
			charIterator++;
			*argumentUTF8 = charIterator;
		}
		else if (*charIterator == 34) { // " character
			while (*charIterator != 34) { //Not double check looking for NULL character
				byteLength++;
				charIterator++;
			}
		}
		else if (*charIterator == 39) { // ' character
			while (*charIterator != 39) { //Not double check looking for NULL character
				byteLength++;
				charIterator++;
			}
		}
		byteLength++;
		charIterator++;
	}
	*argumentByteLength = byteLength;
	cmdCurrentArgument = 0;
	return 0;
}



int compatibilitySleep(uint64_t milliseconds) {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	consoleBufferFlush();
	DWORD res = SleepEx((DWORD) milliseconds, TRUE); //True to return early due to callback functions
	if (res == WAIT_IO_COMPLETION) {
		return INFO_CALLBACK_FUNCTION_COMPLETION;
	}
	
	return 0;
}

void compatibilitySleepFast(uint64_t milliseconds) {
	SleepEx(milliseconds, FALSE);
}

int getCompatibilityTimestamp(uint64_t* timestamp) {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	uint64_t nanoseconds100 = 0;
	FILETIME* sysTimeUTC = (FILETIME*) &nanoseconds100;
	
	GetSystemTimePreciseAsFileTime(sysTimeUTC);
	
	uint64_t seconds = nanoseconds100 / 10000000;
	uint64_t secondFraction = nanoseconds100 - (seconds * 10000000);
	
	seconds -= 9435484800; //Seconds Between Jan 1st 1601 and Jan 1st 1900
	seconds <<= 32;
	
	secondFraction <<= 32;
	secondFraction /= 10000000;
	secondFraction &= 0xFFFFFFFF;
	
	*timestamp = seconds | secondFraction;
	
	return 0;
}

int getCompatibilityMilliseconds(uint64_t* milliseconds) {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	SYSTEMTIME sysTimeUTC;
	GetSystemTime(&sysTimeUTC);
	
	*milliseconds = sysTimeUTC.wMilliseconds;
	
	return 0;
}


static void* memPageBuffer = NULL;

int setCompatibilityMemoryPageBuffer() {
	uint64_t memoryBytes = 0;
	return getCompatibilityNewMemoryPage(&memPageBuffer, &memoryBytes);
}

int openFile(void** filePtr, uint64_t flags, char* filePathUTF8, int filePathBytes) {
	if (memPageBuffer == NULL) {
		return ERROR_MEM_PAGE_BUFFER_UNDEFINED;
	}
	int characters = MultiByteToWideChar(CP_UTF8, 0, filePathUTF8, filePathBytes, NULL, 0);
	if (characters >= (defaultPageSize>>1)) {
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	LPWSTR filePathUTF16 = (LPWSTR) memPageBuffer;
	int result = MultiByteToWideChar(CP_UTF8, 0, filePathUTF8, filePathBytes, filePathUTF16, characters);
	if (result == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	filePathUTF16[result] = 0;
	
	HANDLE fileHandle = NULL;
	if (flags == 0) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else if (flags == 1) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else if (flags == 2) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	}
	else {
		return ERROR_TBD;
	}
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return ERROR_GET_EXTRA_INFO;
	}
	
	*filePtr = fileHandle;
	return 0;
}

int closeFile(void** filePtr) {
	BOOL result = CloseHandle((HANDLE) *filePtr);
	if (result == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	*filePtr = NULL;
	return 0;
}

int getFileSize(void** filePtr, uint64_t* fileSizeBytes) {
	LARGE_INTEGER fileSizeEx;
	BOOL result = GetFileSizeEx((HANDLE) *filePtr, &fileSizeEx);
	if (result == 0) {
		return ERROR_GET_EXTRA_INFO;
	}
	*fileSizeBytes = (uint64_t) fileSizeEx.QuadPart;
	return 0;
}

int readFile(void** filePtr, void* dataPtr, uint32_t numBytes) {
	DWORD readBytes = 0;
	BOOL result = ReadFile((HANDLE) *filePtr, dataPtr, numBytes, &readBytes, NULL);
	if (result == 0) { //readability
		return ERROR_GET_EXTRA_INFO;
	}
	if (readBytes != numBytes) {
		return ERROR_INCORRECT_READ_SIZE;
	}
	return 0;
}

int writeFile(void* filePtr, void* dataPtr, uint32_t numBytes) {
	DWORD writtenBytes = 0;
	BOOL result = WriteFile((HANDLE) filePtr, dataPtr, numBytes, &writtenBytes, NULL);
	if (result == 0) { //writeability
		return ERROR_GET_EXTRA_INFO;
	}
	if (writtenBytes != numBytes) {
		return ERROR_INCORRECT_WRITE_SIZE;
	}
	return 0;
}

int selectAndOpenFile(void** filePtr, uint64_t flags, char* filePathUTF8) {
	//IFileDialog pfd = NULL;
	//IFileDialogEvents *pfde = NULL;
	//HRESULT hrRes = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
	
	
	
	/*
	TCHAR fileNameStr[200] = INPUT_FILE_LOCATION;
	
	printf("Select an input audio wave (.wav) file\n");
	OPENFILENAME openFileInfo = {0};
	openFileInfo.lStructSize = sizeof(OPENFILENAME);
	openFileInfo.hwndOwner = NULL;
	//openFileInfo.hInstance = NULL;
	openFileInfo.lpstrFilter = NULL; //String File Name Filter
	openFileInfo.lpstrCustomFilter = NULL; //Not Needed
	openFileInfo.nMaxCustFilter = 0; //Ignored when lpstrCustomFilter is NULL
	openFileInfo.nFilterIndex = 0;
	openFileInfo.lpstrFile = fileNameStr;
	openFileInfo.nMaxFile = 200;
	openFileInfo.lpstrFileTitle = NULL;
	openFileInfo.nMaxFileTitle = 0;
	openFileInfo.lpstrInitialDir = NULL;
	openFileInfo.lpstrTitle = L"Open Wav To Convert";
	openFileInfo.Flags = 0; //Might need to look through these
	//openFileInfo.nFileOffset = 0;
	//openFileInfo.nFileExtension = 0;
	//openFileInfo.lpstrDefExt = 0;
	//More parameters...
	
	BOOL openFileResult = 1;
	openFileResult = GetOpenFileName(&openFileInfo);
	if (openFileResult == 0) {
		printf("Conversion Canceled\n");
		return 3;
	}
	
	LONGLONG fileChosenTime = 0;
	//Win32PerformanceGetCount(&fileChosenTime);
	
	//Open Input and Output Audio Files (Synchronous Operations For Now)
	HANDLE inFileHandle = CreateFile(fileNameStr, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (inFileHandle == NULL) {
		printf("Problem Opening Input File\n");
		return 4;
	}
	
	printf("%ls -> ", fileNameStr);
	
	fileNameStr[openFileInfo.nFileExtension-1] = 0;
	_tcscat_s(fileNameStr, 200, OUTPUT_FILE_APPEND);
	
	printf("%ls\n\n", fileNameStr);
	
	HANDLE outFileHandle = CreateFile(fileNameStr, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); 
	if (outFileHandle == NULL) {
		printf("Problem Creating Output File\n");
		return 5;
	}
	//*/
	
	/*
	int characters = MultiByteToWideChar(CP_UTF8, 0, filePathUTF8, -1, NULL, 0);
	LPWSTR filePathUTF16 = malloc(characters * sizeof(WCHAR));
	int result = MultiByteToWideChar(CP_UTF8, 0, filePathUTF8, -1, filePathUTF16, characters);
	if (result == 0) {
		return (int) GetLastError();
	}
	HANDLE fileHandle = NULL;
	if (flags == 0) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else {
		fileHandle = CreateFile(filePathUTF16, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	free(filePathUTF16);
	*filePtr = fileHandle;
	//*/
	return 0;
}

void readFileFast(void** filePtr, void* dataPtr, uint32_t numBytes) {
	ReadFile((HANDLE) *filePtr, dataPtr, numBytes, NULL, NULL);
}

void writeFileFast(void** filePtr, void* dataPtr, uint32_t numBytes) {
	WriteFile((HANDLE) *filePtr, dataPtr, numBytes, NULL, NULL);
}


//#define RETURN_ON_ERROR_EXTRA(error, extra,) ({if (error != 0) { return error; }})

#define DESKDUPL_STATE_UNDEFINED 0
#define DESKDUPL_STATE_LUT_LOAD 1
#define DESKDUPL_STATE_STARTED 2
#define DESKDUPL_STATE_RUNNING 3
#define DESKDUPL_STATE_NEXT_FRAME 4



static uint64_t desktopDuplicationState = DESKDUPL_STATE_UNDEFINED;

static int desktopDuplicationExtraInfo = S_OK;

static IDXGIAdapter* desktopDuplicationAdapter = NULL; //Adapter Interface Pointer
static LUID desktopDuplicationAdapterID = {0, 0};
static ID3D11Device* desktopDuplicationDevice = NULL; //Device Interface Pointer
static IDXGIOutputDuplication* desktopDuplicationPtr = NULL;
static uint32_t desktopDuplicationWidth = 0;
static uint32_t desktopDuplicationHeight = 0;
static HANDLE desktopDuplicationTextureHandle = NULL;
static IDXGIKeyedMutex* desktopDuplicationKeyedMutex = NULL;

static int desktopDuplicationSetupDX11() {
	IDXGIFactory6* dxgiFactoryInterface = NULL;
	HRESULT hrRes = CreateDXGIFactory1(&IID_IDXGIFactory6, (void**) &dxgiFactoryInterface);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_CREATE_FACTORY;
	}
	
	//DXGI_GPU_PREFERENCE_UNSPECIFIED: First returns the adapter (GPU device) with the output on which the desktop primary is displayed
	//DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE: Get the adapter (GPU device) by choosing the most performant discrete one (if it exisits and there is no "external" GPU device)
	hrRes = dxgiFactoryInterface->lpVtbl->EnumAdapterByGpuPreference(dxgiFactoryInterface, 0, DXGI_GPU_PREFERENCE_UNSPECIFIED, &IID_IDXGIAdapter, (void**) &desktopDuplicationAdapter);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_ENUM_ADAPTER;
	}
	
	IDXGIAdapter1* adapter1Interface = NULL;
	hrRes = desktopDuplicationAdapter->lpVtbl->QueryInterface(desktopDuplicationAdapter, &IID_IDXGIAdapter1, (void**) &adapter1Interface);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_ENUM_OUTPUT;
	}
	
	DXGI_ADAPTER_DESC1 adapterDescription; //GPU Info
	hrRes = adapter1Interface->lpVtbl->GetDesc1(adapter1Interface, &adapterDescription);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_ADAPTER_DESC;
	}
	
	//Check if Card is NVIDIA brand
	//consoleWriteLineWithNumberFast("Vender ID: ", 11, adapterDescription.VendorId, NUM_FORMAT_UNSIGNED_INTEGER);
	if (adapterDescription.VendorId != NVIDIA_PCI_VENDER_ID) {
		return ERROR_DESKDUPL_ADAPTER_NOT_VALID;
	}
	
	//Save the adapter ID
	desktopDuplicationAdapterID.LowPart = adapterDescription.AdapterLuid.LowPart;
	desktopDuplicationAdapterID.HighPart = adapterDescription.AdapterLuid.HighPart;
	//consoleWriteLineWithNumberFast("UUID Low:  ", 11, desktopDuplicationAdapterID.LowPart, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("UUID High: ", 11, desktopDuplicationAdapterID.HighPart, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	adapter1Interface->lpVtbl->Release(adapter1Interface);
	
	//Creates a DirectX 11.1 Device by using chosen adapter
	//Necessary to target DirectX 11.1 which supports at least DXGI 1.2 which is used for desktop duplication
	D3D_FEATURE_LEVEL minDXfeatureTarget = D3D_FEATURE_LEVEL_11_1; //Minimum Direct X 11 Feature Level Target to Perform Desktop Duplication
	UINT creationFlags = 0;//D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT; //Maybe used to debug in future
	hrRes = D3D11CreateDevice(desktopDuplicationAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, creationFlags, &minDXfeatureTarget, 1, D3D11_SDK_VERSION, &desktopDuplicationDevice, NULL, NULL);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_CREATE_DEVICE;
	}
	
	//Create DXGI Output (Represents a monitor)
	IDXGIOutput* dxgiOutputInterface = NULL; 
	hrRes = desktopDuplicationAdapter->lpVtbl->EnumOutputs(desktopDuplicationAdapter, 0, &dxgiOutputInterface);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_ENUM_OUTPUT;
	}
	
	//Get Version 6 of the DXGI Output
	IDXGIOutput6* dxgiOutput6Interface = NULL;
	hrRes = dxgiOutputInterface->lpVtbl->QueryInterface(dxgiOutputInterface, &IID_IDXGIOutput6, (void**) &dxgiOutput6Interface);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_ENUM_OUTPUT;
	}
	dxgiOutputInterface->lpVtbl->Release(dxgiOutputInterface);
	
	DXGI_OUTPUT_DESC1 outputDescription; //Monitor Info
	hrRes = dxgiOutput6Interface->lpVtbl->GetDesc1(dxgiOutput6Interface, &outputDescription);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_OUTPUT_DESC;
	}
	//uint64_t dpiWidth = outputDescription.DesktopCoordinates.right - outputDescription.DesktopCoordinates.left;
	//uint64_t dpiHeight = outputDescription.DesktopCoordinates.bottom - outputDescription.DesktopCoordinates.top;
	//consoleWriteLineWithNumberFast("DPI Width: ", 11, dpiWidth, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("DPI Height: ", 12, dpiHeight, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//Requires DPI aware app
	DXGI_FORMAT outputFormats[7] = {
		DXGI_FORMAT_B8G8R8A8_UNORM,
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		DXGI_FORMAT_B8G8R8A8_UNORM,
		DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		DXGI_FORMAT_R10G10B10A2_UNORM,
		DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM
	};
	hrRes = dxgiOutput6Interface->lpVtbl->DuplicateOutput1(dxgiOutput6Interface, (IUnknown*) desktopDuplicationDevice, 0, 7, outputFormats, &desktopDuplicationPtr);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_CREATE_OUTPUT_DUPLICATION;
	}
	dxgiOutput6Interface->lpVtbl->Release(dxgiOutput6Interface);
	
	
	DXGI_OUTDUPL_DESC outputDuplicationDescription;
	desktopDuplicationPtr->lpVtbl->GetDesc(desktopDuplicationPtr, &outputDuplicationDescription);
	//Make sure that the image is stored in dedicated gpu memory
	if (outputDuplicationDescription.DesktopImageInSystemMemory == TRUE) {
		return ERROR_DESKDUPL_NOT_VALID;
	}
	
	//Make sure that the format is sRGB and not HDR if it is turned on for that monitor
	DXGI_MODE_DESC* modeDesc = &(outputDuplicationDescription.ModeDesc);
	if (modeDesc->Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
		return ERROR_DESKDUPL_NOT_VALID;
	}
	
	//Get Expected texture width and height dimensions
	desktopDuplicationWidth = (uint32_t) modeDesc->Width;
	desktopDuplicationHeight = (uint32_t) modeDesc->Height;
	consoleWriteLineWithNumberFast("Width: ", 7, desktopDuplicationWidth, NUM_FORMAT_UNSIGNED_INTEGER);
	consoleWriteLineWithNumberFast("Height: ", 8, desktopDuplicationHeight, NUM_FORMAT_UNSIGNED_INTEGER);
	
	
	//Get the first frame to confirm desktop dimensions, texture format, and sharable windows handle
	// and then lowers background desktop duplication resources by not releasing the frame
	IDXGIResource* resourceInterface = NULL;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;
	const uint64_t desktopDuplicationAquireFrameTimeoutMS = 100;
	uint64_t aquireFrameTries = 10;
	while (aquireFrameTries > 0) {
		hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr); //Releases resourceInterface ...?
		if (hrRes != S_OK) {
			if (hrRes != DXGI_ERROR_INVALID_CALL) {
				desktopDuplicationExtraInfo = (int) hrRes;
				return ERROR_DESKDUPL_RELEASE_FAILED;
			}
			//Frame already released
		}
		
		hrRes = desktopDuplicationPtr->lpVtbl->AcquireNextFrame(desktopDuplicationPtr, desktopDuplicationAquireFrameTimeoutMS, &frameInfo, &resourceInterface);
		if (hrRes != S_OK) {
			if (hrRes != DXGI_ERROR_WAIT_TIMEOUT) {
				desktopDuplicationExtraInfo = (int) hrRes;
				return ERROR_DESKDUPL_ACQUIRE_FAILED;
			}
			aquireFrameTries--;
		}
		else {
			if(frameInfo.LastPresentTime.QuadPart != 0) {
				if (frameInfo.AccumulatedFrames == 1) {
					aquireFrameTries = 0;
				}
			}
		}
	}
	if (hrRes == DXGI_ERROR_WAIT_TIMEOUT) {
		return ERROR_DESKDUPL_ACQUIRE_TIMEOUT;
	}
	
	ID3D11Texture2D* desktopDuplicationTexturePtr = NULL;
	hrRes = resourceInterface->lpVtbl->QueryInterface(resourceInterface, &IID_ID3D11Texture2D, (void**) &desktopDuplicationTexturePtr);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_TEXTURE_QUERY;
	}
	
	D3D11_TEXTURE2D_DESC textureDescription;
	desktopDuplicationTexturePtr->lpVtbl->GetDesc(desktopDuplicationTexturePtr, &textureDescription);
	if (textureDescription.Width != desktopDuplicationWidth) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	if (textureDescription.Height != desktopDuplicationHeight) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}	
	//Confirm that the texture format matches the output duplication texture
	if (textureDescription.Format != modeDesc->Format) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	//Confirm that the texture is dedicated on the GPU
	if (textureDescription.CPUAccessFlags != 0) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	if (textureDescription.Usage != D3D11_USAGE_DEFAULT) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	if (textureDescription.MipLevels != 1) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	if (textureDescription.ArraySize != 1) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	
	//Make Sure Resource Can Be Shared
	if ((textureDescription.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) == 0) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	if ((textureDescription.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) == 0) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	desktopDuplicationTexturePtr->lpVtbl->Release(desktopDuplicationTexturePtr);
	
	
	//Get a windows texture handle to share with Vulkan
	IDXGIResource1* resource1Interface = NULL;
	hrRes = resourceInterface->lpVtbl->QueryInterface(resourceInterface, &IID_IDXGIResource1, (void**) &resource1Interface);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_RESOURCE_QUERY;
	}
	
	desktopDuplicationTextureHandle = NULL;
	hrRes = resource1Interface->lpVtbl->CreateSharedHandle(resource1Interface, NULL, DXGI_SHARED_RESOURCE_READ, NULL, &desktopDuplicationTextureHandle);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_CREATE_SHARED_HANDLE;
	}
	resource1Interface->lpVtbl->Release(resource1Interface);
	
	//Confirm Shared Handle
	if (desktopDuplicationTextureHandle == NULL) {
		return ERROR_DESKDUPL_CREATE_SHARED_HANDLE;
	}
	//consoleWriteLineWithNumberFast("Pointer: ", 9, (uint64_t) desktopDuplicationTextureHandle, NUM_FORMAT_FULL_HEXADECIMAL);
	
	hrRes = desktopDuplicationTexturePtr->lpVtbl->QueryInterface(desktopDuplicationTexturePtr, &IID_IDXGIKeyedMutex, (void**) &desktopDuplicationKeyedMutex);
	if (hrRes != S_OK) {
		desktopDuplicationExtraInfo = (int) hrRes;
		return ERROR_DESKDUPL_KEYEDMUTEX_QUERY;
	}
	
	return 0;
}


#define VULKAN_ALLOCATOR NULL

static VkResult vulkanExtraInfo = VK_SUCCESS;

static VkInstance vulkanInstance = VK_NULL_HANDLE;
static VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
static VkDevice vulkanDevice = VK_NULL_HANDLE;
static VkQueue vulkanComputeQueue = VK_NULL_HANDLE;

static VkImage vulkanDuplicationTexturePtr = VK_NULL_HANDLE;
static VkImage vulkanConvertedTexturePtr = VK_NULL_HANDLE;
static VkBuffer vulkanStagingBufferPtr = VK_NULL_HANDLE;
static VkBuffer vulkanLutPtr = VK_NULL_HANDLE;

static VkDeviceMemory vulkanMemGPUimport = VK_NULL_HANDLE;
static VkDeviceMemory vulkanMemGPUexport = VK_NULL_HANDLE;
static VkDeviceSize   vulkanMemGPUexportSize = 0;
static const LPCWSTR  vulkanMemGPUexportStr  = L"TextureExportHandle";
static HANDLE desktopDuplicationTextureConvertedHandle = NULL;
static VkDeviceMemory vulkanMemCPUaccess = VK_NULL_HANDLE;
static VkDeviceMemory vulkanMemGPUexclusive = VK_NULL_HANDLE;

static VkCommandPool vulkanCommandPool = VK_NULL_HANDLE;
static VkCommandBuffer vulkanComputeCommandBuffers[5]; // = {VK_NULL_HANDLE};

static VkShaderModule vulkanComputeShaderModule = VK_NULL_HANDLE;
static VkDescriptorSetLayout vulkanDescriptorSetLayout = VK_NULL_HANDLE;
static VkPipelineLayout vulkanComputePipelineLayout = VK_NULL_HANDLE;
static VkPipeline vulkanComputePipeline = VK_NULL_HANDLE;
static VkDescriptorPool vulkanDescriptorPool = VK_NULL_HANDLE;
static VkImageView vulkanDuplicationTextureView = VK_NULL_HANDLE;
static VkImageView vulkanConvertedTextureView = VK_NULL_HANDLE;

static VkFence vulkanComputeFence = VK_NULL_HANDLE;

//Sets up Vulkan with compute and DirectX11 interfacing in mind
static int desktopDuplicationSetupVulkan(size_t shaderSize, uint32_t* shaderData) {
	//Create Vulkan Instance
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan Compute Shader";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "Vulkan Compute Engine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;
	
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	
	createInfo.enabledLayerCount = 0; //Update this in future to use validation layers
	
	createInfo.enabledExtensionCount = 0; //2 for when its time to output vulkan texture to win32 surface //0 when not using graphics
	const char* const extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME};
	createInfo.ppEnabledExtensionNames = extensions;
	
	VkResult result = vkCreateInstance(&createInfo, VULKAN_ALLOCATOR, &vulkanInstance);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_CREATE_INSTANCE_FAILED;
	}
	
	VkPhysicalDevice pDevices[32]; //256 Bytes: 32 * 8 (sizeof(VkPhysicalDevice))
	uint32_t deviceCount = 32;
	vkEnumeratePhysicalDevices(vulkanInstance, &deviceCount, pDevices);
	if (deviceCount == 0) {
		return ERROR_VULKAN_NO_PHYSICAL_DEVICES;
	}
	
	
	
	VkPhysicalDeviceIDProperties devicePropertiesID;
	devicePropertiesID.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
	devicePropertiesID.pNext = NULL;
	
	VkPhysicalDeviceProperties2 deviceProperties2;
	deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	deviceProperties2.pNext = &devicePropertiesID;
	
	for (uint32_t d = 0; d < deviceCount; d++) {
		vkGetPhysicalDeviceProperties2(pDevices[d], &deviceProperties2);
		
		if (devicePropertiesID.deviceLUIDValid == VK_TRUE) {
			uint32_t* deviceUUID = (uint32_t*) &(devicePropertiesID.deviceLUID);
			//consoleWriteLineWithNumberFast("UUID Low:  ", 11, deviceUUID[0], NUM_FORMAT_PARTIAL_HEXADECIMAL);
			//consoleWriteLineWithNumberFast("UUID High: ", 11, deviceUUID[1], NUM_FORMAT_PARTIAL_HEXADECIMAL);
			if (deviceUUID[0] == desktopDuplicationAdapterID.LowPart) {
				if (deviceUUID[1] == desktopDuplicationAdapterID.HighPart) {
					vulkanPhysicalDevice = pDevices[d];
					d = deviceCount; //break;
				}
			}
		}		
	}
	if (vulkanPhysicalDevice == VK_NULL_HANDLE) {
		return ERROR_VULKAN_CANNOT_FIND_GPU;
	}
	
	
	//consoleWriteLineWithNumberFast("Number: ", 8, sizeof(VkQueueFamilyProperties), NUM_FORMAT_UNSIGNED_INTEGER);
	//uint32_t queueFamilyCount = 0;
	//vkGetPhysicalDeviceQueueFamilyProperties(vulkanPhysicalDevice, &queueFamilyCount, NULL);
	//consoleWriteLineWithNumberFast("Number: ", 8, queueFamilyCount, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//Find Vulkan Compute Queue Family Index (that includes graphics capabilities)
	uint32_t computeQueueFamilyIndex = 256;
	VkQueueFamilyProperties pQueueFamilyProperties[32]; //768 Bytes: 32 * 24 (sizeof(VkQueueFamilyProperties))
	uint32_t queueFamilyCount = 32;
	vkGetPhysicalDeviceQueueFamilyProperties(vulkanPhysicalDevice, &queueFamilyCount, pQueueFamilyProperties);
	
	//for (uint32_t q = 0; q < queueFamilyCount; q++) {
	//	VkQueueFlags qF = pQueueFamilyProperties[q].queueFlags;
	//	consoleWriteLineWithNumberFast("Queue Flags: ", 13, qF, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//}
	
	//uint32_t computeTransferCount = 0;
	for (uint32_t q = 0; q < queueFamilyCount; q++) {
		VkQueueFlags qF = pQueueFamilyProperties[q].queueFlags;
		
		if ((qF & VK_QUEUE_COMPUTE_BIT) && (qF & VK_QUEUE_TRANSFER_BIT )) {
			//computeTransferCount++;
			computeQueueFamilyIndex = q;
			if ((qF & VK_QUEUE_GRAPHICS_BIT) == 0) { //If there is one not sharing a graphics queue break early
				q = queueFamilyCount; //break;
			}
		}
	}
	if (computeQueueFamilyIndex == 256) {
		return ERROR_VULKAN_NO_COMPUTE_QUEUE;
	}
	
	
	//Vulkan Create Device and Queue
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = NULL;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 1;
	
	VkDeviceQueueCreateInfo queueCreateInfos[1];
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].pNext = NULL;
	queueCreateInfos[0].flags = 0;
	queueCreateInfos[0].queueFamilyIndex = computeQueueFamilyIndex;
	queueCreateInfos[0].queueCount = 1;
	float queuePriority = 1.0;
	queueCreateInfos[0].pQueuePriorities = &queuePriority;
	
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
	
	deviceCreateInfo.enabledLayerCount = 0; //deprecated and ignored
	deviceCreateInfo.ppEnabledLayerNames = NULL; //deprecated and ignored
	
	deviceCreateInfo.enabledExtensionCount = 2;
	const char* const deviceExtensions[] = {
		VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
		VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME
		}; //VK_KHR_SWAPCHAIN_EXTENSION_NAME
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(vulkanPhysicalDevice, &deviceFeatures);
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	
	result = vkCreateDevice(vulkanPhysicalDevice, &deviceCreateInfo, VULKAN_ALLOCATOR, &vulkanDevice);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_DEVICE_CREATION_FAILED;
	}
	
	//consoleWriteLineSlow("Vulkan Init Device");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	//Get Vulkan Compute Queue (index 0 since queue count is 1)
	vkGetDeviceQueue(vulkanDevice, computeQueueFamilyIndex, 0, &vulkanComputeQueue);
	//Add error checking in future
	
	//VkExternalImageFormatProperties extProperties;
	//result = vkGetPhysicalDeviceExternalImageFormatProperties(vulkanPhysicalDevice, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 0, handleType, &extProperties);
	
	//Vulkan Images Creation
	//Optimal Tiling for Images
	VkFormatProperties formatProperties;
	VkFormat vulkanFormat = VK_FORMAT_B8G8R8A8_UNORM; //Forced from desktop duplication setup
	vkGetPhysicalDeviceFormatProperties(vulkanPhysicalDevice, vulkanFormat, &formatProperties);
	uint32_t formatFeatures = (uint32_t) formatProperties.optimalTilingFeatures;
	//consoleWriteLineWithNumberFast("Number: ", 8, formatFeatures, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	if (!((formatFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && (formatFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) && (formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))) {
		return ERROR_VULKAN_BAD_OPTIMAL_FEATURES;
	}
	
	
	
	//Create Image that Points to DX11 Desktop Duplication Texture
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	
	VkExternalMemoryImageCreateInfo imageInfoExternal = {};
	imageInfoExternal.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	imageInfoExternal.pNext = NULL;
	imageInfoExternal.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
	
	imageInfo.pNext = &imageInfoExternal;
	imageInfo.flags = 0;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM; //Verified Earlier
	imageInfo.extent.width = desktopDuplicationWidth;
	imageInfo.extent.height = desktopDuplicationHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1; //desktopDuplicationMipLevels;
	imageInfo.arrayLayers = 1; //desktopDuplicationArrayLayers;
	imageInfo.samples = 1; //desktopDuplicationSamples;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; //Optimal Texture
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = 0;
	imageInfo.pQueueFamilyIndices = NULL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
	
	result = vkCreateImage(vulkanDevice, &imageInfo, VULKAN_ALLOCATOR, &vulkanDuplicationTexturePtr);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_IMAGE_CREATION_FAILED;
	}
	
	
	//Create Image that Will Reference the DirectX 11 texture
	imageInfo.format = VK_FORMAT_R16_UNORM;
	imageInfo.extent.height = desktopDuplicationHeight * 3;
	imageInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	result = vkCreateImage(vulkanDevice, &imageInfo, VULKAN_ALLOCATOR, &vulkanConvertedTexturePtr);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_IMAGE_CREATION_FAILED;
	}
	
	
	//Create a Vulkan Buffer that will be used to store the sRGB Color Conversion LUT
	const size_t numSRGBvalues = 16777216; // (256 ^ 3)
	VkDeviceSize lutSize = numSRGBvalues * sizeof(uint32_t);
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = NULL;
	bufferInfo.flags = 0;
	bufferInfo.size = lutSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.queueFamilyIndexCount = 0;
	bufferInfo.pQueueFamilyIndices = NULL;
	
	result = vkCreateBuffer(vulkanDevice, &bufferInfo, VULKAN_ALLOCATOR, &vulkanStagingBufferPtr);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_BUFFER_CREATION_FAILED;
	}
	
	bufferInfo.usage |= VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	
	result = vkCreateBuffer(vulkanDevice, &bufferInfo, VULKAN_ALLOCATOR, &vulkanLutPtr);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_BUFFER_CREATION_FAILED;
	}
	
	
	//consoleWriteLineSlow("Vulkan Create Images and Buffers");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	//List phsyical device mem properties:
	uint32_t deviceLocalOnlyMemoryTypeIndex = 1;
	uint32_t basicCPUaccessMemoryTypeIndex = 2;
	
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vulkanPhysicalDevice, &memProperties);
	uint32_t memType = memProperties.memoryTypeCount;
	for (uint32_t i = 0; i < memType; i++) {
		VkMemoryPropertyFlags memTypeProp = memProperties.memoryTypes[i].propertyFlags;
		if (memTypeProp == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
			deviceLocalOnlyMemoryTypeIndex = i;
		}
		if (memTypeProp == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
			basicCPUaccessMemoryTypeIndex = i;
		}
		//consoleWriteLineWithNumberFast("Prop: ", 6, memTypeProp, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("Ind: ", 5, memProperties.memoryTypes[i].heapIndex, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	}
	//consoleWriteLineWithNumberFast("Number: ", 8, memProperties.memoryHeapCount, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//uint32_t heaps = memProperties.memoryHeapCount;
	//for (uint32_t i = 0; i < heaps; i++) {
	//	VkDeviceSize heapSize = memProperties.memoryHeaps[i].size;
		//consoleWriteLineWithNumberFast("Size: ", 6, heapSize, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("Flags: ", 7, memProperties.memoryHeaps[i].flags, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//}
	//In the future these values will be based on a function above that processes all memory types
	
	
	
	//"Allocate" Memory for Importing the DX11 Image Texture
	VkMemoryRequirements imageMemReqs;
	vkGetImageMemoryRequirements(vulkanDevice, vulkanDuplicationTexturePtr, &imageMemReqs);
	//consoleWriteLineWithNumberFast("Number: ", 8, imageMemReqs.memoryTypeBits, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	VkMemoryAllocateInfo memAllocInfo = {};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	
	VkImportMemoryWin32HandleInfoKHR win32HandleImportInfo = {};
	win32HandleImportInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	win32HandleImportInfo.pNext = NULL;
	
	VkExternalMemoryHandleTypeFlags handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
	win32HandleImportInfo.handleType = handleType;
	
	win32HandleImportInfo.handle = desktopDuplicationTextureHandle;
	win32HandleImportInfo.name = NULL; //Null when handle is not null
	
	
	memAllocInfo.pNext = &win32HandleImportInfo;
	memAllocInfo.allocationSize = imageMemReqs.size; //Ignored for Imported Memory
	
	PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR2 = (PFN_vkGetMemoryWin32HandlePropertiesKHR) (vkGetDeviceProcAddr(vulkanDevice, "vkGetMemoryWin32HandlePropertiesKHR"));
	
	VkMemoryWin32HandlePropertiesKHR win32HandleProp;
	result = vkGetMemoryWin32HandlePropertiesKHR2(vulkanDevice, handleType, desktopDuplicationTextureHandle, &win32HandleProp);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	uint32_t sharedMemoryTypeBits = win32HandleProp.memoryTypeBits;
	//consoleWriteLineWithNumberFast("MemT: ", 6, sharedMemoryTypeBits, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	uint32_t memoryIndex = 0;
	while ((sharedMemoryTypeBits & 0x1) != 1) {
		memoryIndex++;
		sharedMemoryTypeBits >>= 1;
	}
	memAllocInfo.memoryTypeIndex = memoryIndex;
	
	result = vkAllocateMemory(vulkanDevice, &memAllocInfo, VULKAN_ALLOCATOR, &vulkanMemGPUimport);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	result = vkBindImageMemory(vulkanDevice, vulkanDuplicationTexturePtr, vulkanMemGPUimport, 0); //0 offset
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	//consoleWriteLineSlow("Vulkan Allocate for GPU import");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	VkImageMemoryRequirementsInfo2 imageMemReqsInfo;
	imageMemReqsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
	imageMemReqsInfo.pNext = NULL;
	imageMemReqsInfo.image = vulkanConvertedTexturePtr;
	
	VkMemoryRequirements2 memReqs2;
	memReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	
	VkMemoryDedicatedRequirementsKHR dedicatedReqs;
	dedicatedReqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;
	dedicatedReqs.pNext = NULL;
	
	memReqs2.pNext = &dedicatedReqs;
	
	vkGetImageMemoryRequirements2(vulkanDevice, &imageMemReqsInfo, &memReqs2);
	//consoleWriteLineWithNumberFast("Size: ", 6, memReqs2.memoryRequirements.size, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Alignment: ", 11, memReqs2.memoryRequirements.alignment, NUM_FORMAT_PARTIAL_HEXADECIMAL);	
	//consoleWriteLineWithNumberFast("Dedication: ", 12, dedicatedReqs.requiresDedicatedAllocation, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	VkMemoryDedicatedAllocateInfoKHR memDedicatedAllocInfo;
	memDedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	
	VkExportMemoryAllocateInfo exportMemoryInfo = {};
	exportMemoryInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
	
	VkExportMemoryWin32HandleInfoKHR win32HandleExportInfo = {};
	win32HandleExportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	win32HandleExportInfo.pNext = NULL;
	
	SECURITY_ATTRIBUTES securityAttributes = {0};
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = NULL;
	securityAttributes.bInheritHandle = FALSE;
	
	win32HandleExportInfo.pAttributes = &securityAttributes; //Does not matter if pAttributes is null...
	win32HandleExportInfo.dwAccess = DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE; // | GENERIC_ALL;
	win32HandleExportInfo.name = vulkanMemGPUexportStr;
	
	exportMemoryInfo.pNext = &win32HandleExportInfo;
	exportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	
	memDedicatedAllocInfo.pNext = &exportMemoryInfo;
	memDedicatedAllocInfo.image = vulkanConvertedTexturePtr;
	memDedicatedAllocInfo.buffer = VK_NULL_HANDLE;
	
	memAllocInfo.pNext = &memDedicatedAllocInfo;
	memAllocInfo.allocationSize = memReqs2.memoryRequirements.size;
	memAllocInfo.memoryTypeIndex = deviceLocalOnlyMemoryTypeIndex;
	
	result = vkAllocateMemory(vulkanDevice, &memAllocInfo, VULKAN_ALLOCATOR, &vulkanMemGPUexport);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	vulkanMemGPUexportSize = memReqs2.memoryRequirements.size;
	
	result = vkBindImageMemory(vulkanDevice, vulkanConvertedTexturePtr, vulkanMemGPUexport, 0);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR2 = (PFN_vkGetMemoryWin32HandleKHR) (vkGetDeviceProcAddr(vulkanDevice, "vkGetMemoryWin32HandleKHR"));
	
	VkMemoryGetWin32HandleInfoKHR win32HandleInfo = {};
	win32HandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
	win32HandleInfo.pNext = NULL;
	win32HandleInfo.memory = vulkanMemGPUexport;
	win32HandleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	
	result = vkGetMemoryWin32HandleKHR2(vulkanDevice, &win32HandleInfo, &desktopDuplicationTextureConvertedHandle);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineWithNumberFast("Size B: ", 8, (uint64_t) vulkanMemGPUexportSize, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Handle: ", 8, (uint64_t) desktopDuplicationTextureConvertedHandle, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	//consoleWriteLineSlow("Vulkan Allocate for GPU export");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	//Allocate memory for the staging texture
	VkMemoryRequirements bufMemReqs;
	vkGetBufferMemoryRequirements(vulkanDevice, vulkanStagingBufferPtr, &bufMemReqs);
	//consoleWriteLineWithNumberFast("Size: ", 6, bufMemReqs.size, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Alignment: ", 11, bufMemReqs.alignment, NUM_FORMAT_PARTIAL_HEXADECIMAL);	
	
	memAllocInfo.pNext = NULL;
	memAllocInfo.allocationSize = bufMemReqs.size;
	memAllocInfo.memoryTypeIndex = basicCPUaccessMemoryTypeIndex;
	
	result = vkAllocateMemory(vulkanDevice, &memAllocInfo, VULKAN_ALLOCATOR, &vulkanMemCPUaccess); //64-byte aligned most likely
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	result = vkBindBufferMemory(vulkanDevice, vulkanStagingBufferPtr, vulkanMemCPUaccess, 0);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	//consoleWriteLineSlow("Vulkan Stage Buffer");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	//Allocate Memory for GPU exclusive LUT
	VkBufferMemoryRequirementsInfo2 bufMemReqsInfo;
	bufMemReqsInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
	bufMemReqsInfo.pNext = NULL;
	bufMemReqsInfo.buffer = vulkanLutPtr;
	
	vkGetBufferMemoryRequirements2(vulkanDevice, &bufMemReqsInfo, &memReqs2);
	//consoleWriteLineWithNumberFast("Size: ", 6, memReqs2.memoryRequirements.size, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Alignment: ", 11, memReqs2.memoryRequirements.alignment, NUM_FORMAT_PARTIAL_HEXADECIMAL);	
	//consoleWriteLineWithNumberFast("Dedication: ", 12, dedicatedReqs.prefersDedicatedAllocation, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	if (dedicatedReqs.prefersDedicatedAllocation != 0) {
		memDedicatedAllocInfo.pNext = NULL;
		memDedicatedAllocInfo.image = VK_NULL_HANDLE;
		memDedicatedAllocInfo.buffer = vulkanLutPtr;
		
		memAllocInfo.pNext = &memDedicatedAllocInfo;
	}
	else {
		memAllocInfo.pNext = NULL;
	}
	
	memAllocInfo.allocationSize = memReqs2.memoryRequirements.size;
	memAllocInfo.memoryTypeIndex = deviceLocalOnlyMemoryTypeIndex;
	
	result = vkAllocateMemory(vulkanDevice, &memAllocInfo, VULKAN_ALLOCATOR, &vulkanMemGPUexclusive);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	result = vkBindBufferMemory(vulkanDevice, vulkanLutPtr, vulkanMemGPUexclusive, 0);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	//consoleWriteLineSlow("Vulkan Allocate for GPU exclusive buffer");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	
	//Command Pool, Buffer Creation, and Buffer Recording
	//Binding and Compute Pipeline later...
	
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = NULL;
	poolInfo.flags = 0; //Double check in future...
	poolInfo.queueFamilyIndex = computeQueueFamilyIndex;
	
	result = vkCreateCommandPool(vulkanDevice, &poolInfo, VULKAN_ALLOCATOR, &vulkanCommandPool);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COMMAND_POOL_FAILED;
	}
	
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.commandPool = vulkanCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 5;
	
	result = vkAllocateCommandBuffers(vulkanDevice, &allocInfo, vulkanComputeCommandBuffers);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COMMAND_BUFFER_FAILED;
	}
	
	
	//Staging Buffer -> Lut Buffer Transfer
	VkCommandBuffer lutTransfer = vulkanComputeCommandBuffers[0];
	
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = NULL;
	beginInfo.flags = 0; //Double check in future...
	beginInfo.pInheritanceInfo = NULL; // Optional
	
	result = vkBeginCommandBuffer(lutTransfer, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = lutSize;
	
	vkCmdCopyBuffer(lutTransfer, vulkanStagingBufferPtr, vulkanLutPtr, 1, &bufferCopyRegion);
	
	result = vkEndCommandBuffer(lutTransfer);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	
	
	//Desktop Duplication Texture -> Staging Buffer (Offset: 0)
	VkCommandBuffer imgTransfer = vulkanComputeCommandBuffers[1];
	result = vkBeginCommandBuffer(imgTransfer, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	VkBufferImageCopy imgToBufRegions[3];
	
	imgToBufRegions[0].bufferOffset = 0;
	imgToBufRegions[0].bufferRowLength = 0;
	imgToBufRegions[0].bufferImageHeight = 0;
	imgToBufRegions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //IS COMPLETELY NECESSARY
	imgToBufRegions[0].imageSubresource.mipLevel = 0;
	imgToBufRegions[0].imageSubresource.baseArrayLayer = 0;
	imgToBufRegions[0].imageSubresource.layerCount = 1;
	imgToBufRegions[0].imageOffset.x = 0;
	imgToBufRegions[0].imageOffset.y = 0;
	imgToBufRegions[0].imageOffset.z = 0;
	imgToBufRegions[0].imageExtent.width = desktopDuplicationWidth;
	imgToBufRegions[0].imageExtent.height = desktopDuplicationHeight;
	imgToBufRegions[0].imageExtent.depth = 1;
	
	vkCmdCopyImageToBuffer(imgTransfer, vulkanDuplicationTexturePtr, VK_IMAGE_LAYOUT_GENERAL, vulkanStagingBufferPtr, 1, imgToBufRegions);
	
	result = vkEndCommandBuffer(imgTransfer);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	
	//Write and Read of Converted Texture (using staging buffer) to be used as Input of Video Encoder
	imgToBufRegions[0].bufferOffset = desktopDuplicationWidth * desktopDuplicationHeight * 4;
	imgToBufRegions[0].imageExtent.height = desktopDuplicationHeight * 3;
	
	//Staging Buffer (Offset: 1) -> Converted Texture
	VkCommandBuffer imgTransferWrite = vulkanComputeCommandBuffers[2];
	result = vkBeginCommandBuffer(imgTransferWrite, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	vkCmdCopyBufferToImage(imgTransferWrite, vulkanStagingBufferPtr, vulkanConvertedTexturePtr, VK_IMAGE_LAYOUT_GENERAL, 1, imgToBufRegions);
	
	result = vkEndCommandBuffer(imgTransferWrite);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	//Converted Texture -> Staging Buffer (Offset: 1)
	VkCommandBuffer imgTransferRead = vulkanComputeCommandBuffers[3];
	result = vkBeginCommandBuffer(imgTransferRead, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	imgToBufRegions[0].bufferOffset = 0;
	
	vkCmdCopyImageToBuffer(imgTransferRead, vulkanConvertedTexturePtr, VK_IMAGE_LAYOUT_GENERAL, vulkanStagingBufferPtr, 1, imgToBufRegions);
	
	result = vkEndCommandBuffer(imgTransferRead);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	//consoleWriteLineSlow("Vulkan Allocate for First Command Buffers");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	
	
	//Compute Shader for Making the Converted Texture
	//Desktop Duplication Texture -> Converted Texture 
	VkComputePipelineCreateInfo computePipelineInfo = {};
	computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineInfo.pNext = NULL;
	computePipelineInfo.flags = 0; //Double Check
	computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computePipelineInfo.stage.pNext = NULL;
	computePipelineInfo.stage.flags = 0; //Double Check
	computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	
	//Shader Module Creation and Definition
	VkShaderModuleCreateInfo shaderModuleInfo = {};
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.pNext = NULL;
	shaderModuleInfo.flags = 0;
	shaderModuleInfo.codeSize = shaderSize;
	shaderModuleInfo.pCode = shaderData;
	//consoleWriteLineSlow("Got Here!");
	
	result = vkCreateShaderModule(vulkanDevice, &shaderModuleInfo, VULKAN_ALLOCATOR, &vulkanComputeShaderModule);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineSlow("Got Here!");
	
	//consoleWriteLineSlow("Vulkan Allocate for shader");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	computePipelineInfo.stage.module = vulkanComputeShaderModule;
	computePipelineInfo.stage.pName = "main";
	computePipelineInfo.stage.pSpecializationInfo = NULL; //Pretty Sure
	
	//Pipeline Layout Creation and Definition
	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext = NULL;
	pipelineLayoutInfo.flags = 0;
	pipelineLayoutInfo.setLayoutCount = 1;
	
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = {};
	descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutInfo.pNext = NULL;
	descriptorSetLayoutInfo.flags = 0; //Double Check
	descriptorSetLayoutInfo.bindingCount = 3;
	
	VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[3];
	descriptorSetLayoutBindings[0].binding = 0;
	descriptorSetLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorSetLayoutBindings[0].descriptorCount = 1;
	descriptorSetLayoutBindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descriptorSetLayoutBindings[0].pImmutableSamplers = NULL;
	descriptorSetLayoutBindings[1].binding = 1;
	descriptorSetLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; //VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
	descriptorSetLayoutBindings[1].descriptorCount = 1;
	descriptorSetLayoutBindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descriptorSetLayoutBindings[1].pImmutableSamplers = NULL;
	descriptorSetLayoutBindings[2].binding = 2;
	descriptorSetLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorSetLayoutBindings[2].descriptorCount = 1;
	descriptorSetLayoutBindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	descriptorSetLayoutBindings[2].pImmutableSamplers = NULL;
	
	descriptorSetLayoutInfo.pBindings = descriptorSetLayoutBindings;
	
	result = vkCreateDescriptorSetLayout(vulkanDevice, &descriptorSetLayoutInfo, VULKAN_ALLOCATOR, &vulkanDescriptorSetLayout);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	
	pipelineLayoutInfo.pSetLayouts = &vulkanDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;//Maybe used in the future...
	pipelineLayoutInfo.pPushConstantRanges = NULL;
	
	result = vkCreatePipelineLayout(vulkanDevice, &pipelineLayoutInfo, VULKAN_ALLOCATOR, &vulkanComputePipelineLayout);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineSlow("Got Here!");
	
	computePipelineInfo.layout = vulkanComputePipelineLayout;
	computePipelineInfo.basePipelineHandle = 0; //Not Sure
	computePipelineInfo.basePipelineIndex = 0; //Not Sure
	
	result = vkCreateComputePipelines(vulkanDevice, VK_NULL_HANDLE, 1, &computePipelineInfo, VULKAN_ALLOCATOR, &vulkanComputePipeline);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineSlow("Got Here!");
	
	
	//Descriptor Set (allocated from a descriptor pool)
	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = NULL;
	descriptorPoolInfo.flags = 0; //Double Check
	descriptorPoolInfo.maxSets = 1; //Since only one Discriptor Set will be allocated from this pool
	descriptorPoolInfo.poolSizeCount = 2;
	
	VkDescriptorPoolSize descriptorPoolSizes[2];
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorPoolSizes[0].descriptorCount = 2;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorPoolSizes[1].descriptorCount = 1;
	
	descriptorPoolInfo.pPoolSizes = descriptorPoolSizes;
	
	result = vkCreateDescriptorPool(vulkanDevice, &descriptorPoolInfo, VULKAN_ALLOCATOR, &vulkanDescriptorPool);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
	descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocInfo.pNext = NULL;
	descriptorSetAllocInfo.descriptorPool = vulkanDescriptorPool;
	descriptorSetAllocInfo.descriptorSetCount = 1;
	descriptorSetAllocInfo.pSetLayouts = &vulkanDescriptorSetLayout;
	
	VkDescriptorSet vulkanDescriptorSet = NULL;
	result = vkAllocateDescriptorSets(vulkanDevice, &descriptorSetAllocInfo, &vulkanDescriptorSet);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	//Create Image Views
	VkImageViewCreateInfo imgViewInfo = {};
	imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imgViewInfo.pNext = NULL;
	imgViewInfo.flags = 0; //Double Check
	imgViewInfo.image = vulkanDuplicationTexturePtr;
	imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imgViewInfo.format = VK_FORMAT_B8G8R8A8_UINT;
	imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //Probably neccessary
	imgViewInfo.subresourceRange.baseMipLevel = 0;
	imgViewInfo.subresourceRange.levelCount = 1;
	imgViewInfo.subresourceRange.baseArrayLayer = 0;
	imgViewInfo.subresourceRange.layerCount = 1;
	
	result = vkCreateImageView(vulkanDevice, &imgViewInfo, VULKAN_ALLOCATOR, &vulkanDuplicationTextureView);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	imgViewInfo.image = vulkanConvertedTexturePtr;
	imgViewInfo.format = VK_FORMAT_R16_UINT;
	
	result = vkCreateImageView(vulkanDevice, &imgViewInfo, VULKAN_ALLOCATOR, &vulkanConvertedTextureView);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	
	VkWriteDescriptorSet writeDescriptorSets[3];
	writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[0].pNext = NULL;
	writeDescriptorSets[0].dstSet = vulkanDescriptorSet;
	writeDescriptorSets[0].dstBinding = 0;
	writeDescriptorSets[0].dstArrayElement = 0;
	writeDescriptorSets[0].descriptorCount = 2;
	writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;	
	
	VkDescriptorImageInfo descriptorImgInfos[2];
	descriptorImgInfos[0].sampler = VK_NULL_HANDLE; //Not needed since sampling is not performed?
	descriptorImgInfos[0].imageView = vulkanDuplicationTextureView;
	descriptorImgInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	descriptorImgInfos[1].sampler = VK_NULL_HANDLE; //Not needed since sampling is not performed?	
	descriptorImgInfos[1].imageView = vulkanConvertedTextureView;
	descriptorImgInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	
	writeDescriptorSets[0].pImageInfo = &descriptorImgInfos[0];
	writeDescriptorSets[0].pBufferInfo = NULL;
	writeDescriptorSets[0].pTexelBufferView = NULL;
	
	writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[1].pNext = NULL;
	writeDescriptorSets[1].dstSet = vulkanDescriptorSet;
	writeDescriptorSets[1].dstBinding = 1;
	writeDescriptorSets[1].dstArrayElement = 0;
	writeDescriptorSets[1].descriptorCount = 1;
	writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	
	VkDescriptorBufferInfo descriptorBufInfo = {};
	descriptorBufInfo.buffer = vulkanLutPtr;
	descriptorBufInfo.offset = 0;
	descriptorBufInfo.range = VK_WHOLE_SIZE;
	
	writeDescriptorSets[1].pImageInfo = NULL;
	writeDescriptorSets[1].pBufferInfo = &descriptorBufInfo;
	writeDescriptorSets[1].pTexelBufferView = NULL;
	
	writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[2].pNext = NULL;
	writeDescriptorSets[2].dstSet = vulkanDescriptorSet;
	writeDescriptorSets[2].dstBinding = 2;
	writeDescriptorSets[2].dstArrayElement = 0;
	writeDescriptorSets[2].descriptorCount = 1;
	writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	writeDescriptorSets[2].pImageInfo = &descriptorImgInfos[1];
	writeDescriptorSets[2].pBufferInfo = NULL;
	writeDescriptorSets[2].pTexelBufferView = NULL;
	
	vkUpdateDescriptorSets(vulkanDevice, 3, writeDescriptorSets, 0, NULL);
	
	
	VkCommandBuffer computeCommand = vulkanComputeCommandBuffers[4];
	result = vkBeginCommandBuffer(computeCommand, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	vkCmdBindPipeline(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanComputePipeline);
	vkCmdBindDescriptorSets(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, vulkanComputePipelineLayout, 0, 1, &vulkanDescriptorSet, 0, NULL);
	vkCmdDispatch(computeCommand, desktopDuplicationWidth >> 4, desktopDuplicationHeight >> 2, 1); //Based on shader local_sizes
	
	result = vkEndCommandBuffer(computeCommand);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	//Create the Vulkan Compute Finish Fence
	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = NULL;
	fenceInfo.flags = 0;
	
	result = vkCreateFence(vulkanDevice, &fenceInfo, VULKAN_ALLOCATOR, &vulkanComputeFence);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	return 0;
}

static uint32_t greatestCommonDivisor(uint32_t a, uint32_t b) {
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

static int cuExtraInfo = CUDA_SUCCESS;

static CUdevice cuDevice = 0;
static CUcontext cuContext = 0;
static CUexternalMemory cuExtMem = 0;
static CUmipmappedArray cuExtMipArray = 0;
static CUarray cuExtArray = 0;

static int nvEncExtraInfo = NV_ENC_SUCCESS;

static NV_ENCODE_API_FUNCTION_LIST nvEncFunList = {};
static void* nvEncoder = NULL;
static NV_ENC_CREATE_BITSTREAM_BUFFER nvEncBitstreamBuff0 = {};
static NV_ENC_CREATE_BITSTREAM_BUFFER nvEncBitstreamBuff1 = {};
static NV_ENC_PIC_PARAMS nvEncPicParams = {};

static int desktopDuplicationSetupNvEnc() {
	//Setup CUDA interface first
	CUresult cuRes = cuInit(0);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_NO_INIT;
	}
	
	//Do CUDA Version check in future once knowing what to compare it to
	int cudaVersion = 0;
	cuRes = cuDriverGetVersion(&cudaVersion);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_GET_VERSION;
	}
	//consoleWriteLineWithNumberFast("Cuda Version: ", 14, (uint64_t) cudaVersion, NUM_FORMAT_UNSIGNED_INTEGER);
	if (cudaVersion < 10000) {
		return ERROR_CUDA_LOW_VERSION;
	}
	
	
	int numCudaDevices = 0;
	cuRes = cuDeviceGetCount(&numCudaDevices);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_NO_DEVICES;
	}
	if (numCudaDevices == 0) {
		return ERROR_CUDA_NO_DEVICES;
	}
	
	//int cudaDeviceNum = 0;
	for (int d=0; d<numCudaDevices; d++) {
		cuRes = cuDeviceGet(&cuDevice, 0);
		if (cuRes != CUDA_SUCCESS) {
			cuExtraInfo = (int) cuRes;
			return ERROR_CUDA_CANNOT_GET_DEVICE;
		}
		
		char luid[16] = {};
		unsigned int deviceNodeMask = 0;
		cuRes = cuDeviceGetLuid(luid, &deviceNodeMask, cuDevice);
		if (cuRes != CUDA_SUCCESS) {
			cuExtraInfo = (int) cuRes;
			return ERROR_CUDA_CANNOT_GET_DEVICE_LUID;
		}
		
		uint32_t* luidConversion = (uint32_t*) luid;
		//consoleWriteLineWithNumberFast("UUID Low:  ", 11, luidConversion[0], NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("UUID High: ", 11, luidConversion[1], NUM_FORMAT_PARTIAL_HEXADECIMAL);
		if (luidConversion[0] == desktopDuplicationAdapterID.LowPart) {
			if (luidConversion[1] == desktopDuplicationAdapterID.HighPart) {
				//cudaDeviceNum = d;
				d = numCudaDevices; //break
			}
		}
	}
	
	unsigned int cudaContexFlags = 0;
	int cudaContexState = 0;
	cuRes = cuDevicePrimaryCtxGetState(cuDevice, &cudaContexFlags, &cudaContexState);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_GET_CONTEXT_STATE;
	}
	//consoleWriteLineWithNumberFast("Context Flags: ", 15, cudaContexFlags, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Context State: ", 15, cudaContexState, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	if (cudaContexState == 1) {
		consoleWriteLineFast("Warning: Cuda Possibly Active!", 30);
	}
	
	//consoleWriteLineSlow("Cuda Device Init");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	//Get Cuda Context, can set flags with: cuDevicePrimaryCtxSetFlags() function
	cuRes = cuDevicePrimaryCtxRetain(&cuContext, cuDevice);
	//cuRes = cuCtxCreate(&cuContext, 0, cuDevice); //Doesn't have an effect
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_GET_CONTEXT;
	}
	
	//consoleWriteLineSlow("Cuda Context Init");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	cuRes = cuCtxPushCurrent(cuContext);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_PUSH_CONTEXT;
	}
	
	size_t cudaBytes = 0;
	//cuRes = cuCtxGetLimit(&cudaBytes, CU_LIMIT_STACK_SIZE);
	//if (cuRes != CUDA_SUCCESS) {
	//	cuExtraInfo = (int) cuRes;
	//	return ERROR_CUDA_CANNOT_GET_LIMIT;
	//}
	//consoleWriteLineWithNumberFast("Cuda Limit: ", 12, cudaBytes, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	cudaBytes = 0;
	cuRes = cuCtxSetLimit(CU_LIMIT_STACK_SIZE, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	cuRes = cuCtxSetLimit(CU_LIMIT_PRINTF_FIFO_SIZE, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	cuRes = cuCtxSetLimit(CU_LIMIT_MALLOC_HEAP_SIZE, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	//cuRes = cuCtxSetLimit(CU_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT, cudaBytes);
	//if (cuRes != CUDA_SUCCESS) {
	//	cuExtraInfo = (int) cuRes;
	//	return ERROR_CUDA_CANNOT_SET_LIMIT;
	//}
	cuRes = cuCtxSetLimit(CU_LIMIT_DEV_RUNTIME_SYNC_DEPTH, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	
	//cuRes = cuCtxGetLimit(&cudaBytes, CU_LIMIT_STACK_SIZE);
	//if (cuRes != CUDA_SUCCESS) {
	//	cuExtraInfo = (int) cuRes;
	//	return ERROR_CUDA_CANNOT_GET_LIMIT;
	//}
	//consoleWriteLineWithNumberFast("Cuda Limit: ", 12, cudaBytes, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	//size_t cudaFreeMem = 0;
	//size_t cudaTotalMem = 0;
	//cuRes = cuMemGetInfo(&cudaFreeMem, &cudaTotalMem);
	//if (cuRes != CUDA_SUCCESS) {
	//	return ERROR_NVENC_EXTRA_INFO;
	//}
	//consoleWriteLineWithNumberFast("Cuda Free:  ", 12, cudaFreeMem, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Cuda Total: ", 12, cudaTotalMem, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	//consoleWriteLineSlow("Cuda Context Modify");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	CUDA_EXTERNAL_MEMORY_HANDLE_DESC extMemHandle = {};
	extMemHandle.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
	extMemHandle.handle.win32.handle = desktopDuplicationTextureConvertedHandle;
	extMemHandle.handle.win32.name = NULL;//vulkanMemGPUexportStr; //Doesn't work right?
	extMemHandle.size = vulkanMemGPUexportSize;
	extMemHandle.flags = CUDA_EXTERNAL_MEMORY_DEDICATED; //correct based on vulkan property
	
	cuRes = cuImportExternalMemory(&cuExtMem, &extMemHandle);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_IMPORT_MEMORY;
	}
	//consoleWriteLineWithNumberFast("Imported Mem: ", 14, (uint64_t) cuExtMem, NUM_FORMAT_FULL_HEXADECIMAL);
	
	CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC extMemArray = {};
	extMemArray.offset = 0;
	extMemArray.arrayDesc.Width = desktopDuplicationWidth;
	extMemArray.arrayDesc.Height = desktopDuplicationHeight * 3;
	extMemArray.arrayDesc.Depth = 0;
	extMemArray.arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT16; //Lossless format matching the Vulkan Exported Memory 
	extMemArray.arrayDesc.NumChannels = 1;
	extMemArray.arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST; //Manditory for NvEnc
	extMemArray.numLevels = 1;
	
	cuRes = cuExternalMemoryGetMappedMipmappedArray(&cuExtMipArray, cuExtMem, &extMemArray);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_MAP_MEMORY;
	}
	
	cuRes = cuMipmappedArrayGetLevel(&cuExtArray, cuExtMipArray, 0);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_GET_ARRAY;
	}
	
	cuRes = cuCtxPopCurrent(NULL);
	if (cuRes != CUDA_SUCCESS) {
		cuExtraInfo = (int) cuRes;
		return ERROR_CUDA_CANNOT_POP_CONTEXT;
	}
	
	//consoleWriteLineSlow("Cuda External Mem Stuff");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	//NVIDIA Encoder Setup
	nvEncFunList.version = NV_ENCODE_API_FUNCTION_LIST_VER;
	nvEncFunList.reserved = 0;
	NVENCSTATUS nvEncRes = NvEncodeAPICreateInstance(&nvEncFunList);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_CREATE_INSTANCE;
	}
	
	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS nvEncSessionParams = {};
	nvEncSessionParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
	nvEncSessionParams.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
	nvEncSessionParams.device = (void*) cuContext;
	nvEncSessionParams.apiVersion = NVENCAPI_VERSION;
	
	nvEncRes = nvEncFunList.nvEncOpenEncodeSessionEx(&nvEncSessionParams, &nvEncoder);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_OPEN_SESSION;
	}
	
	
	GUID nvEncGUIDs[32]; //32 is enough for all possible GUIDs for now
	uint32_t nvEncGUIDcount = 0;
	nvEncRes = nvEncFunList.nvEncGetEncodeGUIDs(nvEncoder, nvEncGUIDs, 32, &nvEncGUIDcount);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_ENCODE_GUIDS;
	}
	//consoleWriteLineWithNumberFast("Number: ", 8, nvEncGUIDcount, NUM_FORMAT_UNSIGNED_INTEGER);
	
	GUID nvEncChosenGUID = {};
	for (uint32_t i=0; i<nvEncGUIDcount; i++) {
		GUID nvCheckGUID = nvEncGUIDs[i];
		//consoleWriteLineWithNumberFast("GUID: ", 6, nvCheckGUID.Data1, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("GUID: ", 6, nvCheckGUID.Data2, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		if (nvCheckGUID.Data1 == NV_ENC_CODEC_HEVC_GUID.Data1) {
			//consoleWriteLineWithNumberFast("Number: ", 8, i, NUM_FORMAT_UNSIGNED_INTEGER);
			nvEncChosenGUID.Data1 = nvCheckGUID.Data1;
			nvEncChosenGUID.Data2 = nvCheckGUID.Data2;
			nvEncChosenGUID.Data3 = nvCheckGUID.Data3;
			for (uint32_t d=0; d<8; d++) {
				nvEncChosenGUID.Data4[d] = nvCheckGUID.Data4[d];
			}
			break;
		}
	}
	if (nvEncChosenGUID.Data1 != NV_ENC_CODEC_HEVC_GUID.Data1) { //HEVC Encoding
		return ERROR_NVENC_NO_HEVC;
	}
	
	
	uint32_t nvEncProfiles = 0;
	nvEncRes = nvEncFunList.nvEncGetEncodeProfileGUIDs(nvEncoder, nvEncChosenGUID, nvEncGUIDs, 32, &nvEncProfiles);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_ENCODE_PROFILES;
	}
	//consoleWriteLineWithNumberFast("Number: ", 8, nvEncProfiles, NUM_FORMAT_UNSIGNED_INTEGER);
	
	GUID nvEncProfileGUID = {};
	for (uint32_t i=0; i<nvEncProfiles; i++) {
		GUID nvCheckGUID = nvEncGUIDs[i];
		//consoleWriteLineWithNumberFast("GUID: ", 6, nvCheckGUID.Data1, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("GUID: ", 6, nvCheckGUID.Data2, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		if (nvCheckGUID.Data1 == NV_ENC_HEVC_PROFILE_FREXT_GUID.Data1) {
			//consoleWriteLineWithNumberFast("Number: ", 8, i, NUM_FORMAT_UNSIGNED_INTEGER);
			nvEncProfileGUID.Data1 = nvCheckGUID.Data1;
			nvEncProfileGUID.Data2 = nvCheckGUID.Data2;
			nvEncProfileGUID.Data3 = nvCheckGUID.Data3;
			for (uint32_t d=0; d<8; d++) {
				nvEncProfileGUID.Data4[d] = nvCheckGUID.Data4[d];
			}
			break;
		}
	}
	if (nvEncProfileGUID.Data1 != NV_ENC_HEVC_PROFILE_FREXT_GUID.Data1) { //For Lossless 10-bit HEVC
		return ERROR_NVENC_NO_HEVC_PROFILE;
	}
	
	
	uint32_t nvEncPresets = 0;
	nvEncRes = nvEncFunList.nvEncGetEncodePresetGUIDs(nvEncoder, nvEncChosenGUID, nvEncGUIDs, 32, &nvEncPresets);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_ENCODE_PRESETS;
	}
	//consoleWriteLineWithNumberFast("Number: ", 8, nvEncPresets, NUM_FORMAT_UNSIGNED_INTEGER);
	
	GUID nvEncPresetGUID = {};
	for (uint32_t i=0; i<nvEncPresets; i++) {
		GUID nvCheckGUID = nvEncGUIDs[i];
		//consoleWriteLineWithNumberFast("GUID: ", 6, nvCheckGUID.Data1, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("GUID: ", 6, nvCheckGUID.Data2, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		if (nvCheckGUID.Data1 == NV_ENC_PRESET_P1_GUID.Data1) {
			//consoleWriteLineWithNumberFast("Number: ", 8, i, NUM_FORMAT_UNSIGNED_INTEGER);
			nvEncPresetGUID.Data1 = nvCheckGUID.Data1;
			nvEncPresetGUID.Data2 = nvCheckGUID.Data2;
			nvEncPresetGUID.Data3 = nvCheckGUID.Data3;
			for (uint32_t d=0; d<8; d++) {
				nvEncPresetGUID.Data4[d] = nvCheckGUID.Data4[d];
			}
			break;
		}
	}
	if (nvEncPresetGUID.Data1 != NV_ENC_PRESET_P1_GUID.Data1) {
		return ERROR_NVENC_NO_PRESET;
	}
	
	NV_ENC_PRESET_CONFIG nvPresetConfig = {};
	nvPresetConfig.version = NV_ENC_PRESET_CONFIG_VER;
	nvPresetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
	nvEncRes = nvEncFunList.nvEncGetEncodePresetConfigEx(nvEncoder, nvEncChosenGUID, nvEncPresetGUID, NV_ENC_TUNING_INFO_LOSSLESS, &nvPresetConfig);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_PRESET_CONFIG;
	}	
	
	
	NV_ENC_BUFFER_FORMAT nvEncInFmts[16]; //16 should be enough for all formats
	uint32_t nvEncInFmtCount = 0;
	nvEncRes = nvEncFunList.nvEncGetInputFormats(nvEncoder, nvEncChosenGUID, nvEncInFmts, 16, &nvEncInFmtCount);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_INPUT_FORMATS;
	}
	//consoleWriteLineWithNumberFast("Number: ", 8, nvEncInFmts, NUM_FORMAT_UNSIGNED_INTEGER);
	
	NV_ENC_BUFFER_FORMAT nvEncChosenFormat = NV_ENC_BUFFER_FORMAT_UNDEFINED;
	NV_ENC_BUFFER_FORMAT nvEncDesiredFormat = NV_ENC_BUFFER_FORMAT_YUV444_10BIT;
	for (uint32_t i=0; i<nvEncInFmtCount; i++) {
		if (nvEncInFmts[i] == nvEncDesiredFormat) {
			nvEncChosenFormat = nvEncDesiredFormat;
			break;
		}
	}
	if (nvEncChosenFormat != nvEncDesiredFormat) {
		return ERROR_NVENC_NO_LOSSLESS_INPUT_FORMAT;
	}
	
	NV_ENC_CAPS_PARAM nvEncCapability = {};
	nvEncCapability.version = NV_ENC_CAPS_PARAM_VER;
	nvEncCapability.capsToQuery = NV_ENC_CAPS_NUM_MAX_BFRAMES;
	int nvEncCapsVal = 0;
	nvEncRes = nvEncFunList.nvEncGetEncodeCaps(nvEncoder, nvEncChosenGUID, &nvEncCapability, &nvEncCapsVal);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_CAPABILITY;
	}
	//consoleWriteLineWithNumberFast("Max B-Frames: ", 14, nvEncCapsVal, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//consoleWriteLineSlow("Nvidia Setup");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	NV_ENC_INITIALIZE_PARAMS nvEncParams = {0}; //Expects things to be set to 0
	nvEncParams.version = NV_ENC_INITIALIZE_PARAMS_VER;
	nvEncParams.encodeGUID.Data1 = nvEncChosenGUID.Data1;
	nvEncParams.encodeGUID.Data2 = nvEncChosenGUID.Data2;
	nvEncParams.encodeGUID.Data3 = nvEncChosenGUID.Data3;
	nvEncParams.presetGUID.Data1 = nvEncPresetGUID.Data1;
	nvEncParams.presetGUID.Data2 = nvEncPresetGUID.Data2;
	nvEncParams.presetGUID.Data3 = nvEncPresetGUID.Data3;
	for (uint32_t d=0; d<8; d++) {
		nvEncParams.encodeGUID.Data4[d] = nvEncChosenGUID.Data4[d];
		nvEncParams.presetGUID.Data4[d] = nvEncPresetGUID.Data4[d];
	}
	nvEncParams.encodeWidth = desktopDuplicationWidth;
	nvEncParams.encodeHeight = desktopDuplicationHeight;
	uint32_t gcd = greatestCommonDivisor(desktopDuplicationWidth, desktopDuplicationHeight);
	nvEncParams.darWidth = desktopDuplicationWidth / gcd; //16;
	nvEncParams.darHeight = desktopDuplicationHeight / gcd; //9;
	
	//consoleWriteLineWithNumberFast("DAR Width:  ", 12, nvEncParams.darWidth, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("DAR Height: ", 12, nvEncParams.darHeight, NUM_FORMAT_UNSIGNED_INTEGER);
	
	
	nvEncParams.frameRateNum = 60;
	nvEncParams.frameRateDen = 1;
	nvEncParams.enableEncodeAsync = 0; //Lots more work to enable Async and probably not worth it
	nvEncParams.enablePTD = 1; //Enabling the picture type decision to be made by the encoder
	nvEncParams.reportSliceOffsets = 0;
	nvEncParams.enableSubFrameWrite = 0;
	nvEncParams.enableExternalMEHints = 0;
	nvEncParams.enableMEOnlyMode = 0;
	nvEncParams.enableWeightedPrediction = 0;
	nvEncParams.splitEncodeMode = 0; //Not certain
	nvEncParams.enableOutputInVidmem = 0;
	nvEncParams.enableReconFrameOutput = 0;
	nvEncParams.enableOutputStats = 0;
	nvEncParams.reservedBitFields = 0;
	nvEncParams.privDataSize = 0;
	nvEncParams.privData = NULL;
	nvEncParams.privDataSize = 0;
	
	
	//Manual Adjust of Preset Parameters
	nvPresetConfig.presetCfg.profileGUID.Data1 = nvEncProfileGUID.Data1;
	nvPresetConfig.presetCfg.profileGUID.Data2 = nvEncProfileGUID.Data2;
	nvPresetConfig.presetCfg.profileGUID.Data3 = nvEncProfileGUID.Data3;
	for (uint32_t d=0; d<8; d++) {
		nvPresetConfig.presetCfg.profileGUID.Data4[d] = nvEncProfileGUID.Data4[d];
	}
	
	//consoleWriteLineWithNumberFast("Rate Control: ", 14, nvPresetConfig.presetCfg.gopLength, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Rate Control: ", 14, nvPresetConfig.presetCfg.frameIntervalP, NUM_FORMAT_UNSIGNED_INTEGER);
	nvPresetConfig.presetCfg.gopLength = NVENC_INFINITE_GOPLENGTH; //For realtime encoding
	nvPresetConfig.presetCfg.frameIntervalP = 1;
	
	//nvPresetConfig.presetCfg.monoChromeEncoding = 0;
	//nvPresetConfig.presetCfg.frameFieldMode = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;
	//nvPresetConfig.presetCfg.mvPrecision = NV_ENC_MV_PRECISION_DEFAULT;
	
	nvEncParams.encodeConfig = &nvPresetConfig.presetCfg;
	//consoleWriteLineWithNumberFast("Rate Control: ", 14, nvPresetConfig.presetCfg.rcParams.rateControlMode, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("AQ Enable: ", 11, nvPresetConfig.presetCfg.rcParams.enableAQ, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Low Delay: ", 11, nvPresetConfig.presetCfg.rcParams.lowDelayKeyFrameScale, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//consoleWriteLineWithNumberFast("SPSPPS Disable: ", 16, nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.disableSPSPPS, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("SPSPPS Repeat:  ", 16, nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.repeatSPSPPS, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Enable Intra:   ", 16, nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.enableIntraRefresh, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Intra Period:   ", 16, nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.intraRefreshPeriod, NUM_FORMAT_UNSIGNED_INTEGER);
	
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.chromaFormatIDC = 3; //1 for 4:2:0, 3 for 4:4:4 Makes it lossless
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 2;
	
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.videoSignalTypePresentFlag = 1;
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.videoFormat = NV_ENC_VUI_VIDEO_FORMAT_COMPONENT; //?
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.videoFullRangeFlag = 1;
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.colourDescriptionPresentFlag = 1;
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.colourPrimaries = NV_ENC_VUI_COLOR_PRIMARIES_BT709;
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.transferCharacteristics = NV_ENC_VUI_TRANSFER_CHARACTERISTIC_BT709;
	nvPresetConfig.presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.colourMatrix = NV_ENC_VUI_MATRIX_COEFFS_BT709;
	
	
	nvEncParams.maxEncodeWidth = desktopDuplicationWidth; //0?
	nvEncParams.maxEncodeHeight = desktopDuplicationHeight; //0?
	
	nvEncParams.tuningInfo = NV_ENC_TUNING_INFO_LOSSLESS;
	nvEncParams.bufferFormat = nvEncChosenFormat; //Only used when device is DX12
	
	nvEncRes = nvEncFunList.nvEncInitializeEncoder(nvEncoder, &nvEncParams);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_INITIALIZE;
	}
	
	//consoleWriteLineSlow("Nvidia Init Encoder");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	NV_ENC_REGISTER_RESOURCE nvEncInputResource = {0};
	nvEncInputResource.version = NV_ENC_REGISTER_RESOURCE_VER;
	nvEncInputResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY;
	nvEncInputResource.width = desktopDuplicationWidth;
	nvEncInputResource.height = desktopDuplicationHeight;
	nvEncInputResource.pitch = desktopDuplicationWidth * 2;
	
	nvEncInputResource.subResourceIndex = 0; //0 for CUDA
	nvEncInputResource.resourceToRegister = (void*) cuExtArray;
	
	nvEncInputResource.bufferFormat = nvEncChosenFormat;
	nvEncInputResource.bufferUsage = NV_ENC_INPUT_IMAGE;
	
	nvEncInputResource.pInputFencePoint = NULL; //Used for DX12
	
	nvEncRes = nvEncFunList.nvEncRegisterResource(nvEncoder, &nvEncInputResource);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_REGISTER_RES;
	}
	
	NV_ENC_MAP_INPUT_RESOURCE nvEncMappedInput = {0};
	nvEncMappedInput.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
	nvEncMappedInput.registeredResource = nvEncInputResource.registeredResource;
	
	nvEncRes = nvEncFunList.nvEncMapInputResource(nvEncoder, &nvEncMappedInput);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_MAP_RES;
	}
	
	//consoleWriteLineSlow("Nvidia Map Input");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	//Create Bitstream Buffer Output ...Should auto allocate based on image dimensons and max B frames
	//Might need to create multiple buffers depending on how it will be encoded
	nvEncBitstreamBuff0.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
	nvEncRes = nvEncFunList.nvEncCreateBitstreamBuffer(nvEncoder, &nvEncBitstreamBuff0);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_CREATE_BITSTREAM;
	}
	
	//Extra safe
	nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff0.bitstreamBuffer);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_UNLOCK_BITSTREAM;
	}
	
	nvEncBitstreamBuff1.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
	nvEncRes = nvEncFunList.nvEncCreateBitstreamBuffer(nvEncoder, &nvEncBitstreamBuff1);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_CREATE_BITSTREAM;
	}
	
	//Extra safe
	nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff1.bitstreamBuffer);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_UNLOCK_BITSTREAM;
	}
	
	//consoleWriteLineSlow("Nvidia Bitstreams Create");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	nvEncPicParams.version = NV_ENC_PIC_PARAMS_VER;
	nvEncPicParams.inputWidth = desktopDuplicationWidth;
	nvEncPicParams.inputHeight = desktopDuplicationHeight;
	nvEncPicParams.inputPitch = nvEncPicParams.inputWidth; //Double Check Later
	nvEncPicParams.encodePicFlags = 0;//NV_ENC_PIC_FLAG_FORCEINTRA; //Needed at start?
	nvEncPicParams.frameIdx = 0; //Optional
	
	nvEncPicParams.inputTimeStamp = 0; //Figure Out Later
	nvEncPicParams.inputDuration = 0; //Figure Out Later
	
	nvEncPicParams.inputBuffer = nvEncMappedInput.mappedResource;
	nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
	nvEncPicParams.completionEvent = NULL;
	
	nvEncPicParams.bufferFmt = nvEncChosenFormat;//nvEncMappedInput.mappedBufferFmt; //Check Values are equal in future
	
	nvEncPicParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
	nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR;
	
	NV_ENC_CODEC_PIC_PARAMS nvEncSpecificPicParams = {};
	nvEncSpecificPicParams.hevcPicParams.displayPOCSyntax = 1; //Picture Order Count...
	nvEncSpecificPicParams.hevcPicParams.refPicFlag = 0; //Double Check Later on reference frames vs intra / IDR
	nvEncPicParams.codecPicParams = nvEncSpecificPicParams;
	
	return 0;
}



int desktopDuplicationStart(size_t shaderSize, uint32_t* shaderData, void** lutBufferPtr) {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	if (desktopDuplicationState > DESKDUPL_STATE_UNDEFINED) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	int error = desktopDuplicationSetupDX11();
	if (error != 0) {
		return error;
	}
	
	//consoleWriteLineSlow("DirectX 11 Setup");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	error = desktopDuplicationSetupVulkan(shaderSize, shaderData);
	if (error != 0) {
		return error;
	}
	
	//consoleWriteLineSlow("Vulkan Setup");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	error = desktopDuplicationSetupNvEnc();
	if (error != 0) {
		return error;
	}
	
	//consoleWriteLineSlow("Encoder Setup");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	VkResult result = vkMapMemory(vulkanDevice, vulkanMemCPUaccess, 0, VK_WHOLE_SIZE, 0, lutBufferPtr);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_MAP_FAILED;
	}
	
	desktopDuplicationState = DESKDUPL_STATE_LUT_LOAD;
	return 0;
}


int desktopDuplicationLoadLUT() {
	if (desktopDuplicationState != DESKDUPL_STATE_LUT_LOAD) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	//The staging buffer was populated with the LUT data
	//time to transfer the data into the GPU exclusive memory buffer
	vkUnmapMemory(vulkanDevice, vulkanMemCPUaccess);
	
	//Copy from lut data from staging to LUT
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = NULL;
	submitInfo.pWaitDstStageMask = NULL;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vulkanComputeCommandBuffers[0];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = NULL;

	vkQueueSubmit(vulkanComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vulkanComputeQueue); //Fence in future
	
	desktopDuplicationState = DESKDUPL_STATE_STARTED;
	return 0;
}


int desktopDuplicationTestFrame(void* rawARGBfilePtr, void* bitstreamFilePtr) {
	if (desktopDuplicationState != DESKDUPL_STATE_STARTED) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	//Copy Image to Staging
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = NULL;
	submitInfo.pWaitDstStageMask = NULL;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vulkanComputeCommandBuffers[1];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = NULL;
	
	vkQueueSubmit(vulkanComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vulkanComputeQueue); //Fence in future
	
	void* imgData = NULL;
	VkResult result = vkMapMemory(vulkanDevice, vulkanMemCPUaccess, 0, VK_WHOLE_SIZE, 0, &imgData);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_MAP_FAILED;
	}
	
	//Write Image Data to file
	int error = writeFile(rawARGBfilePtr, imgData, 8294400); 
	if (error != 0) {
		return error;
	}
	
	vkUnmapMemory(vulkanDevice, vulkanMemCPUaccess);
	
	uint64_t sTime = getCurrentTime();
	submitInfo.pCommandBuffers = &vulkanComputeCommandBuffers[4];
	vkQueueSubmit(vulkanComputeQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vulkanComputeQueue); //Fence in future
	uint64_t eTime = getCurrentTime();
	uint64_t uTime = getDiffTimeMicroseconds(sTime, eTime);
	consoleWriteLineWithNumberFast("Microseconds: ", 14, uTime, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//NvEnc Run
	NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = nvEncRes;
		return ERROR_NVENC_EXTRA_INFO;
	}
	
	//Lock Bitstream To Read Data
	NV_ENC_LOCK_BITSTREAM nvEncBitstreamLock = {0};
	nvEncBitstreamLock.version = NV_ENC_LOCK_BITSTREAM_VER;
	nvEncBitstreamLock.doNotWait = 0;
	nvEncBitstreamLock.getRCStats = 0;
	nvEncBitstreamLock.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
	nvEncBitstreamLock.sliceOffsets = NULL;
	
	nvEncRes = nvEncFunList.nvEncLockBitstream(nvEncoder, &nvEncBitstreamLock);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = nvEncRes;
		return ERROR_NVENC_EXTRA_INFO;
	}
	
	//Write output to file
	error = writeFile(bitstreamFilePtr, nvEncBitstreamLock.bitstreamBufferPtr, nvEncBitstreamLock.bitstreamSizeInBytes); 
	
	nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff0.bitstreamBuffer);
	if (nvEncRes != NV_ENC_SUCCESS) {
		nvEncExtraInfo = nvEncRes;
		return ERROR_NVENC_EXTRA_INFO;
	}
	
	
	return 0;
}

//Get Next Desktop Image
static int desktopDuplicationGetNextDesktopImage(uint64_t startTime, uint64_t errorTime, uint64_t* presentationTime) {
	uint64_t acquireWaitTimeInMS = getDiffTimeMilliseconds(startTime, errorTime);
	
	IDXGIResource* desktopResourcePtr = NULL;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;
	
	uint64_t acquiredDesktopImage = 0;
	while (acquiredDesktopImage == 0) {
		//Release Frame First
		HRESULT hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
		if (hrRes != S_OK) {
			if (hrRes != DXGI_ERROR_INVALID_CALL) {
				desktopDuplicationExtraInfo = (int) hrRes;
				return ERROR_DESKDUPL_RELEASE_FAILED;
			}
			//Frame already released
		}
		
		hrRes = desktopDuplicationPtr->lpVtbl->AcquireNextFrame(desktopDuplicationPtr, acquireWaitTimeInMS, &frameInfo, &desktopResourcePtr);
		if (hrRes == S_OK) { //Acquired Something (Might just be mouse stuff)
			uint64_t framePresentTime = (uint64_t) frameInfo.LastPresentTime.QuadPart;
			if(framePresentTime != 0) { //Actually acquired image
				*presentationTime = framePresentTime;
				acquiredDesktopImage = 1; //Could return right here!
			}
			else { //probably acquired mouse change info... need to try again
				uint64_t currentTime = getCurrentTime();
				acquireWaitTimeInMS = getDiffTimeMilliseconds(currentTime, errorTime);
			}
		}
		else { //Unfortunately failed to acquire next Desktop Image
			if (hrRes == DXGI_ERROR_WAIT_TIMEOUT) { //Just Timed Out
				return ERROR_DESKDUPL_ACQUIRE_TIMEOUT;
			}
			else {
				desktopDuplicationExtraInfo = (int) hrRes;
				return ERROR_DESKDUPL_ACQUIRE_FAILED;
			}
		}
	}
	
	return 0;
}


int desktopDuplicationGetFrame() {
	UINT64 mutexKey = 0; 
	
	//See if can acquire while its otherwise acquired (probably not)
	HRESULT hrRes = desktopDuplicationKeyedMutex->lpVtbl->AcquireSync(desktopDuplicationKeyedMutex, mutexKey, 0);
	if (hrRes != S_OK) {
		consoleWriteLineSlow("Error Acquiring Keyed Mutex");
		//if (hrRes != WAIT_ABANDONED) {
		//	return ERROR_DESKDUPL_EXTRA_INFO;
		//}
	}
	else {
		consoleWriteLineSlow("Acquired Keyed Mutex");
		hrRes = desktopDuplicationKeyedMutex->lpVtbl->ReleaseSync(desktopDuplicationKeyedMutex, mutexKey);
		if (hrRes != S_OK) {
			consoleWriteLineSlow("Error Releasing Keyed Mutex");
		}
	}
	
	//ReleaseFrame
	hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
	if (hrRes != S_OK) {
		if (hrRes != DXGI_ERROR_INVALID_CALL) {
			desktopDuplicationExtraInfo = (int) hrRes;
			return ERROR_DESKDUPL_RELEASE_FAILED;
		}
		//Frame already released
	}
	
	
	hrRes = desktopDuplicationKeyedMutex->lpVtbl->AcquireSync(desktopDuplicationKeyedMutex, mutexKey, 0);
	if (hrRes != S_OK) {
		consoleWriteLineSlow("Error Acquiring Keyed Mutex");
		//if (hrRes != WAIT_ABANDONED) {
		//	return ERROR_DESKDUPL_EXTRA_INFO;
		//}
	}
	else {
		consoleWriteLineSlow("Acquired Keyed Mutex");
		hrRes = desktopDuplicationKeyedMutex->lpVtbl->ReleaseSync(desktopDuplicationKeyedMutex, mutexKey);
		if (hrRes != S_OK) {
			consoleWriteLineSlow("Error Releasing Keyed Mutex");
		}
	}
	
	uint64_t presentationTime = 0;
	uint64_t currentTime = getCurrentTime();
	uint64_t errorTime = getEndTimeFromMilliDiff(currentTime, 2000); //Wait 2 seconds
	int error = desktopDuplicationGetNextDesktopImage(currentTime, errorTime, &presentationTime);
	if (error != 0) {
		return error;
	}
	
	return 0;
}

static uint64_t ddLastPresentationTime = 0;
static uint64_t ddCurrentPresentationTime = 0;
static uint64_t ddCurrentFrame = 0;
static uint64_t ddCurrentFrameState = 0;
static uint64_t ddCounterIDRreset = 0;
static uint64_t ddCounterIDR = 0;
static uint64_t ddFrameIntervalTime = 0;
static uint64_t ddFirstFrameStartTime = 0;
static uint64_t ddFirstAcquireStartTime = 0;

static VkWin32KeyedMutexAcquireReleaseInfoKHR ddTextureMutex = {};
static VkSubmitInfo ddComputeSubmitInfo = {};
static NV_ENC_LOCK_BITSTREAM ddEncodeBitstreamLock0 = {};
static NV_ENC_LOCK_BITSTREAM ddEncodeBitstreamLock1 = {};

static uint64_t ddRepeatFrame = 0;
static uint64_t ddFrameNumInvalidStat = 0;

static uint64_t ddLastAcquireTime = 0;
static uint64_t ddLatencySum = 0;
static uint64_t ddLatencyCount = 0;

static uint64_t ddComputeLatencySum = 0;
static uint64_t ddComputeLatencyCount = 0;
static uint64_t ddComputeCheck = 0;

static HANDLE ddThreadEndEvent = NULL;
static HANDLE ddEncodeEvent = NULL;
static HANDLE ddLockEvent = NULL;
static HANDLE ddEncodeLockThreadHandle = NULL;

static DWORD WINAPI ddEncodeLockThread(LPVOID lpParam) {
	uint64_t bitTest = 0;
	NV_ENC_LOCK_BITSTREAM* bitstreamToLock = &ddEncodeBitstreamLock0;
	DWORD stopThread = WaitForSingleObject(ddThreadEndEvent, 0);
	while (stopThread != 0) {
		DWORD waitRes = WaitForSingleObject(ddEncodeEvent, INFINITE);
		if (waitRes != 0) {
			return waitRes;
		}
		NVENCSTATUS nvEncRes = nvEncFunList.nvEncLockBitstream(nvEncoder, bitstreamToLock);
		if (nvEncRes != NV_ENC_SUCCESS) {
			return nvEncRes;
		}
		BOOL setRes = SetEvent(ddLockEvent);
		if (setRes == 0) {
			return ERROR_GET_EXTRA_INFO;
		}
		bitTest ^= 1; //XOR with 1
		if (bitTest == 0) {
			bitstreamToLock = &ddEncodeBitstreamLock0;
		}
		else {
			bitstreamToLock = &ddEncodeBitstreamLock1;
		}
		stopThread = WaitForSingleObject(ddThreadEndEvent, 0);
	}	
	
	return stopThread;
}


static uint64_t ddWriteOffset = 0;
static uint64_t ddWaitOnEncoding = 0;
static HANDLE ddOverlapped0Event = NULL;
static HANDLE ddOverlapped1Event = NULL;
static OVERLAPPED ddOverlapped0 = {};
static OVERLAPPED ddOverlapped1 = {};


int desktopDuplicationSetFrameRate(uint64_t fps) {
	if (desktopDuplicationState != DESKDUPL_STATE_STARTED) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	uint64_t acquireWaitTimeInMS = 2000 / fps; //Error thrown if not here (truncate desired here)
	uint64_t currentTime = getCurrentTime();
	uint64_t errorTime = getEndTimeFromMilliDiff(currentTime, acquireWaitTimeInMS);
	
	int error = desktopDuplicationGetNextDesktopImage(currentTime, errorTime, &ddLastPresentationTime);
	if (error != 0) {
		return error;
	}
	
	//Make the Compute Setup and Run Once
	uint64_t mutexKey = 0;
	uint32_t timeout = 1;
	
	ddTextureMutex.sType = VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR;
	ddTextureMutex.pNext = NULL;
	ddTextureMutex.acquireCount = 1;
	ddTextureMutex.pAcquireSyncs = &vulkanMemGPUimport;
	ddTextureMutex.pAcquireKeys = &mutexKey;
	ddTextureMutex.pAcquireTimeouts = &timeout;
	ddTextureMutex.releaseCount = 1;
	ddTextureMutex.pReleaseSyncs = &vulkanMemGPUimport;
	ddTextureMutex.pReleaseKeys = &mutexKey;
	
	ddComputeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	ddComputeSubmitInfo.pNext = NULL;
	ddComputeSubmitInfo.waitSemaphoreCount = 0;
	ddComputeSubmitInfo.pWaitSemaphores = NULL;
	ddComputeSubmitInfo.pWaitDstStageMask = NULL;
	ddComputeSubmitInfo.commandBufferCount = 1;
	ddComputeSubmitInfo.pCommandBuffers = &vulkanComputeCommandBuffers[4];
	ddComputeSubmitInfo.signalSemaphoreCount = 0;//1;
	ddComputeSubmitInfo.pSignalSemaphores = NULL;//&vulkanComputeSemaphore;
	vkQueueSubmit(vulkanComputeQueue, 1, &ddComputeSubmitInfo, VK_NULL_HANDLE);
	
	ddEncodeBitstreamLock0.version = NV_ENC_LOCK_BITSTREAM_VER;
	ddEncodeBitstreamLock0.doNotWait = 0; //Has to be 0 for synchronous mode... tested and documented
	ddEncodeBitstreamLock0.getRCStats = 0;
	ddEncodeBitstreamLock0.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
	ddEncodeBitstreamLock0.sliceOffsets = NULL;
	
	ddEncodeBitstreamLock1.version = NV_ENC_LOCK_BITSTREAM_VER;
	ddEncodeBitstreamLock1.doNotWait = 0;
	ddEncodeBitstreamLock1.getRCStats = 0;
	ddEncodeBitstreamLock1.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
	ddEncodeBitstreamLock1.sliceOffsets = NULL;
	
	//Setup time boundries
	ddCurrentPresentationTime = 0;
	ddCurrentFrame = 0;
	ddCurrentFrameState = 0;
	
	nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA;
	ddCounterIDRreset = fps * 3;
	ddCounterIDR = ddCounterIDRreset;
	ddFrameIntervalTime = timeCounterFrequency / fps;
	ddFirstFrameStartTime = ddLastPresentationTime + (ddFrameIntervalTime >> 1);
	ddFirstAcquireStartTime = getEndTimeFromMicroDiff(ddFirstFrameStartTime, 500);
	//ddFirstComputeFailTime = ddFirstFrameStartTime + ((ddFrameIntervalTime * 9) / 8);
	//ddFirstEncodeFailTime  = ddFirstFrameStartTime + ((ddFrameIntervalTime * 3) / 2);
	
	//consoleWriteLineWithNumberFast("Time 0: ", 8, getDiffTimeMilliseconds(ddFirstFrameStartTime, ddFirstFrameStartTime+ddFrameIntervalTime), NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Time 1: ", 8, getDiffTimeMicroseconds(ddFirstFrameStartTime, ddFirstAcquireStartTime), NUM_FORMAT_UNSIGNED_INTEGER);
	//currentTime = getCurrentTime();
	//consoleWriteLineWithNumberFast("Time 3: ", 8, getDiffTimeMilliseconds(ddFirstFrameStartTime, currentTime), NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleBufferFlush();
	
	//Setup Encode Frame Values
	ddRepeatFrame = 0;
	ddWaitOnEncoding = 0;
	ddFrameNumInvalidStat = 0;
	ddLatencySum = 0;
	ddLatencyCount = 0;
	
	ddComputeLatencySum = 0;
	ddComputeLatencyCount = 0;
	
	ddThreadEndEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (ddThreadEndEvent == NULL) {
		return ERROR_EVENT_NOT_CREATED;
	}
	ddEncodeEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (ddEncodeEvent == NULL) {
		return ERROR_EVENT_NOT_CREATED;
	}
	ddLockEvent = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (ddLockEvent == NULL) {
		return ERROR_EVENT_NOT_CREATED;
	}
	
	ddEncodeLockThreadHandle = CreateThread(NULL, 1, ddEncodeLockThread, NULL, 0, NULL);
	if (ddEncodeLockThreadHandle == NULL) {
		return ERROR_THREAD_NOT_CREATED;
	}
	
	ddOverlapped0Event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (ddOverlapped0Event == NULL) {
		return ERROR_EVENT_NOT_CREATED;
	}
	ddOverlapped1Event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (ddOverlapped1Event == NULL) {
		return ERROR_EVENT_NOT_CREATED;
	}
	
	ddOverlapped0.Internal = 0;
	ddOverlapped0.InternalHigh = 0;
	ddOverlapped0.Offset = 0xFFFFFFFF;
	ddOverlapped0.OffsetHigh = 0xFFFFFFFF;
	ddOverlapped0.Pointer = 0;
	ddOverlapped0.hEvent = ddOverlapped0Event;
	
	ddOverlapped1.Internal = 0;
	ddOverlapped1.InternalHigh = 0;
	ddOverlapped1.Offset = 0xFFFFFFFF;
	ddOverlapped1.OffsetHigh = 0xFFFFFFFF;
	ddOverlapped1.Pointer = 0;
	ddOverlapped1.hEvent = ddOverlapped1Event;
	
	
	//Wait on the residule compute run and then release
	vkQueueWaitIdle(vulkanComputeQueue);
	HRESULT hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
	if (hrRes != S_OK) {
		if (hrRes != DXGI_ERROR_INVALID_CALL) {
			desktopDuplicationExtraInfo = (int) hrRes;
			return ERROR_DESKDUPL_RELEASE_FAILED;
		}
		//Frame already released
	}
	
	currentTime = getCurrentTime();
	//uint64_t diffTimeMS = getDiffTimeMilliseconds(ddFirstFrameStartTime, currentTime);
	//consoleWriteLineWithNumberFast("Time 4: ", 8, diffTimeMS, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleBufferFlush();
	
	if (currentTime >= ddFirstFrameStartTime) {
		ddFirstFrameStartTime = currentTime;
		ddFirstAcquireStartTime = getEndTimeFromMicroDiff(currentTime, 500);
	}
	
	desktopDuplicationState = DESKDUPL_STATE_RUNNING;
	
	return 0;
}

//Can Enter this Function Under a Variety of Conditions
int desktopDuplicationEncodeNextFrame(void* bitstreamFilePtr, uint64_t* frameWriteCount) {
	if (desktopDuplicationState < DESKDUPL_STATE_RUNNING) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	if ((ddCurrentFrameState & 1) > 0) {
		//Check on async file write here
		//consoleWriteLineFast("Check Write File 0", 18);
		DWORD waitRes = WaitForSingleObject(ddOverlapped0Event, 0);
		if (waitRes == 0) { //Write Event Signaled
			NVENCSTATUS nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff0.bitstreamBuffer);
			if (nvEncRes != NV_ENC_SUCCESS) {
				nvEncExtraInfo = nvEncRes;
				return ERROR_NVENC_EXTRA_INFO;
			}
			(*frameWriteCount)++;
			//consoleWriteLineFast("Wrote to File 0", 15);
			
			ddCurrentFrameState &= ~1;
		}
	}
	if ((ddCurrentFrameState & 2) > 0) {
		//Check on async file write here
		DWORD waitRes = WaitForSingleObject(ddOverlapped1Event, 0);
		if (waitRes == 0) { //Write Event Signaled
			NVENCSTATUS nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff1.bitstreamBuffer);
			if (nvEncRes != NV_ENC_SUCCESS) {
				nvEncExtraInfo = nvEncRes;
				return ERROR_NVENC_EXTRA_INFO;
			}
			(*frameWriteCount)++;
			//consoleWriteLineFast("Wrote to File 1", 15);
			
			ddCurrentFrameState &= ~2;
		}
	}
	
	if ((ddCurrentFrameState & 4) > 0) {
		//Lock Bitstream to "finish" encoding step
		DWORD waitRes = WaitForSingleObject(ddLockEvent, 0);
		if (waitRes == 0) { //Lock Event Signaled
			//consoleWriteLineFast("Locked Bitstream", 16);
			
			uint64_t encodeFrameEndTime = getCurrentTime();
			//uint64_t encodeFailTime = ddFirstEncodeFailTime + (frameNum * ddIntervalTime);
			//if (encodeFrameEndTime > encodeFailTime) {
			//	ddEncodeFailTimeStat++;
			//}
			ddLatencySum += encodeFrameEndTime - ddLastPresentationTime;
			ddLatencyCount++;
			
			if ((ddCurrentFrame & 1) > 0) {
				//consoleWriteLineFast("Write 0", 7);
				
				//Start Async Write Here
				ddOverlapped0.Offset = (DWORD) (ddWriteOffset & 0xFFFFFFFF);
				ddOverlapped0.OffsetHigh = (DWORD) (ddWriteOffset >> 32);
				WriteFile((HANDLE) bitstreamFilePtr, ddEncodeBitstreamLock0.bitstreamBufferPtr, ddEncodeBitstreamLock0.bitstreamSizeInBytes, NULL, &ddOverlapped0);
				ddWriteOffset += ddEncodeBitstreamLock0.bitstreamSizeInBytes;
				
				//consoleWriteLineWithNumberFast("Ptr: ", 5, (uint64_t) ddEncodeBitstreamLock0.bitstreamBufferPtr, NUM_FORMAT_FULL_HEXADECIMAL);
				//consoleWriteLineWithNumberFast("Bytes: ", 7, (uint64_t) ddEncodeBitstreamLock0.bitstreamSizeInBytes, NUM_FORMAT_UNSIGNED_INTEGER);
		
				
				ddCurrentFrameState |= 1;
			}
			else {
				//consoleWriteLineFast("Write 1", 7);
				
				//Start Async Write Here
				ddOverlapped1.Offset = (DWORD) (ddWriteOffset & 0xFFFFFFFF);
				ddOverlapped1.OffsetHigh = (DWORD) (ddWriteOffset >> 32);
				WriteFile((HANDLE) bitstreamFilePtr, ddEncodeBitstreamLock1.bitstreamBufferPtr, ddEncodeBitstreamLock1.bitstreamSizeInBytes, NULL, &ddOverlapped1);
				ddWriteOffset += ddEncodeBitstreamLock1.bitstreamSizeInBytes;
				
				ddCurrentFrameState |= 2;
			}
			ddCurrentFrameState &= ~4;
		}
	}
	else if ((ddCurrentFrameState & 8) > 0) { 	//Compute Frame Stuff	
		ddComputeCheck++;
		VkResult vkRes = vkGetFenceStatus(vulkanDevice, vulkanComputeFence);
		if (vkRes == VK_SUCCESS) { //Can Now Encode and Release Desktop Duplication
			
			uint64_t computeFrameEndTime = getCurrentTime();
			ddComputeLatencySum += computeFrameEndTime - ddLastAcquireTime;
			ddComputeLatencyCount++;
			
			//consoleWriteLineFast("Frame Computed", 14);
			//nvEncPicParams.frameIdx = frameNum;
			NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
			if (nvEncRes != NV_ENC_SUCCESS) {
				nvEncExtraInfo = nvEncRes;
				return ERROR_NVENC_EXTRA_INFO;
			}
			BOOL setRes = SetEvent(ddEncodeEvent);
			if (setRes == 0) {
				return ERROR_GET_EXTRA_INFO;
			}
			HRESULT hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
			if (hrRes != S_OK) {
				if (hrRes != DXGI_ERROR_INVALID_CALL) {
					desktopDuplicationExtraInfo = (int) hrRes;
					return ERROR_DESKDUPL_RELEASE_FAILED;
				}
			}
			ddCurrentFrame++;
			if (ddCounterIDR > 0) {
				nvEncPicParams.encodePicFlags = 0; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_P;
				ddCounterIDR--;
			}
			else {
				nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR; //NV_ENC_PIC_TYPE_I;
				ddCounterIDR = ddCounterIDRreset;
			}
			if ((ddCurrentFrame & 1) > 0) {
				nvEncPicParams.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
			}
			else {
				nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
			}
			ddCurrentFrameState |= 4;
			ddCurrentFrameState &= ~8;
		}
		else if (vkRes == VK_NOT_READY) {
			return 1;
		}
		else {
			return ERROR_VULKAN_EXTRA_INFO;
		}
	}
	
	uint64_t frameStartTime = ddFirstFrameStartTime + (ddCurrentFrame * ddFrameIntervalTime);
	uint64_t frameEndTime = frameStartTime + ddFrameIntervalTime;
	if ((ddCurrentFrameState & 16) > 0) { //Acquired Image But Need To Wait before Starting Compute or Encoding
		ddWaitOnEncoding++;
		//consoleWriteLineFast("Waiting On Encoding", 19);
		if ((ddCurrentFrameState & 0xF) < 3) {
			ddLastPresentationTime = ddCurrentPresentationTime;
			if (ddLastPresentationTime < frameEndTime) { //Acquired Frame is Valid Needs to wait on 8
				ddLastAcquireTime = getCurrentTime();
				vkQueueSubmit(vulkanComputeQueue, 1, &ddComputeSubmitInfo, vulkanComputeFence);
				ddCurrentFrameState |= 8;
			}
			else { //Same Frame to re-encode
				//Some Indicator that it will be the same frame
				//consoleWriteLineFast("Repeat Frame 0", 14);
				ddRepeatFrame++;
				NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
				if (nvEncRes != NV_ENC_SUCCESS) {
					nvEncExtraInfo = nvEncRes;
					return ERROR_NVENC_EXTRA_INFO;
				}
				BOOL setRes = SetEvent(ddEncodeEvent);
				if (setRes == 0) {
					return ERROR_GET_EXTRA_INFO;
				}
				ddCurrentFrame++;
				if (ddCounterIDR > 0) {
					nvEncPicParams.encodePicFlags = 0; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_P;
					ddCounterIDR--;
				}
				else {
					nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR; //NV_ENC_PIC_TYPE_I;
					ddCounterIDR = ddCounterIDRreset;
				}
				if ((ddCurrentFrame & 1) > 0) {
					nvEncPicParams.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
				}
				else {
					nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
				}
				ddCurrentFrameState |= 4;
				
				HRESULT hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
				if (hrRes != S_OK) {
					if (hrRes != DXGI_ERROR_INVALID_CALL) {
						desktopDuplicationExtraInfo = (int) hrRes;
						return ERROR_DESKDUPL_RELEASE_FAILED;
					}
				}
			}
			ddCurrentFrameState &= ~16;
		}
	}
	else {
		//Wait for release time
		uint64_t acquireStartTime = ddFirstAcquireStartTime + (ddCurrentFrame * ddFrameIntervalTime);
		uint64_t acquireEndTime = acquireStartTime + ddFrameIntervalTime;
		uint64_t currentTime = getCurrentTime();
		if (currentTime < acquireStartTime) {
			//ddReleaseFailTimeStat++;
			if (ddCurrentFrameState != 0) {
				return 0;
			}
			else {
				return getDiffTimeMilliseconds(currentTime, acquireStartTime);
			}
		}
		else if (currentTime >= acquireEndTime) {
			//Do different things based on a variety of states
			ddFrameNumInvalidStat++;
			//uint64_t validFrameNum = (currentTime - ddFirstFrameStartTime) / ddFrameIntervalTime;
			//validFrameNum += 1;
			//consoleWriteLineWithNumberFast("Invalid Frame Number: ", 23, ddCurrentFrame, NUM_FORMAT_UNSIGNED_INTEGER);
			//consoleWriteLineWithNumberFast("Invalid Frame Number: ", 23, validFrameNum, NUM_FORMAT_UNSIGNED_INTEGER);
		}
		
		//Frame rate cannot be more than 250 so I can always do 1ms wait for first try
		if (ddCurrentPresentationTime < frameStartTime) {
			//consoleWriteLineWithNumberFast("Acquire Try: ", 13, getDiffTimeMilliseconds(ddFirstFrameStartTime, currentTime), NUM_FORMAT_UNSIGNED_INTEGER);
			DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
			IDXGIResource* desktopResourcePtr = NULL;
			HRESULT hrRes = desktopDuplicationPtr->lpVtbl->AcquireNextFrame(desktopDuplicationPtr, 1, &frameInfo, &desktopResourcePtr);
			if (hrRes == S_OK) { //Acquired Something (Might just be mouse stuff)
				ddCurrentPresentationTime = (uint64_t) frameInfo.LastPresentTime.QuadPart;
				//currentTime = getCurrentTime();
				//if (ddCurrentPresentationTime > currentTime) {
				//	ddCurrentPresentationTime = currentTime;
				//	consoleWriteLineFast("Presentation Time Issue", 10);
				//}
				if (ddCurrentPresentationTime >= frameStartTime) { //Actually acquired image and it is in the expected time
					if (ddCurrentFrameState < 3) {
						//consoleWriteLineWithNumberFast("Frame Acquired: ", 16, getDiffTimeMilliseconds(ddFirstFrameStartTime, ddCurrentPresentationTime), NUM_FORMAT_UNSIGNED_INTEGER);
						ddLastPresentationTime = ddCurrentPresentationTime;
						if (ddLastPresentationTime < frameEndTime) { //Acquired Frame is Valid Needs to wait on 8
							ddLastAcquireTime = getCurrentTime();
							vkQueueSubmit(vulkanComputeQueue, 1, &ddComputeSubmitInfo, vulkanComputeFence);
							ddCurrentFrameState |= 8;
						}
						else { //Same Frame to re-encode
							//Some Indicator that it will be the same frame
							//consoleWriteLineFast("Repeat Frame 1", 14);
							ddRepeatFrame++;
							NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
							if (nvEncRes != NV_ENC_SUCCESS) {
								nvEncExtraInfo = nvEncRes;
								return ERROR_NVENC_EXTRA_INFO;
							}
							BOOL setRes = SetEvent(ddEncodeEvent);
							if (setRes == 0) {
								return ERROR_GET_EXTRA_INFO;
							}
							ddCurrentFrame++;
							if (ddCounterIDR > 0) {
								nvEncPicParams.encodePicFlags = 0; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_P;
								ddCounterIDR--;
							}
							else {
								nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR; //NV_ENC_PIC_TYPE_I;
								ddCounterIDR = ddCounterIDRreset;
							}
							if ((ddCurrentFrame & 1) > 0) {
								nvEncPicParams.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
							}
							else {
								nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
							}
							ddCurrentFrameState |= 4;
							
							hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
							if (hrRes != S_OK) {
								if (hrRes != DXGI_ERROR_INVALID_CALL) {
									desktopDuplicationExtraInfo = (int) hrRes;
									return ERROR_DESKDUPL_RELEASE_FAILED;
								}
							}
						}
					}
					else {
						ddCurrentFrameState |= 16;
					}
				}
				else { //probably acquired mouse change info... need to release frame
					hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
					if (hrRes != S_OK) {
						if (hrRes != DXGI_ERROR_INVALID_CALL) {
							desktopDuplicationExtraInfo = (int) hrRes;
							return ERROR_DESKDUPL_RELEASE_FAILED;
						}
					}
					
					currentTime = getCurrentTime();
					if (currentTime < frameEndTime) {
						return getDiffTimeMilliseconds(currentTime, frameEndTime);
					}
					else { //Re-encode previous Frame If Space
						if (ddCurrentFrameState < 3) {
							//Some Indicator that it will be the same frame
							//consoleWriteLineFast("Repeat Frame 2", 14);
							ddRepeatFrame++;
							NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
							if (nvEncRes != NV_ENC_SUCCESS) {
								nvEncExtraInfo = nvEncRes;
								return ERROR_NVENC_EXTRA_INFO;
							}
							BOOL setRes = SetEvent(ddEncodeEvent);
							if (setRes == 0) {
								return ERROR_GET_EXTRA_INFO;
							}
							ddCurrentFrame++;
							if (ddCounterIDR > 0) {
								nvEncPicParams.encodePicFlags = 0; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_P;
								ddCounterIDR--;
							}
							else {
								nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR; //NV_ENC_PIC_TYPE_I;
								ddCounterIDR = ddCounterIDRreset;
							}
							if ((ddCurrentFrame & 1) > 0) {
								nvEncPicParams.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
							}
							else {
								nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
							}
							ddCurrentFrameState |= 4;
						}
						else {
							ddCurrentFrameState |= 16;
						}
					}
				}
			}
			else if (hrRes == DXGI_ERROR_WAIT_TIMEOUT) { //Failed to acquire next frame due to timeout
				currentTime = getCurrentTime();
				if (currentTime < frameEndTime) {
					//return 0;
					return getDiffTimeMilliseconds(currentTime, frameEndTime);
				}
				else { //Re-encode previous Frame If Space
					if (ddCurrentFrameState < 3) {
						ddLastPresentationTime = currentTime;
						//Some Indicator that it will be the same frame
						//consoleWriteLineWithNumberFast("Repeat Frame 3: ", 16, getDiffTimeMilliseconds(ddFirstFrameStartTime, currentTime), NUM_FORMAT_UNSIGNED_INTEGER);
						ddRepeatFrame++;
						NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
						if (nvEncRes != NV_ENC_SUCCESS) {
							nvEncExtraInfo = nvEncRes;
							return ERROR_NVENC_EXTRA_INFO;
						}
						BOOL setRes = SetEvent(ddEncodeEvent);
						if (setRes == 0) {
							return ERROR_GET_EXTRA_INFO;
						}
						ddCurrentFrame++;
						if (ddCounterIDR > 0) {
							nvEncPicParams.encodePicFlags = 0; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_P;
							ddCounterIDR--;
						}
						else {
							nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR; //NV_ENC_PIC_TYPE_I;
							ddCounterIDR = ddCounterIDRreset;
						}
						if ((ddCurrentFrame & 1) > 0) {
							nvEncPicParams.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
						}
						else {
							nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
						}
						ddCurrentFrameState |= 4;
					}
					else {
						ddCurrentFrameState |= 16;
					}
				}
			}
			else {
				desktopDuplicationExtraInfo = (int) hrRes;
				return ERROR_DESKDUPL_ACQUIRE_FAILED;
			}
		}
		else if (ddCurrentPresentationTime < frameEndTime) {
			if (ddCurrentFrameState < 3) {
				consoleWriteLineFast("Reusing Acquired", 15);
				ddLastPresentationTime = ddCurrentPresentationTime;
				if (ddLastPresentationTime < frameEndTime) { //Acquired Frame is Valid Needs to wait on 8
					ddLastAcquireTime = getCurrentTime();
					vkQueueSubmit(vulkanComputeQueue, 1, &ddComputeSubmitInfo, vulkanComputeFence);
					ddCurrentFrameState |= 8;
				}
				else { //Same Frame to re-encode
					//Some Indicator that it will be the same frame
					//consoleWriteLineFast("Repeat Frame 4", 14);
					ddRepeatFrame++;
					NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
					if (nvEncRes != NV_ENC_SUCCESS) {
						nvEncExtraInfo = nvEncRes;
						return ERROR_NVENC_EXTRA_INFO;
					}
					BOOL setRes = SetEvent(ddEncodeEvent);
					if (setRes == 0) {
						return ERROR_GET_EXTRA_INFO;
					}
					ddCurrentFrame++;
					if (ddCounterIDR > 0) {
						nvEncPicParams.encodePicFlags = 0; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_P;
						ddCounterIDR--;
					}
					else {
						nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR; //NV_ENC_PIC_TYPE_I;
						ddCounterIDR = ddCounterIDRreset;
					}
					if ((ddCurrentFrame & 1) > 0) {
						nvEncPicParams.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
					}
					else {
						nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
					}
					ddCurrentFrameState |= 4;
					
					HRESULT hrRes = desktopDuplicationPtr->lpVtbl->ReleaseFrame(desktopDuplicationPtr);
					if (hrRes != S_OK) {
						if (hrRes != DXGI_ERROR_INVALID_CALL) {
							desktopDuplicationExtraInfo = (int) hrRes;
							return ERROR_DESKDUPL_RELEASE_FAILED;
						}
					}
				}
			}
			else {
				ddCurrentFrameState |= 16;
			}
		}
		else {
			consoleWriteLineFast("Sync Issue", 10);
			currentTime = getCurrentTime();
			consoleWriteLineWithNumberFast("PT: ", 4, getDiffTimeMicroseconds(frameEndTime, ddCurrentPresentationTime), NUM_FORMAT_UNSIGNED_INTEGER);
			consoleWriteLineWithNumberFast("CT: ", 4, currentTime, NUM_FORMAT_UNSIGNED_INTEGER);
			return ERROR_RARE_TIMING_DESYNC;
		}
		
	}
	
	
	return 0;
}

int desktopDuplicationPrintEncodingStats() {
	//Insert State Check(s) Here!
	
	consoleWriteLineWithNumberFast("Repeated Frame Count: ", 22, ddRepeatFrame, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Wait On Encoding: ", 18, ddWaitOnEncoding, NUM_FORMAT_UNSIGNED_INTEGER);
	consoleWriteLineWithNumberFast("Frame Invalid Stat: ", 20, ddFrameNumInvalidStat, NUM_FORMAT_UNSIGNED_INTEGER);
	
	
	uint64_t latency = ddLatencySum / ddLatencyCount;
	latency /= microsecondDivider;
	consoleWriteLineWithNumberFast("Average Latency in us: ", 23, latency, NUM_FORMAT_UNSIGNED_INTEGER);
	
	latency = ddComputeLatencySum / ddComputeLatencyCount;
	latency /= microsecondDivider;
	consoleWriteLineWithNumberFast("Avg Compute Latency us: ", 24, latency, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//uint64_t checks = ddComputeCheck / ddComputeLatencyCount;
	//consoleWriteLineWithNumberFast("Compute Avg Checks: ", 20, checks, NUM_FORMAT_UNSIGNED_INTEGER);
	
	return 0;
}


int desktopDuplicationStop() {
	if (desktopDuplicationState == DESKDUPL_STATE_UNDEFINED) {
		return 0;
	}
	
	//Stop NvEncoder
	//Stop Vulkan
	//Stop DirectX 11 Duplication
	
	return 0;
}

void desktopDuplicationGetError(int* error) {
	*error = desktopDuplicationExtraInfo;
}

void vulkanGetError(int* error) {
	*error = vulkanExtraInfo;
}

void nvEncodeGetError(int* error) {
	*error = nvEncExtraInfo;
}


#define RETURN_ON_INVALID_SOCKET(socket) ({if (networkSocket == INVALID_SOCKET) {	return ERROR_WSA_EXTRA_INFO; }})
#define RETURN_ON_SOCKET_ERROR(error) ({if (error == SOCKET_ERROR) {	return ERROR_WSA_EXTRA_INFO; }})

static const char localHostStr[] = "::"; //IPv6 LocalHost Address (Short Form) "::1" is specifically loopback only
static const uint64_t serverPort = 4567;
static const uint64_t recvMsgBuffers = 50; //50 Based on 250Mbps and 2ms processing time
static const uint64_t sendMsgBuffers = 20;
static const uint64_t msgBufSize = 1400;
static const uint64_t msgBufSizeTest = 1400;

// Network State Codes:
#define NETWORK_STATE_UNDEFINED 0
#define NETWORK_STATE_STARTED 1
#define NETWORK_STATE_MEM_ALLOCATED 2
#define NETWORK_STATE_SOCKET_CONFIGURED 3
#define NETWORK_STATE_CLIENT 4
#define NETWORK_STATE_SERVER 5
static uint64_t networkState = NETWORK_STATE_UNDEFINED;

static SOCKET networkSocket = INVALID_SOCKET;
LPFN_WSARECVMSG WSARecvMsgF = NULL;
LPFN_WSASENDMSG WSASendMsgF = NULL;

static SOCKADDR_IN6 networkServerAddress;
static int networkServerAddressSize = sizeof(networkServerAddress);

static void* networkRecvBuffer = NULL;
static uint64_t networkCurrentRecvBuffer = 0;
static void* networkSendBuffer = NULL;
static uint64_t networkCurrentSendBuffer = 0;

//NULL terminated server address string
int networkStartup(uint64_t isServer, char* serverAddress) {
	if (compatibilityState < COMPATIBILITY_STATE_FULL) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	if (networkState > NETWORK_STATE_UNDEFINED) {
		return ERROR_NOT_STARTED_ENOUGH;
	}
	
	//Windows Networking Startup:
	WSADATA wsaData;
	int error = WSAStartup(MAKEWORD(2,2), &wsaData);
	RETURN_ON_ERROR(error);
	
	networkState = NETWORK_STATE_STARTED;
	
	//Receive and Send Buffers Setup:
	uint64_t allocationSize = (msgBufSize * recvMsgBuffers) >> 12; // 4096 byte sizes rounded up
	allocationSize = (allocationSize + 1) << 12; 
	error = allocCompatibilityMemory(&networkRecvBuffer, allocationSize, 0);
	RETURN_ON_ERROR(error);
	
	char* recvMsgBuf = (char*) networkRecvBuffer;
	for (uint64_t i=0; i<recvMsgBuffers; i++) {
		WSAMSG* WSAMsgPtr = (WSAMSG*) recvMsgBuf;
		WSAMsgPtr->name = (SOCKADDR*) &recvMsgBuf[56];
		WSAMsgPtr->namelen = sizeof(SOCKADDR_IN6); //Expecting it to equal 28
		WSAMsgPtr->lpBuffers = (WSABUF*) &recvMsgBuf[88];
		WSAMsgPtr->dwBufferCount = 1;
		WSAMsgPtr->Control.len = 64;
		WSAMsgPtr->Control.buf = &recvMsgBuf[104];
		WSAMsgPtr->dwFlags = 0;
		WSABUF* WSABufPtr = (WSABUF*) &recvMsgBuf[88];
		WSABufPtr->len = 1200;
		WSABufPtr->buf = &recvMsgBuf[200];
		WSAOVERLAPPED* WSAOverlappedPtr = (WSAOVERLAPPED*) &recvMsgBuf[168];
		WSAOverlappedPtr->hEvent = WSACreateEvent();
		if (WSAOverlappedPtr->hEvent == WSA_INVALID_EVENT) {
			return ERROR_WSA_EXTRA_INFO;
		}
		recvMsgBuf += msgBufSize;
	}
	networkCurrentRecvBuffer = 0;
	
	allocationSize = (msgBufSizeTest * sendMsgBuffers) >> 12; // 4096 byte sizes rounded up
	allocationSize = (allocationSize + 1) << 12; 
	error = allocCompatibilityMemory(&networkSendBuffer, allocationSize, 0);
	RETURN_ON_ERROR(error);
	
	char* sendMsgBuf = (char*) networkSendBuffer;
	for (uint64_t i=0; i<sendMsgBuffers; i++) {
		WSAMSG* WSAMsgPtr = (WSAMSG*) sendMsgBuf;
		WSAMsgPtr->name = (SOCKADDR*) &sendMsgBuf[56];
		SOCKADDR_IN6* SockAddrPtr = (SOCKADDR_IN6*) &sendMsgBuf[56];
		SockAddrPtr->sin6_family = AF_INET6;
		WSAMsgPtr->namelen = sizeof(SOCKADDR_IN6); //Expecting it to equal 28
		WSAMsgPtr->lpBuffers = (WSABUF*) &sendMsgBuf[88];
		WSAMsgPtr->dwBufferCount = 1;
		WSAMsgPtr->Control.len = 0;//64;
		WSAMsgPtr->Control.buf = NULL;//&sendMsgBuf[104];
		WSAMsgPtr->dwFlags = 0;
		WSABUF* WSABufPtr = (WSABUF*) &sendMsgBuf[88];
		WSABufPtr->len = 0;//1200;
		WSABufPtr->buf = &sendMsgBuf[200];
		WSAOVERLAPPED* WSAOverlappedPtr = (WSAOVERLAPPED*) &sendMsgBuf[168];
		WSAOverlappedPtr->hEvent = WSACreateEvent();
		if (WSAOverlappedPtr->hEvent == WSA_INVALID_EVENT) {
			return ERROR_WSA_EXTRA_INFO;
		}
		sendMsgBuf += msgBufSizeTest;
	}
	networkCurrentSendBuffer = 0;
	
	networkState = NETWORK_STATE_MEM_ALLOCATED;
	
	//Socket Setup
	networkSocket = WSASocket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
	RETURN_ON_INVALID_SOCKET(networkSocket);
	
	DWORD returnedBytes;
	GUID WSARecvMsgGuid = WSAID_WSARECVMSG;
	GUID WSASendMsgGuid = WSAID_WSASENDMSG;
	
	error = WSAIoctl(networkSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &WSARecvMsgGuid, sizeof(WSARecvMsgGuid), &WSARecvMsgF, sizeof(WSARecvMsgF), &returnedBytes, NULL, NULL);
	RETURN_ON_SOCKET_ERROR(error);
	
	error = WSAIoctl(networkSocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &WSASendMsgGuid, sizeof(WSASendMsgGuid), &WSASendMsgF, sizeof(WSASendMsgF), &returnedBytes, NULL, NULL);
	RETURN_ON_SOCKET_ERROR(error);
	
	//Setup Socket Options (and sometimes double check them)
	DWORD optValue = FALSE;
	//int optValBytes = 8; //Used with getsockopt
	
	// IPv6 Only Mode (should be default, but setting it just in-case)
	optValue = TRUE;
	error = setsockopt(networkSocket, IPPROTO_IPV6, IPV6_V6ONLY, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	// IPv6 Don't Let OS Fragment Packets
	// This codebase focuses exclusively on IPv6 and sending datgrams of 1200 bytes or fewer
	optValue = TRUE;
	error = setsockopt(networkSocket, IPPROTO_IPV6, IPV6_DONTFRAG, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	// IPv6 Packet Info for microsoft send and recv message function
	optValue = TRUE;
	error = setsockopt(networkSocket, IPPROTO_IPV6, IPV6_PKTINFO, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	optValue = TRUE;
	error = setsockopt(networkSocket, IPPROTO_IPV6, IPV6_ECN, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	// IPv6 Set User MTU and MTU path discovery so Outgoing Messages Fail
	optValue = 1280;
	error = setsockopt(networkSocket, IPPROTO_IPV6, IPV6_USER_MTU, (char*) &optValue, sizeof(DWORD));
	//error = WSASetIPUserMtu(networkSocket, optValue);
	RETURN_ON_SOCKET_ERROR(error);
	
	//optValue = IP_PMTUDISC_DONT; //IP_PMTUDISC_PROBE? //Returns with problems right now...
	//error = setsockopt(networkSocket, IPPROTO_IPV6, IPV6_MTU_DISCOVER, (char*) &optValue, sizeof(DWORD));
	//RETURN_ON_SOCKET_ERROR(error);
	
	// IPv6 Set Max Hops (TTL)
	optValue = 150;
	error = setsockopt(networkSocket, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	// UDP Set Certain Features to Confirm Defaults:
	//optValue = TRUE; //Error Right Now...
	//error = setsockopt(networkSocket, IPPROTO_UDP, UDP_CHECKSUM_COVERAGE, (char*) &optValue, sizeof(DWORD));
	//RETURN_ON_SOCKET_ERROR(error);
	
	//optValue = 0; //Not Working Right Now...Could be a mingw gcc thing
	//error = setsockopt(networkSocket, IPPROTO_UDP, UDP_RECV_MAX_COALESCED_SIZE, (char*) &optValue, sizeof(DWORD));
	//error = WSASetUdpRecvMaxCoalescedSize(networkSocket, optValue);
	//RETURN_ON_SOCKET_ERROR(error);
	
	//optValue = 0; //Not Working Right Now...Could be a mingw gcc thing
	//error = setsockopt(networkSocket, IPPROTO_UDP, UDP_SEND_MSG_SIZE, (char*) &optValue, sizeof(DWORD));
	//error = WSASetUdpSendMessageSize(networkSocket, optValue);
	//RETURN_ON_SOCKET_ERROR(error);
	
	// Socket Timeout Options:
	optValue = 1000;
	error = setsockopt(networkSocket, SOL_SOCKET, SO_RCVTIMEO, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	optValue = 1000;
	error = setsockopt(networkSocket, SOL_SOCKET, SO_SNDTIMEO, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	// Socket Experimental Buffer Options:
	optValue = 1000;
	error = setsockopt(networkSocket, SOL_SOCKET, SO_RCVBUF, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	optValue = 0;
	error = setsockopt(networkSocket, SOL_SOCKET, SO_SNDBUF, (char*) &optValue, sizeof(DWORD));
	RETURN_ON_SOCKET_ERROR(error);
	
	networkState = NETWORK_STATE_SOCKET_CONFIGURED;
	
	networkServerAddress.sin6_family = AF_INET6;
	//networkServerAddress.sin6_flowinfo = 42;
	
	char* addressStr = serverAddress;
	if (isServer) {
		addressStr = (char*) localHostStr;
	}
	error = WSAStringToAddressA(addressStr, AF_INET6, NULL, (SOCKADDR*) &networkServerAddress, &networkServerAddressSize);
	RETURN_ON_SOCKET_ERROR(error);
	networkServerAddress.sin6_port = shortByteSwap(serverPort);
	
	if (isServer) {	
		
		error = bind(networkSocket, (SOCKADDR*) &networkServerAddress, networkServerAddressSize);
		RETURN_ON_SOCKET_ERROR(error);
		
		networkState = NETWORK_STATE_SERVER;
	}
	else {
		//Should auto discard datagrams recieved from other addresses (nice, helpful feature...) (need to check tho and then also ICMP or DNS firewall stuff)
		error = WSAConnect(networkSocket, (SOCKADDR*) &networkServerAddress, networkServerAddressSize, NULL, NULL, NULL, NULL);
		RETURN_ON_SOCKET_ERROR(error);
		
		networkState = NETWORK_STATE_CLIENT;
	}
	
	//networkServerAddress.sin6_flowinfo = 42;
	
	networkCurrentRecvBuffer = recvMsgBuffers - 1;
	for (uint64_t i=0; i<networkCurrentRecvBuffer; i++) {
		uint8_t* bufPtr = (uint8_t*) networkRecvBuffer;
		uint64_t bufPos = i * msgBufSize;
		bufPtr += bufPos;
		
		error = WSARecvMsgF(networkSocket, (WSAMSG*) bufPtr, NULL, (WSAOVERLAPPED*) &bufPtr[168], NULL);
		if (error == SOCKET_ERROR) {
			int extraError = WSAGetLastError();
			if (extraError != WSA_IO_PENDING) {
				return ERROR_WSA_EXTRA_INFO;
			}
		}
		else {
			return ERROR_NETWORK_MSG_ALREADY_RECV;
		}
	}
	
	//networkCurrentSendBuffer = 0;
	
	return 0;
}

int networkCleanup() {
	if (networkState == NETWORK_STATE_UNDEFINED) {
		return 0;
	}
	
	//if (networkState >= NETWORK_STATE_CLIENT) {
	//	//Stop any pending receives and sends...
	//}
	
	if (networkState >= NETWORK_STATE_SOCKET_CONFIGURED) {
		closesocket(networkSocket);
	}
	
	if (networkState >= NETWORK_STATE_MEM_ALLOCATED) {
		//Close Events Properly
		uint8_t* bufPtr = (uint8_t*) networkRecvBuffer;
		for (uint64_t i=0; i<recvMsgBuffers; i++) {
			WSAOVERLAPPED* WSAOverlappedPtr = (WSAOVERLAPPED*) &bufPtr[168];
			BOOL res = WSACloseEvent(WSAOverlappedPtr->hEvent);
			if (res == FALSE) {
				//return error
			}
			bufPtr += msgBufSize;
		}
		bufPtr = (uint8_t*) networkSendBuffer;
		for (uint64_t i=0; i<sendMsgBuffers; i++) {
			WSAOVERLAPPED* WSAOverlappedPtr = (WSAOVERLAPPED*) &bufPtr[168];
			BOOL res = WSACloseEvent(WSAOverlappedPtr->hEvent);
			if (res == FALSE) {
				//return error
			}
			bufPtr += msgBufSizeTest;
		}
	}
		
	//Receive and Send Buffers Cleanup:
	int error = deallocCompatibilityMemory(&networkSendBuffer);
	if (error != 0) {
		//return error;
	}
	
	error = deallocCompatibilityMemory(&networkRecvBuffer);
	if (error != 0) {
		//return error;
	}
	
	error = WSACleanup();
	if (error != 0) {
		//return error;
	}
	
	networkState = NETWORK_STATE_UNDEFINED;
	return 0;
}

void getCompatibilityWSAError(int* error) {
	if (compatibilityState < COMPATIBILITY_STATE_BARE) {
		return;
	}
	if (networkState == NETWORK_STATE_UNDEFINED) {
		return;
	}
	
	*error = (int) WSAGetLastError();
}

int networkGetServerAddrPort(netAddrPortFlow* addrPort) {
	if (networkState < NETWORK_STATE_CLIENT) {
		return ERROR_NETWORK_NOT_SETUP;
	}
	
	//if (networkServerAddress.sin6_family != AF_INET6) {
	//	return ERROR_NETWORK_BAD_ADDRESS;
	//}
	
	memcpyBasic(addrPort->address, &(networkServerAddress.sin6_addr), 16);
	addrPort->port = networkServerAddress.sin6_port;
	addrPort->flow = networkServerAddress.sin6_flowinfo;
	
	return 0;
}

int networkGetNextRecvMessageBuffer(uint8_t** recvMsgBuf, uint64_t* recvMsgBytes, uint64_t wait) {
	if (networkState < NETWORK_STATE_CLIENT) {
		return ERROR_NETWORK_NOT_SETUP;
	}
	
	uint64_t nextRecvBuffer = networkCurrentRecvBuffer + 1;
	if (nextRecvBuffer >= recvMsgBuffers) {
		nextRecvBuffer = 0;
	}
	
	uint8_t* bufPtr = (uint8_t*) networkRecvBuffer;
	uint64_t bufPos = nextRecvBuffer * msgBufSize;
	bufPtr += bufPos;
	
	WSAOVERLAPPED* WSAOverlappedPtr = (WSAOVERLAPPED*) &bufPtr[168];
	
	//First check if next message is ready 
	DWORD actualBytesRecv;
	DWORD recvFlagsResult;
	BOOL res = WSAGetOverlappedResult(networkSocket, WSAOverlappedPtr, &actualBytesRecv, (BOOL) wait, &recvFlagsResult);
	if (res == FALSE) {
		int extraError = WSAGetLastError();
		if (extraError == WSA_IO_INCOMPLETE) {
			return NETWORK_RECV_PENDING;
		}
		else if (extraError == WSAEMSGSIZE) {
			WSAMSG* WSAMsgPtr = (WSAMSG*) bufPtr;
			WSAMsgPtr->dwFlags = 0;
			//BOOL res = WSAResetEvent(WSAOverlappedPtr->hEvent);
			//if (res == FALSE) {
			//	return ERROR_WSA_EXTRA_INFO;
			//}
		}
		else {
			return ERROR_WSA_EXTRA_INFO;
		}
	}
	*recvMsgBuf = (uint8_t*) &bufPtr[200];
	*recvMsgBytes = (uint64_t) actualBytesRecv;
	
	//Second Queue up the past used message:
	bufPtr = (uint8_t*) networkRecvBuffer;
	bufPos = networkCurrentRecvBuffer * msgBufSize;
	bufPtr += bufPos;
	
	int error = WSARecvMsgF(networkSocket, (WSAMSG*) bufPtr, NULL, (WSAOVERLAPPED*) &bufPtr[168], NULL);
	if (error == SOCKET_ERROR) {
		int extraError = WSAGetLastError();
		if (extraError != WSA_IO_PENDING) {
			return ERROR_WSA_EXTRA_INFO;
		}
	}
	else {
		return ERROR_NETWORK_MSG_ALREADY_RECV;
	}
	
	//Point currentRecv Buffer to current spot
	networkCurrentRecvBuffer = nextRecvBuffer;	
	
	return 0;
}

//addrPort char array should have an allocated length of at least 64.. will fix later
int networkGetAddrPortStr(char* addrPortStr, uint64_t* addrPortBytes, uint64_t currRecvAddr) {
	if (networkState < NETWORK_STATE_CLIENT) {
		return ERROR_NETWORK_NOT_SETUP;
	}
	
	if (*addrPortBytes < 64) {
		return ERROR_NETWORK_LOW_BSIZE;
	}
	
	SOCKADDR_IN6 localAddress;
	int addressSize = sizeof(localAddress);
	SOCKADDR* address;
	
	if (currRecvAddr == 0) {
		int error = getsockname(networkSocket, (SOCKADDR*) &localAddress, &addressSize);
		if (error == SOCKET_ERROR) {
			return ERROR_WSA_EXTRA_INFO;
		}
		address = (SOCKADDR*) &localAddress;
	}
	else {
		uint8_t* bufPtr = (uint8_t*) networkRecvBuffer;
		uint64_t bufPos = networkCurrentRecvBuffer * msgBufSize;
		bufPtr += bufPos;
		address = (SOCKADDR*) &bufPtr[56];
	}
	
	int error = WSAAddressToStringA(address, (DWORD) addressSize, NULL, addrPortStr, (DWORD*) addrPortBytes);
	if (error == SOCKET_ERROR) {
		return ERROR_WSA_EXTRA_INFO;
	}
	
	*addrPortBytes -= 1;
	
	return 0;
}

int networkGetRecvAddrPort(netAddrPortFlow* addrPort) {
	if (networkState < NETWORK_STATE_CLIENT) {
		return ERROR_NETWORK_NOT_SETUP;
	}
	
	uint8_t* bufPtr = (uint8_t*) networkRecvBuffer;
	uint64_t bufPos = networkCurrentRecvBuffer * msgBufSize;
	bufPtr += bufPos;
	SOCKADDR_IN6* recvAddress = (SOCKADDR_IN6*) &bufPtr[56];
	
	memcpyBasic(addrPort->address, &(recvAddress->sin6_addr), 16);
	addrPort->port = recvAddress->sin6_port;
	addrPort->flow = recvAddress->sin6_flowinfo;
	
	return 0;
}

int networkGetNextSendMessageBuffer(uint8_t** sendMsgBuf, uint64_t* sendMsgMaxBytes, uint64_t wait) {
	if (networkState < NETWORK_STATE_CLIENT) {
		return ERROR_NETWORK_NOT_SETUP;
	}
	
	uint8_t* bufPtr = (uint8_t*) networkSendBuffer;
	uint64_t bufPos = networkCurrentSendBuffer * msgBufSizeTest;
	bufPtr += bufPos;
	
	DWORD actualBytesSent;
	DWORD sendFlagsResult;
	BOOL res = WSAGetOverlappedResult(networkSocket, (WSAOVERLAPPED*) &bufPtr[168], &actualBytesSent, (BOOL) wait, &sendFlagsResult);
	if (res == FALSE) {
		int extraError = WSAGetLastError();
		if (extraError != WSA_IO_INCOMPLETE) {
			return ERROR_WSA_EXTRA_INFO;
		}
		else {
			return NETWORK_SEND_PENDING;
		}
	}
	
	*sendMsgBuf = (uint8_t*) &bufPtr[200];
	*sendMsgMaxBytes = msgBufSizeTest - 200;
	
	return 0;
}

int networkSendMessage(netAddrPortFlow* addrPort, uint64_t sendBytes) {
	if (networkState < NETWORK_STATE_CLIENT) {
		return ERROR_NETWORK_NOT_SETUP;
	}
	
	if (sendBytes > (msgBufSizeTest - 200)) {
		return ERROR_NETWORK_TOO_MANY_BYTES;
	}
	
	uint8_t* bufPtr = (uint8_t*) networkSendBuffer;
	uint64_t bufPos = networkCurrentSendBuffer * msgBufSizeTest;
	bufPtr += bufPos;
	
	SOCKADDR_IN6* SockAddrPtr = (SOCKADDR_IN6*) &bufPtr[56];
	memcpyBasic(&(SockAddrPtr->sin6_addr), addrPort->address, 16);
	SockAddrPtr->sin6_port = addrPort->port;
	SockAddrPtr->sin6_flowinfo = addrPort->flow;
	WSABUF* WSABufPtr = (WSABUF*) &bufPtr[88];
	WSABufPtr->len = sendBytes;
	
	int error = WSASendMsgF(networkSocket, (WSAMSG*) bufPtr, 0, NULL, (WSAOVERLAPPED*) &bufPtr[168], NULL);
	if (error == SOCKET_ERROR) {
		int extraError = WSAGetLastError();
		if (extraError != WSA_IO_PENDING) {
			return ERROR_WSA_EXTRA_INFO;
		}
	}
	
	networkCurrentSendBuffer++;
	if (networkCurrentSendBuffer >= sendMsgBuffers) {
		networkCurrentSendBuffer = 0;
	}
	
	return 0;
}

int networkWaitOnSentMessages() {
	if (networkState < NETWORK_STATE_CLIENT) {
		return ERROR_NETWORK_NOT_SETUP;
	}
	
	uint64_t previousSendBuffer = sendMsgBuffers;
	if (networkCurrentSendBuffer > 0) {
		previousSendBuffer = networkCurrentSendBuffer - 1;
	}
	
	uint8_t* bufPtr = (uint8_t*) networkSendBuffer;
	uint64_t bufPos = previousSendBuffer * msgBufSizeTest;
	bufPtr += bufPos;
	
	DWORD actualBytesSent;
	DWORD sendFlagsResult;
	BOOL res = WSAGetOverlappedResult(networkSocket, (WSAOVERLAPPED*) &bufPtr[168], &actualBytesSent, TRUE, &sendFlagsResult);
	if (res == FALSE) {
		return ERROR_WSA_EXTRA_INFO;
	}
	
	return 0;
}


static void stopCompatibility() {
	if (networkState > NETWORK_STATE_UNDEFINED) {
		networkCleanup();
	}
	
	if (consoleState > CONSOLE_STATE_UNDEFINED) {
		consoleBufferTerminate(0);
	}
	
	if (compatibilityState >= COMPATIBILITY_STATE_FULL) {
		SetCurrentConsoleFontEx(consoleOut, FALSE, &originalConsoleFont);
		
		SMALL_RECT sRect = {0, 0, 1, 1};
		SetConsoleWindowInfo(consoleOut, TRUE, &sRect);
		SetConsoleScreenBufferSize(consoleOut, originalConsoleScreenBufferSize);
		SetConsoleWindowInfo(consoleOut, TRUE, &originalConsoleScreenBufferCoordinates);
		
		SetConsoleMode(consoleIn, originalConsoleMode);
		SetConsoleMode(consoleOut, originalConsoleOutMode);
	}
	
	if (compatibilityState >= COMPATIBILITY_STATE_BARE) {
		SetConsoleCP(originalConsoleCP);
		SetConsoleOutputCP(originalConsoleOutCP);
	}	
	
	if (memPageBuffer != NULL) {
		deallocCompatibilityMemory(&memPageBuffer);
		memPageBuffer = NULL;
	}
	
	
	compatibilityState = COMPATIBILITY_STATE_UNDEFINED;
}

void exitCompatibility(int returnError) {
	stopCompatibility();
	
	ExitProcess((DWORD) returnError);
}

