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


//Media Enhanced OS Compatibility Function Definitions for Console Programs
//UTF-8 is used Everywhere EXCEPT when calling MOST Windows OS API Functions
//in which cases the UTF-8 strings will be converted to UTF-16 before being
//passed along to the function calls
//This design choice makes it easy to stay consistent across OSes for code
//editing, string literal storeage, and program runtime (user interactions)
//It also makes it easier to provide proper localization when necessary
#ifndef MEDIA_ENHANCED_COMPATIBILITY_H
#define MEDIA_ENHANCED_COMPATIBILITY_H

#include <stdint.h>	//Defines Data Types: https://en.wikipedia.org/wiki/C_data_types

#define RETURN_ON_ERROR(error) ({if (error != 0) { return error; }})

//The Microsoft x64 calling convention is used in all of the assembly functions:
//https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170 
#define ASM_CALLING_CONVENTION __attribute__((ms_abi))

// Necessary Fast Helper (Assembly) Functions:
uint64_t ASM_CALLING_CONVENTION numToFHexStr(uint64_t number, char* strPtr);
uint64_t ASM_CALLING_CONVENTION numToPHexStr(uint64_t number, char* strPtr);
uint64_t ASM_CALLING_CONVENTION numToUDecStr(char* strPtr, uint64_t number);
//uint64_t ASM_CALLING_CONVENTION numToSDecStr(uint64_t number, char* strPtr);
uint64_t ASM_CALLING_CONVENTION shortToDecStr(char* strPtr, uint64_t number);

//Common Compatibility Functions: Implemented in compatibility.c
void* memcpyBasic(void* dest, const void* src, size_t count);


//Non Common Misc Functions:
void compatibilityExit(int returnError);
void compatibilityGetExtraError(int* error);


// Time Functions:
#define SECOND_FREQUENCY 1 //1s = 1e0 Hz
#define MILLISECOND_FREQUENCY 1000 //1ms = 1e3 Hz
#define MICROSECOND_FREQUENCY 1000000 //1us = 1e6 Hz

int timeFunctionSetup();
uint64_t getCurrentTime();
uint64_t getDiffTimeMicroseconds(uint64_t startTime, uint64_t endTime);
uint64_t getDiffTimeMilliseconds(uint64_t startTime, uint64_t endTime);
uint64_t getDiffTimeSeconds(uint64_t startTime, uint64_t endTime);
uint64_t getEndTimeFromMicroDiff(uint64_t startTime, uint64_t usDiff);
uint64_t getEndTimeFromMilliDiff(uint64_t startTime, uint64_t msDiff);

uint64_t getTimestampNTP();
uint64_t getTimestamp100us();


// Memory Functions
int memoryLargePageSetup();
int memoryAllocateOnePage(void** memoryPtr, uint64_t* memoryBytes);
int memoryAllocate(void** memoryPtr, uint64_t memoryBytes, uint64_t largePage);
int memoryGetSize(void* memoryPtr, uint64_t* memoryBytes);
int memoryDeallocate(void** memoryPtr);


// Number Format Codes:
#define NUM_FORMAT_UNDEFINED 0
#define NUM_FORMAT_FULL_HEXADECIMAL 1
#define NUM_FORMAT_PARTIAL_HEXADECIMAL 2
#define NUM_FORMAT_UNSIGNED_INTEGER 3
#define NUM_FORMAT_SIGNED_INTEGER 4

// Minimum Console Functions:
void consoleSetupMinimum();
void consoleWriteDirectLine(char* strUTF8, uint64_t strBytes);
void consoleWriteDirectLineWithNumber(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat);
void consoleWaitForEnter();

// Console Control Codes:
#define CON_NO_CTRL 0
#define CON_NEW_LINE 1
#define CON_FLIP_ORDER 2
#define CON_FLIP_ORDER_NEW_LINE 3
#define CON_CURSOR_ADVANCE 4

// Full Console Functions:
int consoleSetupFull();
int consoleBufferFlush();
int consoleWrite(char* strUTF8, uint64_t strBytes, uint64_t conExtraInfo);
void consoleWriteLineFast(char* strUTF8, uint64_t strBytes);
int consoleWriteLineSlow(char* strUTF8);
int consoleWriteWithNumber(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat, uint64_t conExtraInfo);
void consoleWriteLineWithNumberFast(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat);
int consoleControl(uint64_t conInstruction, uint64_t conExtraValue);
void consoleCleanup();


//Sleep Functions: 
#define SLEEP_RETURN_IO_COMPLETION 0x10000
int compatibilitySleep(uint64_t milliseconds);
void compatibilitySleepFast(uint64_t milliseconds);


// File Flag Codes:
#define IO_FILE_READ_NORMAL 0
#define IO_FILE_WRITE_NORMAL 1
#define IO_FILE_READ_ASYNC 2
#define IO_FILE_WRITE_ASYNC 3

// Argument and File Management Functions:
int ioSetup();
int ioGetNextCommandArgument(char** argumentUTF8, uint64_t* argumentByteLength);
int ioGetCommandArgument(uint64_t argumentNumber, char** argumentUTF8, uint64_t* argumentByteLength);
int ioOpenFile(void** filePtr, char* filePathUTF8, int filePathBytes, uint64_t flags);
int ioCloseFile(void** filePtr);
int ioGetFileSize(void** filePtr, uint64_t* fileSizeBytes);
int ioReadFile(void** filePtr, void* dataPtr, uint32_t numBytes);
int ioWriteFile(void* filePtr, void* dataPtr, uint32_t numBytes);
int ioSelectAndOpenFile(void** filePtr, uint64_t flags, char* filePathUTF8);
void ioCleanup();


// Compatibility Setup and Cleanup Helper Functions:
int compatibilitySetup();
void compatibilityCleanup();


int desktopDuplicationSetup(size_t shaderSize, uint32_t* shaderData, void** lutBufferPtr);
int desktopDuplicationLoadLUT();
int desktopDuplicationTestFrame(void* rawARGBfilePtr, void* bitstreamFilePtr);

int desktopDuplicationStart(uint64_t fps);
int desktopDuplicationRun(void* bitstreamFilePtr, uint64_t* frameWriteCount);
int desktopDuplicationStop();

int desktopDuplicationGetFrame();

int desktopDuplicationSetFrameRate(uint64_t fps);
int desktopDuplicationEncodeNextFrame(void* bitstreamFilePtr, uint64_t* frameWriteCount);
int desktopDuplicationPrintEncodingStats();

int desktopDuplicationCleanup();

void desktopDuplicationGetError(int* error);
void vulkanGetError(int* error);
void nvidiaGetError(int* error);





#define ERROR_ARGUMENT_DNE 0x0FFE
#define ERROR_INVALID_ARGUMENT 0x0FFF
#define ERROR_TIMER_BAD 0x1000
#define ERROR_LARGE_PAGE_NOT_ALLOWED 0x1001
#define ERROR_MEMORY_CANNOT_ALLOC 0x1002
#define ERROR_LARGE_PAGE_NOT_ENOUGH_BYTES 0x1003
#define ERROR_MEMORY_CANNOT_GET_SIZE 0x1004
#define ERROR_MEMORY_CANNOT_FREE 0x1005
#define ERROR_CONSOLE_WRONG_STATE 0x1006
#define ERROR_CONSOLE_FULL_SETUP 0x1007
#define ERROR_CONSOLE_WRITE 0x1008
#define ERROR_CONSOLE_WRITE_SIZE 0x1009
#define ERROR_IO_WRONG_STATE 0x100A
#define ERROR_IO_UNICODE_TRANSLATE 0x100B
#define ERROR_IO_TEMP_BUFF_NOT_ENOUGH_MEMORY 0x100C
#define ERROR_IO_CANNOT_OPEN_FILE 0x100D
#define ERROR_IO_CANNOT_CLOSE_FILE 0x100E
#define ERROR_IO_CANNOT_GET_FILE_SIZE 0x100F
#define ERROR_IO_CANNOT_READ_FILE 0x1010
#define ERROR_IO_WRONG_READ_SIZE 0x1011
#define ERROR_IO_CANNOT_WRITE_FILE 0x1012
#define ERROR_IO_WRONG_WRITE_SIZE 0x1013
#define ERROR_EVENT_NOT_CREATED 0x1014
#define ERROR_THREAD_NOT_CREATED 0x1015
#define ERROR_EVENT_NOT_SET 0x1016

#define ERROR_TBD 0x103F


// Compatibility Error Codes:
#define ERROR_RETURN_EARLY 0x0500







#define ERROR_MEM_PAGE_BUFFER_UNDEFINED 0x3000
#define ERROR_NOT_ENOUGH_MEMORY 0x3002










#define NVIDIA_PCI_VENDER_ID 4318


#define ERROR_VULKAN_EXTRA_INFO 0x5040
#define ERROR_VULKAN_CREATE_INSTANCE_FAILED 0x5041
#define ERROR_VULKAN_NO_PHYSICAL_DEVICES 0x5042
#define ERROR_VULKAN_CANNOT_FIND_GPU 0x5043
#define ERROR_VULKAN_NO_COMPUTE_QUEUE 0x5044
#define ERROR_VULKAN_DEVICE_CREATION_FAILED 0x5045
#define ERROR_VULKAN_IMAGE_CREATION_FAILED 0x5046
#define ERROR_VULKAN_BUFFER_CREATION_FAILED 0x5047
#define ERROR_VULKAN_BAD_OPTIMAL_FEATURES 0x5048
#define ERROR_VULKAN_WIN32_HANDLE_PROBLEM 0x5049
#define ERROR_VULKAN_MEM_ALLOC_FAILED 0x504A
#define ERROR_VULKAN_MEM_BIND_FAILED 0x504B
#define ERROR_VULKAN_COMMAND_POOL_FAILED 0x504C
#define ERROR_VULKAN_COMMAND_BUFFER_FAILED 0x504D
#define ERROR_VULKAN_COM_BUF_BEGIN_FAILED 0x504E
#define ERROR_VULKAN_COM_BUF_END_FAILED 0x504F
#define ERROR_VULKAN_MEM_MAP_FAILED 0x5050
#define ERROR_VULKAN_TBD 0x507F

#define ERROR_DESKDUPL_CREATE_FACTORY 0x5000
#define ERROR_DESKDUPL_ENUM_ADAPTER 0x5001
#define ERROR_DESKDUPL_ADAPTER_DESC 0x5002
#define ERROR_DESKDUPL_ADAPTER_NOT_VALID 0x5003
#define ERROR_DESKDUPL_CREATE_DEVICE 0x5004
#define ERROR_DESKDUPL_ENUM_OUTPUT 0x5005
#define ERROR_DESKDUPL_OUTPUT_DESC 0x5006
#define ERROR_DESKDUPL_CREATE_OUTPUT_DUPLICATION 0x5007
#define ERROR_DESKDUPL_NOT_VALID 0x5008
#define ERROR_DESKDUPL_RELEASE_FAILED 0x5009
#define ERROR_DESKDUPL_ACQUIRE_FAILED 0x500A
#define ERROR_DESKDUPL_ACQUIRE_TIMEOUT 0x500B
#define ERROR_DESKDUPL_TEXTURE_QUERY 0x500C
#define ERROR_DESKDUPL_TEXTURE_INVALID 0x500D
#define ERROR_DESKDUPL_RESOURCE_QUERY 0x500E
#define ERROR_DESKDUPL_CREATE_SHARED_HANDLE 0x500F
#define ERROR_DESKDUPL_KEYEDMUTEX_QUERY 0x5010
#define ERROR_DESKDUPL_WRONG_STATE 0x5011



#define ERROR_CUDA_NO_INIT 0x5080
#define ERROR_CUDA_CANNOT_GET_VERSION 0x5081
#define ERROR_CUDA_LOW_VERSION 0x5082
#define ERROR_CUDA_NO_DEVICES 0x5083
#define ERROR_CUDA_CANNOT_GET_DEVICE 0x5084
#define ERROR_CUDA_CANNOT_GET_DEVICE_LUID 0x5085
#define ERROR_CUDA_CANNOT_GET_CONTEXT_STATE 0x5086
#define ERROR_CUDA_CANNOT_GET_CONTEXT 0x5087
#define ERROR_CUDA_CANNOT_PUSH_CONTEXT 0x5088
#define ERROR_CUDA_CANNOT_GET_LIMIT 0x5089
#define ERROR_CUDA_CANNOT_SET_LIMIT 0x508A
#define ERROR_CUDA_CANNOT_IMPORT_MEMORY 0x508B
#define ERROR_CUDA_CANNOT_MAP_MEMORY 0x508C
#define ERROR_CUDA_CANNOT_GET_ARRAY 0x508D
#define ERROR_CUDA_CANNOT_POP_CONTEXT 0x508E
#define ERROR_CUDA_TBD 0x50BF

#define ERROR_NVENC_CANNOT_CREATE_INSTANCE 0x50C0
#define ERROR_NVENC_CANNOT_OPEN_SESSION 0x50C1
#define ERROR_NVENC_CANNOT_GET_ENCODE_GUIDS 0x50C2
#define ERROR_NVENC_NO_HEVC 0x50C3
#define ERROR_NVENC_CANNOT_GET_ENCODE_PROFILES 0x50C4
#define ERROR_NVENC_NO_HEVC_PROFILE 0x50C5
#define ERROR_NVENC_CANNOT_GET_ENCODE_PRESETS 0x50C6
#define ERROR_NVENC_NO_PRESET 0x50C7
#define ERROR_NVENC_CANNOT_GET_PRESET_CONFIG 0x50C8
#define ERROR_NVENC_CANNOT_GET_INPUT_FORMATS 0x50C9
#define ERROR_NVENC_NO_LOSSLESS_INPUT_FORMAT 0x50CA
#define ERROR_NVENC_CANNOT_GET_CAPABILITY 0x50CB
#define ERROR_NVENC_CANNOT_INITIALIZE 0x50CC
#define ERROR_NVENC_CANNOT_REGISTER_RES 0x50CD
#define ERROR_NVENC_CANNOT_MAP_RES 0x50CE
#define ERROR_NVENC_CANNOT_CREATE_BITSTREAM 0x50CF
#define ERROR_NVENC_CANNOT_UNLOCK_BITSTREAM 0x50D0
#define ERROR_NVENC_TBD 0x50FF

#define ERROR_NVENC_EXTRA_INFO 0x9000

//Network Stuff

#define ERROR_NETWORK_WRONG_STATE 0x6000
#define ERROR_NETWORK_UNDEFINED 0x6001
#define ERROR_NETWORK_NOT_SETUP 0x6002
#define ERROR_NETWORK_LOW_BSIZE 0x6003
#define ERROR_NETWORK_NO_ADDRESS 0x6004
#define ERROR_NETWORK_BAD_ADDRESS 0x6005
#define ERROR_NETWORK_MSG_ALREADY_RECV 0x6006
#define ERROR_NETWORK_TOO_MANY_BYTES 0x6007
#define ERROR_NETWORK_TBD 0x603F

#define NETWORK_RECV_PENDING 1
#define NETWORK_SEND_PENDING 2









// Network Functions:
void compatibilityGetNetworkError(int* error);

struct networkAddressPortFlow {
	uint64_t address[2];
	uint64_t port;
	uint64_t flow;
};

typedef struct networkAddressPortFlow netAddrPortFlow;

int networkStartup(uint64_t isServer, char* serverAddress);
int networkCleanup();

int networkGetServerAddrPort(netAddrPortFlow* addrPort);
int networkGetNextRecvMessageBuffer(uint8_t** recvMsgBuf, uint64_t* recvMsgBytes, uint64_t wait);
int networkGetAddrPortStr(char* addrPortStr, uint64_t* addrPortBytes, uint64_t currRecvAddr); //addrPort char array should have an allocated length of at least 64
int networkGetRecvAddrPort(netAddrPortFlow* addrPort);
int networkGetNextSendMessageBuffer(uint8_t** sendMsgBuf, uint64_t* sendMsgMaxBytes, uint64_t wait);
int networkSendMessage(netAddrPortFlow* addrPort, uint64_t sendBytes);
int networkWaitOnSentMessages();






#endif //MEDIA_ENHANCED_COMPATIBILITY_H

