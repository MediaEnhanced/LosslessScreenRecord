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


//This is the main file for the Vulkan Video Playback program for x64
//processor architecture targets and is designed to be compiled by gcc
//This top-level program file includes the entry point and OS independent code
//When OS dependent functionality needs to be used, the linked in compatibility
//functions get called which share the same function definitions (prototypes)
//These functions make their own calls to the OS API and abstract it away from
//the main program (sometimes the majority of the code executed are in these functions)
//The program avoids using any runtime library functions which usually vary
//between OSes and insteads calls specific implementations for relevant functions
// More Details Here in the Future

#define COMPATIBILITY_NETWORK_UNNEEDED //Do not need networking
#include "programEntry.h" //Includes "programStrings.h" & "compatibility.h" & <stdint.h>


//Program Main Function
int programMain() {
	int error = 0;
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	//Desktop Duplication Setup:
	consolePrintLine(26);
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t venderID = 0;
	error = graphicsDesktopDuplicationSetup(&width, &height, &venderID);
	RETURN_ON_ERROR(error);
	consolePrintLine(27);
	
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	//Vulkan Window Setup:
	consolePrintLine(51);
	VkDevice device = VK_NULL_HANDLE;
	uint32_t graphicsTransferQFI = 256;
	VkQueue graphicsTransferQueue = VK_NULL_HANDLE;
	error = vulkanWindowSetup(width>>2, height>>2, &device, &graphicsTransferQFI, &graphicsTransferQueue, NULL);
	RETURN_ON_ERROR(error);
	
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	//Vulkan Window Start Using Desktop Duplication Image
	VkImage desktopDuplicationImage = VK_NULL_HANDLE;
	VkDeviceMemory desktopDuplicationImageMemory = VK_NULL_HANDLE;
	error = vulkanImportDesktopDuplicationImage(device, &desktopDuplicationImage, &desktopDuplicationImageMemory);
	RETURN_ON_ERROR(error);
	
	VkFence swapchainCopyFence = VK_NULL_HANDLE;
	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = NULL;
	fenceInfo.flags = 0;
	VkResult result = vkCreateFence(device, &fenceInfo, VULKAN_ALLOCATOR, &swapchainCopyFence);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	error = vulkanWindowStart(width, height, desktopDuplicationImage, swapchainCopyFence);
	RETURN_ON_ERROR(error);
	
	consolePrintLine(52);
	consoleBufferFlush();
	//consoleWaitForEnter();
	
	//Vulkan Window Main Loop
	uint64_t windowDestroyed = 0;
	uint64_t renderPause = 0;
	uint64_t desktopDuplicationAllowed = 0;
	uint64_t renderNextTry = 0;
	
	uint64_t presentationTime = 0;
	uint64_t accumulatedFrames = 0;
	
	uint64_t enterPressed = 0;
	do {
		
		if (renderPause == 0) {
			if (desktopDuplicationAllowed == 0) {
				result = vkGetFenceStatus(device, swapchainCopyFence);
				if (result == VK_SUCCESS) { //Finished the swapchain copy
					vkResetFences(device, 1, &swapchainCopyFence);
					desktopDuplicationAllowed = 1;
				}
				else if (result != VK_NOT_READY) {
					return ERROR_VULKAN_TBD;
				}
			}
			if (desktopDuplicationAllowed == 1) {
				error = graphicsDesktopDuplicationReleaseFrame();
				RETURN_ON_ERROR(error);
				
				error = graphicsDesktopDuplicationAcquireNextFrame(8, &presentationTime, &accumulatedFrames);
				if (error == 0) {
					renderNextTry = 1;
				}
				else if (error != ERROR_DESKDUPL_ACQUIRE_TIMEOUT) {
					return error;
				}
			}
					
			if (renderNextTry == 1) {
				error = vulkanWindowRenderNext(swapchainCopyFence);
				if (error == 0) {
					renderNextTry = 0;
					desktopDuplicationAllowed = 0;
				}
				else if (error == ERROR_VULKAN_WINDOW_IS_PAUSED) {
					renderPause = 1;
				}
				else if (error == ERROR_VULKAN_WINDOW_MUST_FIX) {
					error = vulkanWindowResize();
					RETURN_ON_ERROR(error);
				}
				else {
					return error;
				}
			}
		}
		else {
			compatibilitySleepFast(20);
		}
		
		
		error = consoleCheckForEnter(&enterPressed);
		RETURN_ON_ERROR(error);
		if (enterPressed == 1) {
			vulkanWindowCleanup();
			windowDestroyed = 1;
		}
		else {
			uint64_t windowInformation = 0;
			error = vulkanWindowProcessMessages(&windowInformation);
			RETURN_ON_ERROR(error);
			if ((windowInformation & 1) > 0) {
				windowDestroyed = 1;
			}
			else if ((windowInformation & 2) > 0) {
				renderPause = 0;
			}
		}
	} while (windowDestroyed == 0);
	
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	//Cleanup ALL Vulkan Elements
	vkDestroyFence(device, swapchainCopyFence, VULKAN_ALLOCATOR);
	swapchainCopyFence = VK_NULL_HANDLE;
	
	//Desktop Duplication Imported Memory Check?
	//vkUnmapMemory(device, desktopDuplicationImageMemory);
	vkDestroyImage(device, desktopDuplicationImage, VULKAN_ALLOCATOR);
	vkFreeMemory(device, desktopDuplicationImageMemory, VULKAN_ALLOCATOR);
	
	graphicsTransferQueue = VK_NULL_HANDLE;
	vkDestroyDevice(device, VULKAN_ALLOCATOR);
	device = VK_NULL_HANDLE;
	
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	graphicsDesktopDuplicationCleanup();
	
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	vulkanCleanup();
	
	consolePrintLine(53);
	
	return 0; //Exit Program Successfully
}

