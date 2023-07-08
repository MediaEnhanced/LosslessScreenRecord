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


//Media Enhanced OS Compatibility Helper for console-based programs
//Reusable gcc header file for x64 architectures
#ifndef MEDIA_ENHANCED_COMPATIBILITY_H
#define MEDIA_ENHANCED_COMPATIBILITY_H

#include <stdint.h>	//Defines Data Types: https://en.wikipedia.org/wiki/C_data_types
#include "helperFunctions.h" //Defines the helper functions

//link in externaly created stringsData.o "compiled" data file
extern uint8_t stringsData[];
extern uint32_t stringsIndicies[];

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


// Compatibility Error Codes:
#define ERROR_RETURN_EARLY 0x0500 //return ERROR_RETURN_EARLY;
#define ERROR_TBD 0x0501
#define ERROR_ALREADY_STARTED 0x1000
#define ERROR_GET_EXTRA_INFO 0x1001
#define ERROR_INVALID_PAGE_SIZE 0x1002
#define ERROR_NOT_STARTED_ENOUGH 0x1003
#define ERROR_EVENT_NOT_CREATED 0x1004
#define ERROR_THREAD_NOT_CREATED 0x1005
#define ERROR_CON_BUFFER_ALREADY_DEFINED 0x2000
#define ERROR_CON_BUFFER_UNDEFINED 0x2001
#define ERROR_MEM_PAGE_BUFFER_UNDEFINED 0x3000
#define ERROR_LARGE_PAGE_PROBLEM 0x3001
#define ERROR_NOT_ENOUGH_MEMORY 0x3002
#define ERROR_NULL_POINTER 0x4000
#define ERROR_ARGUMENT_DNE 0x4001
#define ERROR_INCORRECT_READ_SIZE  0x5000
#define ERROR_INCORRECT_WRITE_SIZE 0x5001


// Number Format Codes:
#define NUM_FORMAT_UNDEFINED 0
#define NUM_FORMAT_FULL_HEXADECIMAL 1
#define NUM_FORMAT_PARTIAL_HEXADECIMAL 2
#define NUM_FORMAT_UNSIGNED_INTEGER 3
#define NUM_FORMAT_SIGNED_INTEGER 4

// Timing Constants:
#define SECOND_FREQUENCY 1 //1s = 1e0 Hz
#define MILLISECOND_FREQUENCY 1000 //1ms = 1e3 Hz
#define MICROSECOND_FREQUENCY 1000000 //1us = 1e6 Hz

// Console State Codes:
#define CONSOLE_STATE_UNDEFINED 0
#define CONSOLE_STATE_SETUP 1

// Console Control Codes:
#define CON_NEW_LINE 1
#define CON_FLIP_ORDER 2
#define CON_FLIP_ORDER_NEW_LINE 3
#define CON_CURSOR_ADVANCE 4

#define CON_FLUSH_MS 1

#define INFO_CALLBACK_FUNCTION_COMPLETION 0xA000
// Command Argument Codes:
// File Open / Read Codes:


#define NVIDIA_PCI_VENDER_ID 4318

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

#define ERROR_RARE_TIMING_DESYNC 0x5FFF

#define ERROR_WSA_EXTRA_INFO 0x6000
#define ERROR_NETWORK_UNDEFINED 0x6001
#define ERROR_NETWORK_NOT_SETUP 0x6002
#define ERROR_NETWORK_LOW_BSIZE 0x6003
#define ERROR_NETWORK_NO_ADDRESS 0x6004
#define ERROR_NETWORK_BAD_ADDRESS 0x6005
#define ERROR_NETWORK_MSG_ALREADY_RECV 0x6006
#define ERROR_NETWORK_TOO_MANY_BYTES 0x6007
#define ERROR_INCORRECT_SEND_SIZE 0x6100

#define NETWORK_RECV_PENDING 1
#define NETWORK_SEND_PENDING 2


#define ERROR_NVENC_EXTRA_INFO 0x9000

void* memcpyBasic(void* dest, const void* src, size_t count);

// Bare Console Functions:
void consoleWriteLineDirect(char* strUTF8, uint64_t strBytes);
void consoleWriteLineWithNumberDirect(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat);
void getCompatibilityExtraError(int* error);
void consoleWaitForEnter();

// Time / Performance Functions:
uint64_t getCurrentTime();
void incDiffTime(uint64_t* countTime, uint64_t startTime, uint64_t endTime);
uint64_t getDiffTimeMicroseconds(uint64_t startTime, uint64_t endTime);
uint64_t getDiffTimeMilliseconds(uint64_t startTime, uint64_t endTime);
uint64_t getDiffTimeSeconds(uint64_t startTime, uint64_t endTime);
uint64_t getEndTimeFromMicroDiff(uint64_t startTime, uint64_t usDiff);
uint64_t getEndTimeFromMilliDiff(uint64_t startTime, uint64_t msDiff);

// Start and Exit (Stop) Compatibility:
int startFullCompatibility();
void exitCompatibility(int returnError);

// Memory Operations:
int getCompatibilityNewMemoryPage(void** memoryPtr, uint64_t* memoryBytes);
int tryCompatibilitySetupLargeMemoryPages();
int allocCompatibilityMemory(void** memoryPtr, uint64_t memoryBytes, uint64_t largePage);
int deallocCompatibilityMemory(void** memoryPtr);

// Console Write and Read Functions:
int consoleBufferSetup();
int consoleBufferFlush();
int consoleBufferTerminate(uint64_t flush);
int consoleWrite(char* strUTF8, uint64_t strBytes, uint64_t conExtraInfo);
void consoleWriteLineFast(char* strUTF8, uint64_t strBytes);
void consoleWriteLineSlow(char* strUTF8);
int consoleWriteWithNumber(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat, uint64_t conExtraInfo);
void consoleWriteLineWithNumberFast(char* strUTF8, uint64_t strBytes, uint64_t number, uint64_t numberFormat);
int consoleControl(uint64_t conInstruction, uint64_t conExtraValue);

// Program Argument Handling:
int getCompatibilityArgument(uint64_t argumentNumber, char** argumentUTF8, uint64_t* argumentByteLength);
int getCompatibilityNextArgument(char** argumentUTF8, uint64_t* argumentByteLength);

int compatibilitySleep(uint64_t milliseconds);
void compatibilitySleepFast(uint64_t milliseconds);

int getCompatibilityTimestamp(uint64_t* timestamp);
int getCompatibilityMilliseconds(uint64_t* milliseconds);

int setCompatibilityMemoryPageBuffer();

// File Management Functions:
int openFile(void** filePtr, uint64_t flags, char* filePathUTF8, int filePathBytes);
int closeFile(void** filePtr);
int getFileSize(void** filePtr, uint64_t* fileSizeBytes);
int readFile(void** filePtr, void* dataPtr, uint32_t numBytes);
int writeFile(void* filePtr, void* dataPtr, uint32_t numBytes);
int selectAndOpenFile(void** filePtr, uint64_t flags, char* filePathUTF8);

void readFileFast(void** filePtr, void* dataPtr, uint32_t numBytes);
void writeFileFast(void** filePtr, void* dataPtr, uint32_t numBytes);

int desktopDuplicationStart(size_t shaderSize, uint32_t* shaderData, void** lutBufferPtr);
int desktopDuplicationLoadLUT();
int desktopDuplicationTestFrame(void* rawARGBfilePtr, void* bitstreamFilePtr);

int desktopDuplicationGetFrame();

int desktopDuplicationSetFrameRate(uint64_t fps);
int desktopDuplicationEncodeNextFrame(void* bitstreamFilePtr, uint64_t* frameWriteCount);
int desktopDuplicationPrintEncodingStats();

int desktopDuplicationStop();
void desktopDuplicationGetError(int* error);
void vulkanGetError(int* error);
void nvEncodeGetError(int* error);


// Network Functions:
struct networkAddressPortFlow {
	uint64_t address[2];
	uint64_t port;
	uint64_t flow;
};

typedef struct networkAddressPortFlow netAddrPortFlow;

int networkStartup(uint64_t isServer, char* serverAddress);
int networkCleanup();
void getCompatibilityWSAError(int* error);
int networkGetServerAddrPort(netAddrPortFlow* addrPort);
int networkGetNextRecvMessageBuffer(uint8_t** recvMsgBuf, uint64_t* recvMsgBytes, uint64_t wait);
int networkGetAddrPortStr(char* addrPortStr, uint64_t* addrPortBytes, uint64_t currRecvAddr); //addrPort char array should have an allocated length of at least 64
int networkGetRecvAddrPort(netAddrPortFlow* addrPort);
int networkGetNextSendMessageBuffer(uint8_t** sendMsgBuf, uint64_t* sendMsgMaxBytes, uint64_t wait);
int networkSendMessage(netAddrPortFlow* addrPort, uint64_t sendBytes);
int networkWaitOnSentMessages();


#endif //MEDIA_ENHANCED_COMPATIBILITY_H
