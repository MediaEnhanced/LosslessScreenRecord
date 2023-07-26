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


//Media Enhanced Windows NVIDIA Compatibility Implementation
//All input strings are in UTF-8 but converts to UTF-16
//when neccessary to communicate with the win32 API
//Complete error checking is still a work in progress
#define COMPATIBILITY_GRAPHICS_UNNEEDED
#include "compatibility.h" //Includes stdint.h
//#include "math.h" //Includes the math function definitions (needed for gcd)

#define UNICODE //defined before any includes, dictates that Windows function defaults will work with Unicode UTF-16(LE) encoded strings
#define _UNICODE //similar definition
#define WIN32_LEAN_AND_MEAN //excludes several unnecessary includes when using windows.h
#include <windows.h> //Includes win32 functions and helper macros (uses the above defines)

#include <winsock2.h> //Windows Networking Header
#include <ws2tcpip.h> //Needed for additional windows networking functions and definitions (IPv6)
#include <mswsock.h> //Needed for (better) recv and send socket operations and some socket option defines


//Assumes that computer operates with little-endianess (reasonable assumption)
static uint64_t shortByteSwap(uint64_t value) {
	return ((value & 0xFF00) >> 8) | ((value & 0xFF) << 8);
}

#define RETURN_ON_INVALID_SOCKET(socket) ({if (networkSocket == INVALID_SOCKET) {	return ERROR_NETWORK_TBD; }})
#define RETURN_ON_SOCKET_ERROR(error) ({if (error == SOCKET_ERROR) {	return ERROR_NETWORK_TBD; }})

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

void compatibilityGetNetworkError(int* error) {
	if (networkState == NETWORK_STATE_UNDEFINED) {
		return;
	}
	
	*error = (int) WSAGetLastError();
}

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
	if (networkState > NETWORK_STATE_UNDEFINED) {
		return ERROR_NETWORK_WRONG_STATE;
	}
	
	//Windows Networking Startup:
	WSADATA wsaData;
	int error = WSAStartup(MAKEWORD(2,2), &wsaData);
	RETURN_ON_ERROR(error);
	
	networkState = NETWORK_STATE_STARTED;
	
	//Receive and Send Buffers Setup:
	uint64_t allocationSize = (msgBufSize * recvMsgBuffers) >> 12; // 4096 byte sizes rounded up
	allocationSize = (allocationSize + 1) << 12; 
	error = memoryAllocate(&networkRecvBuffer, allocationSize, 0);
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
			return ERROR_NETWORK_TBD;
		}
		recvMsgBuf += msgBufSize;
	}
	networkCurrentRecvBuffer = 0;
	
	allocationSize = (msgBufSizeTest * sendMsgBuffers) >> 12; // 4096 byte sizes rounded up
	allocationSize = (allocationSize + 1) << 12; 
	error = memoryAllocate(&networkSendBuffer, allocationSize, 0);
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
			return ERROR_NETWORK_TBD;
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
				return ERROR_NETWORK_TBD;
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
	int error = memoryDeallocate(&networkSendBuffer);
	if (error != 0) {
		//return error;
	}
	
	error = memoryDeallocate(&networkRecvBuffer);
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
			//	return ERROR_NETWORK_TBD;
			//}
		}
		else {
			return ERROR_NETWORK_TBD;
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
			return ERROR_NETWORK_TBD;
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
			return ERROR_NETWORK_TBD;
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
		return ERROR_NETWORK_TBD;
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
			return ERROR_NETWORK_TBD;
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
			return ERROR_NETWORK_TBD;
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
		return ERROR_NETWORK_TBD;
	}
	
	return 0;
}


