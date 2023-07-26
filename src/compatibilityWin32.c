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


//Media Enhanced Windows Compatibility Implementation
//All input strings are in UTF-8 but converts to UTF-16
//when neccessary to communicate with the win32 API
//Complete error checking is still a work in progress
#define COMPATIBILITY_GRAPHICS_UNNEEDED
#define COMPATIBILITY_NETWORK_UNNEEDED
#include "compatibility.h" //Includes stdint.h

#define UNICODE //defined before any includes, dictates that Windows function defaults will work with Unicode UTF-16(LE) encoded strings
#define _UNICODE //similar definition
#define WIN32_LEAN_AND_MEAN //excludes several unnecessary includes when using windows.h
#include <windows.h> //Includes win32 functions and helper macros (uses the above defines)

void compatibilityExit(int returnError) {	
	ExitProcess((DWORD) returnError);
}

void compatibilityGetExtraError(int* error) {
	*error = (int) GetLastError();
}


//Time State and Functions:
static uint64_t timeCounterFrequency = 0;
static uint64_t timeSecondDivider = 0;
static uint64_t timeMillisecondDivider = 0;
static uint64_t timeMicrosecondDivider = 0;

int timeFunctionSetup() {
	LARGE_INTEGER performanceCounter;
	BOOL result = QueryPerformanceFrequency(&performanceCounter);
	if (result == 0) {
		return ERROR_TIMER_BAD; //Will probably NEVER happen
	}
	timeCounterFrequency = (uint64_t) performanceCounter.QuadPart;
	timeSecondDivider = timeCounterFrequency / SECOND_FREQUENCY;
	timeMillisecondDivider = timeCounterFrequency / MILLISECOND_FREQUENCY;
	timeMicrosecondDivider = timeCounterFrequency / MICROSECOND_FREQUENCY;
	
	return 0;
}

uint64_t getCurrentTime() {
	LARGE_INTEGER performanceCounter;
	QueryPerformanceCounter(&performanceCounter);
	return (uint64_t) performanceCounter.QuadPart;
}

uint64_t getDiffTimeMicroseconds(uint64_t startTime, uint64_t endTime) {
	return ((endTime - startTime) / timeMicrosecondDivider);
}

uint64_t getDiffTimeMilliseconds(uint64_t startTime, uint64_t endTime) {
	return ((endTime - startTime) / timeMillisecondDivider);
}

uint64_t getDiffTimeSeconds(uint64_t startTime, uint64_t endTime) {
	return ((endTime - startTime) / timeSecondDivider);
}

uint64_t getEndTimeFromMicroDiff(uint64_t startTime, uint64_t usDiff) {
	return (startTime + (usDiff * timeMicrosecondDivider));
}

uint64_t getEndTimeFromMilliDiff(uint64_t startTime, uint64_t msDiff) {
	return (startTime + (msDiff * timeMillisecondDivider));
}

uint64_t getFrameIntervalTime(uint64_t fps) {
	return timeCounterFrequency / fps;
}

uint64_t getMicrosecondDivider() {
	return timeMicrosecondDivider;
}

uint64_t getTimestampNTP() {
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
	
	return seconds | secondFraction;
}

uint64_t getTimestamp100us() {
	uint64_t nanoseconds100 = 0;
	FILETIME* sysTimeUTC = (FILETIME*) &nanoseconds100;
	
	GetSystemTimePreciseAsFileTime(sysTimeUTC);
	
	nanoseconds100 -= (9435484800 * 10000000);
	uint64_t us100  = nanoseconds100 / 1000;
	
	return us100;
}


// Memory Operations:
static uint64_t largePageSupport = 0;

int memoryLargePageSetup() { //Try to allow the creation of large page memory
	/*
	//To be implemented using: https://learn.microsoft.com/en-us/windows/win32/memory/large-page-support
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
	
	largePageSupport = 0;
	return ERROR_LARGE_PAGE_NOT_ALLOWED;
}

int memoryAllocateOnePage(void** memoryPtr, uint64_t* memoryBytes) {	
	SYSTEM_INFO sSysInfo;
	GetSystemInfo(&sSysInfo);
	DWORD defaultPageSize = sSysInfo.dwPageSize; //Expecting to be set to 4KB (1024 * 4 bytes) for x64 Architecture
	
	*memoryBytes = (uint64_t) defaultPageSize;
	*memoryPtr = VirtualAlloc(NULL, (SIZE_T) defaultPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (*memoryPtr == NULL) {
		return ERROR_MEMORY_CANNOT_ALLOC;
	}
	return 0;
}

int memoryAllocate(void** memoryPtr, uint64_t memoryBytes, uint64_t largePage) {
	DWORD allocationType = MEM_COMMIT | MEM_RESERVE;
	if (largePage > 0) {
		if (largePageSupport == 0) {
			return ERROR_LARGE_PAGE_NOT_ALLOWED;
		}
		SIZE_T largePageMinBytes = GetLargePageMinimum();
		if ((memoryBytes % largePageMinBytes) != 0) {
			return ERROR_LARGE_PAGE_NOT_ENOUGH_BYTES;
		}
		allocationType |= MEM_LARGE_PAGES;
	}
	*memoryPtr = VirtualAlloc(NULL, (SIZE_T) memoryBytes, allocationType, PAGE_READWRITE);
	if (*memoryPtr == NULL) {
		return ERROR_MEMORY_CANNOT_ALLOC;
	}
	return 0;
}

int memoryGetSize(void* memoryPtr, uint64_t* memoryBytes) {
	MEMORY_BASIC_INFORMATION memInfo;
	SIZE_T infoBytes = VirtualQuery(memoryPtr, &memInfo, sizeof(MEMORY_BASIC_INFORMATION));
	if (infoBytes != sizeof(MEMORY_BASIC_INFORMATION)) {
		return ERROR_MEMORY_CANNOT_GET_SIZE;
	}
	*memoryBytes = (uint64_t) memInfo.RegionSize;
	return 0;
}

int memoryDeallocate(void** memoryPtr) {
	BOOL freeResult = VirtualFree(*memoryPtr, 0, MEM_RELEASE);
	if (freeResult == 0) {
		return ERROR_MEMORY_CANNOT_FREE;
	}
	*memoryPtr = NULL;
	return 0;
}


// Console State Codes:
#define CONSOLE_STATE_UNDEFINED 0
#define CONSOLE_STATE_MINIMUM 1
#define CONSOLE_STATE_FULL 2
static uint64_t consoleState = CONSOLE_STATE_UNDEFINED;

static HANDLE consoleOut = NULL;
static HANDLE consoleIn = NULL;
static UINT consoleOutCPOriginal = 0;
static UINT consoleCPOriginal = 0;
static DWORD consoleOutModeOriginal = 0;
static DWORD consoleModeOriginal = 0;

void consoleSetupMinimum() {
	if (consoleState > CONSOLE_STATE_UNDEFINED) {
		return;
	}
	
	consoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
	consoleIn = GetStdHandle(STD_INPUT_HANDLE);
	
	consoleOutCPOriginal = GetConsoleOutputCP();
	consoleCPOriginal = GetConsoleCP();
	
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	
	GetConsoleMode(consoleOut, &consoleOutModeOriginal);
	GetConsoleMode(consoleIn, &consoleModeOriginal);
	
	DWORD consoleOutMode = consoleOutModeOriginal | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	DWORD consoleMode = ENABLE_PROCESSED_INPUT;//consoleModeOriginal & (~ENABLE_ECHO_INPUT);
	
	SetConsoleMode(consoleOut, consoleOutMode);
	SetConsoleMode(consoleIn, consoleMode);
	
	consoleState = CONSOLE_STATE_MINIMUM;
}

void consoleWriteDirectLine(char* strUTF8, uint64_t strBytes) {
	if (consoleState < CONSOLE_STATE_MINIMUM) {
		return;
	}
	
	WriteConsoleA(consoleOut, strUTF8, (DWORD) strBytes, NULL, NULL);
	char newLine = '\n';
	WriteConsoleA(consoleOut, &newLine, 1, NULL, NULL);
}

void consoleWriteDirectLineWithNumber(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat) {
	if (consoleState < CONSOLE_STATE_MINIMUM) {
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

void consoleWaitForEnter() {
	if (consoleState < CONSOLE_STATE_UNDEFINED) {
		return;
	}
	
	//If there is an enter event already "waiting" clear it first
	BOOL res = FlushConsoleInputBuffer(consoleIn); //Not Recommended
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

// Console (With Buffer) State and Functions:
const WCHAR consoleFontDesiredName[] = L"Courier New";
static CONSOLE_FONT_INFOEX consoleFontOrignal = {0};
static COORD consoleScreenBufferSizeOriginal = {0, 0};
static SMALL_RECT consoleScreenBufferCoordinatesOriginal = {0, 0, 0, 0};

#define CONSOLE_FLUSH_MS 20
static void* consoleBuffer = NULL;
static char* consoleBufferPos = NULL;
static uint64_t consoleByteSize = 0;
static uint64_t consoleBytesRemaining = 0;
static uint64_t consoleLastFlushTime = 0;

int consoleSetupFull() {
	if (consoleState != CONSOLE_STATE_MINIMUM) {
		return ERROR_CONSOLE_WRONG_STATE;
	}
	
	consoleFontOrignal.cbSize = sizeof(CONSOLE_FONT_INFOEX);
	BOOL res = GetCurrentConsoleFontEx(consoleOut, FALSE, &consoleFontOrignal); //Not Recommended
	if (res == 0) {
		return ERROR_CONSOLE_FULL_SETUP;
	}
	
	CONSOLE_SCREEN_BUFFER_INFO conInfo;
	res = GetConsoleScreenBufferInfo(consoleOut, &conInfo);
	if (res == 0) {
		return ERROR_CONSOLE_FULL_SETUP;
	}
	consoleScreenBufferSizeOriginal.X = conInfo.dwSize.X;
	consoleScreenBufferSizeOriginal.Y = conInfo.dwSize.Y;
	consoleScreenBufferCoordinatesOriginal.Left = conInfo.srWindow.Left;
	consoleScreenBufferCoordinatesOriginal.Top = conInfo.srWindow.Top;
	consoleScreenBufferCoordinatesOriginal.Right = conInfo.srWindow.Right;
	consoleScreenBufferCoordinatesOriginal.Bottom = conInfo.srWindow.Bottom;
	
	CONSOLE_FONT_INFOEX cfi;
	cfi.cbSize = sizeof(cfi);
	cfi.nFont = 0;
	cfi.dwFontSize.X = 0;
	cfi.dwFontSize.Y = 20;
	cfi.FontFamily = FF_DONTCARE;
	cfi.FontWeight = FW_NORMAL;
	wcscpyBasic(cfi.FaceName, consoleFontDesiredName);
	res = SetCurrentConsoleFontEx(consoleOut, FALSE, &cfi); //Not Recommended
	if (res == 0) {
		return ERROR_CONSOLE_FULL_SETUP;
	}
	
	//What effect does this have on the background console "buffer" memory?
	SMALL_RECT sRect = {0, 0, 1, 1};
	res = SetConsoleWindowInfo(consoleOut, TRUE, &sRect); //Not Recommended
	if (res == 0) {
		return ERROR_CONSOLE_FULL_SETUP;
	}
	
	COORD cSize = {80, 400}; //100
	res = SetConsoleScreenBufferSize(consoleOut, cSize); //Not Recommended
	if (res == 0) {
		return ERROR_CONSOLE_FULL_SETUP;
	}
	
	sRect.Right = 79;
	sRect.Bottom = 20;
	res = SetConsoleWindowInfo(consoleOut, TRUE, &sRect); //Not Recommended
	if (res == 0) {
		return ERROR_CONSOLE_FULL_SETUP;
	}
	
	int error = memoryAllocateOnePage(&consoleBuffer, &consoleByteSize);
	if (error != 0) {
		return error;
	}
	
	consoleBufferPos = (char*) consoleBuffer;
	consoleBytesRemaining = consoleByteSize;
	consoleLastFlushTime = getCurrentTime();
	
	consoleState = CONSOLE_STATE_FULL;
	
	return 0;
}

int consoleBufferFlush() {
	if (consoleState < CONSOLE_STATE_FULL) {
		return ERROR_CONSOLE_WRONG_STATE;
	}
	
	DWORD bytesToWrite = (DWORD) (consoleByteSize - consoleBytesRemaining);
	if (bytesToWrite > 0) {
		DWORD bytesWritten = 0;
		BOOL res = WriteConsoleA(consoleOut, consoleBuffer, (DWORD) bytesToWrite, &bytesWritten, NULL);
		if (res == 0) {
			return ERROR_CONSOLE_WRITE;
		}
		if (bytesWritten != bytesToWrite) {
			return ERROR_CONSOLE_WRITE_SIZE;
		}
		
		consoleBufferPos = (char*) consoleBuffer;
		consoleBytesRemaining = consoleByteSize;
	}
	
	consoleLastFlushTime = getCurrentTime();
	return 0;
}

//Throws no error on invalid extra info
int consoleWrite(char* strUTF8, uint64_t strBytes, uint64_t conExtraInfo) {
	if (consoleState < CONSOLE_STATE_FULL) {
		return ERROR_CONSOLE_WRONG_STATE;
	}
	
	if (strUTF8 == NULL) {
		return ERROR_INVALID_ARGUMENT;
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
			return ERROR_CONSOLE_WRITE;
		}
		if (bytesWritten != ((DWORD) strBytes)) {
			return ERROR_CONSOLE_WRITE_SIZE;
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
	
	if ((consoleBytesRemaining < 256) || (diffTimeMS > CONSOLE_FLUSH_MS)) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	return 0;
}

//Fast Functions Assume Proper Console State
void consoleWriteLineFast(char* strUTF8, uint64_t strBytes) {		
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
	
	if (diffTimeMS > CONSOLE_FLUSH_MS) {
		consoleBufferFlush();
	}
}

int consoleWriteLineSlow(char* strUTF8) {
	if (consoleState < CONSOLE_STATE_FULL) {
		return ERROR_CONSOLE_WRONG_STATE;
	}
	
	
	while (*strUTF8 != 0) {
		if (consoleBytesRemaining == 0) {
			consoleBufferFlush();
		}
		
		*consoleBufferPos = *strUTF8;
		consoleBufferPos++;
		strUTF8++;
		consoleBytesRemaining--;
	}
	
	if (consoleBytesRemaining == 0) {
		consoleBufferFlush();
	}
	char newLine = '\n';
	*consoleBufferPos = newLine;
	consoleBufferPos++;
	consoleBytesRemaining--;
	
	uint64_t currentTime = getCurrentTime();
	uint64_t diffTimeMS = getDiffTimeMilliseconds(consoleLastFlushTime, currentTime);
	
	if (diffTimeMS > CONSOLE_FLUSH_MS) {
		consoleBufferFlush();
	}
	
	return 0;
}

int consoleWriteWithNumber(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat, uint64_t conExtraInfo) {
	if (consoleState < CONSOLE_STATE_FULL) {
		return ERROR_CONSOLE_WRONG_STATE;
	}
	
	if (strUTF8 == NULL) {
		return ERROR_INVALID_ARGUMENT;
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
				return ERROR_CONSOLE_WRITE;
			}
			if (bytesWritten != ((DWORD) strBytes)) {
				return ERROR_CONSOLE_WRITE_SIZE;
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
				return ERROR_CONSOLE_WRITE;
			}
			if (bytesWritten != ((DWORD) strBytes)) {
				return ERROR_CONSOLE_WRITE_SIZE;
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
	
	if ((consoleBytesRemaining < 256) || (diffTimeMS > CONSOLE_FLUSH_MS)) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	return 0;
}

void consoleWriteLineWithNumberFast(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat) {	
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
	
	if (diffTimeMS > CONSOLE_FLUSH_MS) {
		consoleBufferFlush();
	}
}

int consoleControl(uint64_t conInstruction, uint64_t conExtraValue) {
	if (consoleState < CONSOLE_STATE_FULL) {
		return ERROR_CONSOLE_WRONG_STATE;
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
	
	if ((consoleBytesRemaining < 256) || (diffTimeMS > CONSOLE_FLUSH_MS)) {
		int error = consoleBufferFlush();
		if (error != 0) {
			return error;
		}
	}
	
	return 0;
}

int consoleCheckForEnter(uint64_t* enterResult) {
	if (consoleState < CONSOLE_STATE_UNDEFINED) {
		return ERROR_CONSOLE_WRONG_STATE;
	}
	
	INPUT_RECORD inRecords[32];
	DWORD numRecordsRead = 0;
	do {
		BOOL res = PeekConsoleInputA(consoleIn, inRecords, 32, &numRecordsRead);
		if (res == 0) {
			return ERROR_CONSOLE_PEAK_INPUT;
		}
		for (DWORD r = 0; r < numRecordsRead; r++) {
			if (inRecords[r].EventType == KEY_EVENT) {
				if (inRecords[r].Event.KeyEvent.bKeyDown == FALSE) {
					if (inRecords[r].Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
						*enterResult = 1;
						return 0;
					}
				}
			}
		}
	} while (numRecordsRead == 32);
	
	*enterResult = 0;
	return 0;
}

void consoleCleanup() {
	if (consoleState == CONSOLE_STATE_FULL) {
		consoleBufferFlush();
		
		memoryDeallocate(&consoleBuffer);
		consoleBuffer = NULL;
		consoleBufferPos = NULL;
		consoleByteSize = 0;
		consoleBytesRemaining = 0;
		
		
		SetCurrentConsoleFontEx(consoleOut, FALSE, &consoleFontOrignal);
		
		SMALL_RECT sRect = {0, 0, 1, 1};
		SetConsoleWindowInfo(consoleOut, TRUE, &sRect);
		SetConsoleScreenBufferSize(consoleOut, consoleScreenBufferSizeOriginal);
		SetConsoleWindowInfo(consoleOut, TRUE, &consoleScreenBufferCoordinatesOriginal);
	}
	
	if (consoleState >= CONSOLE_STATE_MINIMUM) {
		SetConsoleMode(consoleIn, consoleModeOriginal);
		SetConsoleMode(consoleOut, consoleOutModeOriginal);
		
		SetConsoleCP(consoleCPOriginal);
		SetConsoleOutputCP(consoleOutCPOriginal);
		
		consoleOut = NULL;
		consoleIn = NULL;
	}
	
	consoleState = CONSOLE_STATE_UNDEFINED;
}


int compatibilitySleep(uint64_t milliseconds) {
	if (consoleState == CONSOLE_STATE_FULL) {
		consoleBufferFlush();
	}
	
	DWORD res = SleepEx((DWORD) milliseconds, TRUE); //True to return early due to callback functions
	if (res == WAIT_IO_COMPLETION) {
		return SLEEP_RETURN_IO_COMPLETION;
	}
	return 0;
}

void compatibilitySleepFast(uint64_t milliseconds) {
	SleepEx(milliseconds, FALSE);
}


//I/O include Stuff
//#include <Commdlg.h> //Includes win32 command dialog "pop-up" boxes used for choosing an input file

//#include <tchar.h> //needed for _tcscat_s
//#include <shobjidl.h>

// I/O State Codes:
#define IO_STATE_UNDEFINED 0
#define IO_STATE_SETUP 1
static uint64_t ioState = IO_STATE_UNDEFINED;

static void* ioTempBuffer = NULL;
static uint64_t ioTempBufferByteSize = 0;
static void* ioCommandArgumentBuffer = NULL;
static char* ioCommandArgumentPosition = NULL;

int ioSetup() {
	int error = memoryAllocateOnePage(&ioTempBuffer, &ioTempBufferByteSize);
	if (error != 0) {
		return error;
	}
	
	WCHAR* commandArguments = GetCommandLine();
	int characters = WideCharToMultiByte(CP_UTF8, 0, commandArguments, -1, NULL, 0, NULL, NULL);
	//consoleWriteLineWithNumberFast("Char N: ", 8, characters, NUM_FORMAT_UNSIGNED_INTEGER);
	if (characters == 0) {
		return ERROR_IO_UNICODE_TRANSLATE;
	}
	
	error = memoryAllocate(&ioCommandArgumentBuffer, (uint64_t) characters, 0);
	if (error != 0) {
		return error;
	}
	
	//uint64_t memBytes = 0;
	//error = memoryGetSize(ioCommandArgumentBuffer, &memBytes);
	//if (error != 0) {
	//	return error;
	//}
	//consoleWriteLineWithNumberFast("Bytes: ", 7, memBytes, NUM_FORMAT_UNSIGNED_INTEGER);
	
	error = WideCharToMultiByte(CP_UTF8, 0, commandArguments, -1, (LPSTR) ioCommandArgumentBuffer, characters, NULL, NULL);
	if (error != characters) {
		consoleWriteLineWithNumberFast("ERROR: ", 7, (uint64_t) GetLastError(), NUM_FORMAT_FULL_HEXADECIMAL);
		return ERROR_IO_UNICODE_TRANSLATE;
	}
	
	
	ioCommandArgumentPosition = (char*) ioCommandArgumentBuffer;
	
	ioState = IO_STATE_SETUP;
	return 0;
}

//Needs update
int ioGetNextCommandArgument(char** argumentUTF8, uint64_t* argumentByteLength) {
	if (ioState != IO_STATE_SETUP) {
		return ERROR_IO_WRONG_STATE;
	}
	
	char* charIterator = ioCommandArgumentPosition;
	*argumentUTF8 = charIterator;
	uint64_t byteLength = 0;
	while (*charIterator != 0) { // NULL character
		if (*charIterator == 32) { // SPACE (' ') character
			if (byteLength != 0) {
				*argumentByteLength = byteLength;
				charIterator++;
				ioCommandArgumentPosition = charIterator;
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
	ioCommandArgumentPosition = NULL;
	
	return 0;
}

//Needs update
int ioGetCommandArgument(uint64_t argumentNumber, char** argumentUTF8, uint64_t* argumentByteLength) {
	if (ioState != IO_STATE_SETUP) {
		return ERROR_IO_WRONG_STATE;
	}
	
	char* charIterator = (char*) ioCommandArgumentBuffer;
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

int ioOpenFile(void** filePtr, char* filePathUTF8, int filePathBytes, uint64_t flags) {
	if (ioState != IO_STATE_SETUP) {
		return ERROR_IO_WRONG_STATE;
	}
	int characters = MultiByteToWideChar(CP_UTF8, 0, filePathUTF8, filePathBytes, NULL, 0);
	if (characters >= (ioTempBufferByteSize>>1)) { //Account for NULL termination not fitting in future
		return ERROR_IO_TEMP_BUFF_NOT_ENOUGH_MEMORY;
	}
	LPWSTR filePathUTF16 = (LPWSTR) ioTempBuffer;
	int result = MultiByteToWideChar(CP_UTF8, 0, filePathUTF8, filePathBytes, filePathUTF16, characters);
	if (result == 0) {
		return ERROR_IO_UNICODE_TRANSLATE;
	}
	//if (filePathBytes > 0) { //-1 for file path bytes writes a null...?
		filePathUTF16[result] = 0;
	//}
	
	HANDLE fileHandle = NULL;
	if (flags == IO_FILE_READ_NORMAL) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else if (flags == IO_FILE_WRITE_NORMAL) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	else if (flags == IO_FILE_READ_ASYNC) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	}
	else if (flags == IO_FILE_WRITE_ASYNC) {
		fileHandle = CreateFile(filePathUTF16, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
	}
	else {
		return ERROR_INVALID_ARGUMENT;
	}
	if (fileHandle == INVALID_HANDLE_VALUE) {
		return ERROR_IO_CANNOT_OPEN_FILE;
	}
	
	*filePtr = fileHandle;
	return 0;
}

int ioCloseFile(void** filePtr) {
	BOOL result = CloseHandle((HANDLE) *filePtr);
	if (result == 0) {
		return ERROR_IO_CANNOT_CLOSE_FILE;
	}
	*filePtr = NULL;
	return 0;
}

int ioGetFileSize(void* filePtr, uint64_t* fileSizeBytes) {
	LARGE_INTEGER fileSizeEx;
	BOOL result = GetFileSizeEx((HANDLE) filePtr, &fileSizeEx);
	if (result == 0) {
		return ERROR_IO_CANNOT_GET_FILE_SIZE;
	}
	*fileSizeBytes = (uint64_t) fileSizeEx.QuadPart;
	return 0;
}

int ioReadFile(void* filePtr, void* dataPtr, uint32_t* numBytes) {
	DWORD readBytes = 0;
	BOOL result = ReadFile((HANDLE) filePtr, dataPtr, *numBytes, &readBytes, NULL);
	if (result == 0) {
		*numBytes = 0;
		return ERROR_IO_CANNOT_READ_FILE;
	}
	*numBytes = (uint32_t) readBytes;
	return 0;
}

int ioWriteFile(void* filePtr, void* dataPtr, uint32_t numBytes) {
	DWORD writtenBytes = 0;
	BOOL result = WriteFile((HANDLE) filePtr, dataPtr, numBytes, &writtenBytes, NULL);
	if (result == 0) {
		return ERROR_IO_CANNOT_WRITE_FILE;
	}
	if (writtenBytes != numBytes) {
		return ERROR_IO_WRONG_WRITE_SIZE;
	}
	return 0;
}

#define ASYNC_OPERATION_MAX 4
static OVERLAPPED ioAsyncOperations[ASYNC_OPERATION_MAX];
int ioAsyncSetup(uint64_t asyncOperationCount) {
	if (asyncOperationCount > ASYNC_OPERATION_MAX) {
		return ERROR_TBD;
	}
	for (uint64_t i = 0; i < asyncOperationCount; i++) {
		ioAsyncOperations[i].Internal = 0;
		ioAsyncOperations[i].InternalHigh = 0;
		ioAsyncOperations[i].Offset = 0xFFFFFFFF;
		ioAsyncOperations[i].OffsetHigh = 0xFFFFFFFF;
		ioAsyncOperations[i].Pointer = 0;
		ioAsyncOperations[i].hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (ioAsyncOperations[i].hEvent == NULL) {
			return ERROR_EVENT_NOT_CREATED;
		}
	}
	return 0;
}

int ioAsyncSignalWait(uint64_t asyncOperation) {
	DWORD waitRes = WaitForSingleObject(ioAsyncOperations[asyncOperation].hEvent, INFINITE);
	if (waitRes != 0) { //Write Event Signaled
		return ERROR_TBD;
	}
	return 0;
}

int ioAsyncSignalCheck(uint64_t asyncOperation, uint64_t* signaled) {
	DWORD waitRes = WaitForSingleObject(ioAsyncOperations[asyncOperation].hEvent, 0);
	if (waitRes == 0) { //Write Event Signaled
		*signaled = 1;
	}
	else if (waitRes == WAIT_TIMEOUT) {
		*signaled = 0;
	}
	else {
		return ERROR_TBD;
	}
	return 0;
}

int ioAsyncWriteFile(void* filePtr, void* dataPtr, uint64_t numBytes, uint64_t asyncOperation, uint64_t offset) {
	ioAsyncOperations[asyncOperation].Offset     = (DWORD) (offset &  0xFFFFFFFF);
	ioAsyncOperations[asyncOperation].OffsetHigh = (DWORD) (offset >> 32);
	WriteFile((HANDLE) filePtr, dataPtr, numBytes, NULL, &(ioAsyncOperations[asyncOperation]));
	//Error checking in future...?
	return 0;
}

void ioAsyncCleanup() {
	for (uint64_t i = 0; i < ASYNC_OPERATION_MAX; i++) {
		if (ioAsyncOperations[i].hEvent != NULL) {
			CloseHandle(ioAsyncOperations[i].hEvent);
		}
	}
}

//Needs some work
int ioSelectAndOpenFile(void** filePtr, uint64_t flags, char* filePathUTF8) {
	if (ioState != IO_STATE_SETUP) {
		return ERROR_IO_WRONG_STATE;
	}
	
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

int ioLoadLibrary(void** libraryPtr, char* libraryNameUTF8) {
	if (ioState != IO_STATE_SETUP) {
		return ERROR_IO_WRONG_STATE;
	}
	
	LPWSTR libraryNameUTF16 = (LPWSTR) ioTempBuffer;
	int result = MultiByteToWideChar(CP_UTF8, 0, libraryNameUTF8, -1, libraryNameUTF16, ioTempBufferByteSize);
	if (result == 0) {
		return ERROR_IO_UNICODE_TRANSLATE;
	}
	libraryNameUTF16[result] = 0; //Needed?
	
	HMODULE library = LoadLibraryEx(libraryNameUTF16, NULL, 0);
	if (library == NULL) {
		return ERROR_IO_CANNOT_LOAD_LIBRARY;
	}
	
	*libraryPtr = (void*) library;
	return 0;
}

int ioGetLibraryFunction(void* libraryPtr, char* functionNameUTF8, void** functionPtr) {	
	FARPROC funcPtr = GetProcAddress((HMODULE) libraryPtr, functionNameUTF8);
	if (funcPtr == NULL) {
		return ERROR_IO_CANNOT_FIND_LIBRARY_FUNCTION;
	}
	
	*functionPtr = (void*) funcPtr;
	return 0;
}

void ioCleanup() {
	if (ioState == IO_STATE_SETUP) {		
		memoryDeallocate(&ioCommandArgumentBuffer);
		ioCommandArgumentPosition = NULL;
		
		memoryDeallocate(&ioTempBuffer);
		ioTempBufferByteSize = 0;
	}
	
	ioState = IO_STATE_UNDEFINED;
}


// Compatibility Setup and Cleanup Helper Functions:
int compatibilitySetup() {
	int error = timeFunctionSetup();
	if (error != 0) {
		return error;
	}
	
	consoleSetupMinimum();
	//consoleWaitForEnter();
	
	error = consoleSetupFull();
	if (error != 0) {
		return error;
	}
	//consoleWaitForEnter();
	
	error = ioSetup();
	if (error != 0) {
		return error;
	}
	//consoleWaitForEnter();
	
	return 0;
}

void compatibilityCleanup() {
	ioCleanup();
	consoleCleanup();
}


//Create Event and Threads:
int syncCreateEvent(void** eventPtr, uint64_t manualReset, uint64_t initialState) {
	BOOL manReset = FALSE;
	if (manualReset > 0) {
		manReset = TRUE;
	}
	BOOL initState = FALSE;
	if (initialState > 0) {
		initState = TRUE;
	}
	
	*eventPtr = (void*) CreateEvent(NULL, manReset, initState, NULL);
	if (*eventPtr == NULL) {
		return ERROR_EVENT_NOT_CREATED;
	}
	
	return 0;
}

int syncSetEvent(void* eventPtr) {
	BOOL res = SetEvent((HANDLE) eventPtr);
	if (res == FALSE) {
		return ERROR_EVENT_NOT_SET;
	}
	return 0;
}

int syncResetEvent(void* eventPtr) {
	BOOL res = ResetEvent((HANDLE) eventPtr);
	if (res == FALSE) {
		return ERROR_EVENT_NOT_RESET;
	}
	return 0;
}

int syncEventWait(void* eventPtr) {
	DWORD waitRes = WaitForSingleObject(eventPtr, INFINITE);
	if (waitRes != 0) { //Write Event Signaled
		return ERROR_TBD;
	}
	return 0;
}

int syncEventCheck(void* eventPtr, uint64_t* signaled) {
	DWORD waitRes = WaitForSingleObject(eventPtr, 0);
	if (waitRes == 0) { //Write Event Signaled
		*signaled = 1;
	}
	else if (waitRes == WAIT_TIMEOUT) {
		*signaled = 0;
	}
	else {
		return ERROR_TBD;
	}
	return 0;
}

void syncCloseEvent(void** eventPtr) {
	CloseHandle((HANDLE) (*eventPtr));
	*eventPtr = NULL;
}

static DWORD WINAPI syncThreadStart(LPVOID lpParam) {
	PFN_ThreadStart threadStart = (PFN_ThreadStart) (lpParam);
	return (DWORD) threadStart();
}

int syncStartThread(void** threadPtr, PFN_ThreadStart threadStart, uint64_t initialState) {
	DWORD creationFlags = 0;
	if (initialState > 0) {
		creationFlags = CREATE_SUSPENDED;
	}
	
	*threadPtr = (void*) CreateThread(NULL, 1, syncThreadStart, (LPVOID) threadStart, creationFlags, NULL);
	if (*threadPtr == NULL) {
		return ERROR_THREAD_NOT_CREATED;
	}
	
	return 0;
}




