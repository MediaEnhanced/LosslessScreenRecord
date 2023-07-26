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

#include "programEntry.h" //Includes "programStrings.h" & "compatibility.h" & Common Vulkan & <stdint.h>
#include "math.h" //Includes the math function definitions
#include "include/nvEncodeAPI.h" //Includes the NVIDIA Encoder API

//During the Make process the GLSL Vulkan Compute Shader gets compiled to SPIR-V
//and then this binary data gets linked into the program via the following definitons
extern uint64_t shader_size;
extern uint8_t  shader_data[];

//Definition constants used for sRGB loops:
#define SRGB_MAX_VALUE 256
#define NUM_SRGB_VALUES 16777216

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

static VkDevice device = VK_NULL_HANDLE;
static VkQueue computeQueue = VK_NULL_HANDLE;
static VkQueue transferQueue = VK_NULL_HANDLE;
static VkImage desktopDuplicationImage = VK_NULL_HANDLE;
static VkDeviceMemory desktopDuplicationImageMemory = VK_NULL_HANDLE;
static VkImage yuv10Bit444PlanarTexture = VK_NULL_HANDLE;
static VkDeviceMemory yuv10Bit444PlanarTextureMemory = VK_NULL_HANDLE;

static VkBuffer stageBuffer = VK_NULL_HANDLE;
static VkBuffer lutBuffer = VK_NULL_HANDLE;
static VkDeviceMemory stageBufferMemory = VK_NULL_HANDLE;
static VkDeviceMemory lutBufferMemory = VK_NULL_HANDLE;

#define NUM_TRANSFER_COMMAND_BUFFERS 4
static VkCommandPool transferCommandPool = VK_NULL_HANDLE;
static VkCommandBuffer transferCommandBuffers[NUM_TRANSFER_COMMAND_BUFFERS];

#define NUM_COMPUTE_COMMAND_BUFFERS 1
static VkCommandPool computeCommandPool = VK_NULL_HANDLE;
static VkCommandBuffer computeCommandBuffers[NUM_COMPUTE_COMMAND_BUFFERS];
static VkShaderModule computeShaderModule = VK_NULL_HANDLE;
static VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
static VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
static VkPipeline computePipeline = VK_NULL_HANDLE;
static VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
static VkImageView desktopDuplicationImageView = VK_NULL_HANDLE;
static VkImageView yuv10Bit444PlanarTextureView = VK_NULL_HANDLE;

int setupVulkanCompute(uint32_t width, uint32_t height) {
	uint32_t computeQFI = 256;
	uint32_t transferQFI = 256;
	int error = vulkanComputeSetup(&device, &computeQFI, &transferQFI);
	RETURN_ON_ERROR(error);
	
	vkGetDeviceQueue(device, computeQFI, 0, &computeQueue);
	if (transferQFI != 256) {
		vkGetDeviceQueue(device, transferQFI, 0, &transferQueue);
	}
	else {
		transferQueue = computeQueue;
		transferQFI = computeQFI;
	}
	
	// Imports desktop duplication image
	error = vulkanImportDesktopDuplicationImage(device, &desktopDuplicationImage, &desktopDuplicationImageMemory);
	RETURN_ON_ERROR(error);
	
	//Create Converted Texture and Bind it to an (exclusive) Exportable Memory
	VkImageCreateInfo imageInfo;
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = NULL;
	imageInfo.flags = 0;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R16_UINT;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height * 3;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = 1;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; //Optimal Texture
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = 0;
	imageInfo.pQueueFamilyIndices = NULL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	error = vulkanCreateExportImageMemory(device, &imageInfo, "CnvTexHandle", &yuv10Bit444PlanarTexture, &yuv10Bit444PlanarTextureMemory);
	RETURN_ON_ERROR(error);
	
	
	
	//Create a Vulkan Staging Buffer:
	VkDeviceSize lutSize = NUM_SRGB_VALUES * sizeof(uint32_t);
	VkBufferCreateInfo bufferInfo;
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = NULL;
	bufferInfo.flags = 0;
	bufferInfo.size = lutSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.queueFamilyIndexCount = 0;
	bufferInfo.pQueueFamilyIndices = NULL;
	
	VkResult result = vkCreateBuffer(device, &bufferInfo, VULKAN_ALLOCATOR, &stageBuffer);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_BUFFER_CREATION_FAILED;
	}
	
	//Create Vulkan LUT Bufferthat will be used to store the sRGB Color Conversion LUT
	bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; //VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	
	result = vkCreateBuffer(device, &bufferInfo, VULKAN_ALLOCATOR, &lutBuffer);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_BUFFER_CREATION_FAILED;
	}
	
	//Allocate memory for the buffers (and then bind those buffers)
	VkBufferMemoryRequirementsInfo2 bufMemReqsInfo;
	bufMemReqsInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
	
	VkMemoryDedicatedRequirementsKHR dedicatedReqs;
	dedicatedReqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;
	dedicatedReqs.pNext = NULL;
	dedicatedReqs.prefersDedicatedAllocation = 0;
	
	bufMemReqsInfo.pNext = NULL;//&dedicatedReqs;
	bufMemReqsInfo.buffer = stageBuffer;
	
	VkMemoryRequirements2 memReqs2;
	memReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	memReqs2.pNext = NULL;
	
	vkGetBufferMemoryRequirements2(device, &bufMemReqsInfo, &memReqs2);
	//consoleWriteLineWithNumberFast("Size: ", 6, memReqs2.memoryRequirements.size, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Alignment: ", 11, memReqs2.memoryRequirements.alignment, NUM_FORMAT_PARTIAL_HEXADECIMAL);	
	//consoleWriteLineWithNumberFast("Dedication: ", 12, dedicatedReqs.prefersDedicatedAllocation, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	VkMemoryAllocateInfo memAllocInfo;
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	
	VkMemoryDedicatedAllocateInfoKHR memDedicatedAllocInfo;
	memDedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	memDedicatedAllocInfo.pNext = NULL;
	memDedicatedAllocInfo.image = VK_NULL_HANDLE;
	memDedicatedAllocInfo.buffer = stageBuffer;
	
	if (dedicatedReqs.prefersDedicatedAllocation != 0) {
		memAllocInfo.pNext = &memDedicatedAllocInfo;
	}
	else {
		memAllocInfo.pNext = NULL;
	}
	
	uint32_t deviceLocalMemIndex = 0;
	uint32_t cpuAccessMemIndex = 0;
	error = vulkanGetMemoryTypeIndex(device, &deviceLocalMemIndex, &cpuAccessMemIndex);
	RETURN_ON_ERROR(error);
	
	memAllocInfo.allocationSize = memReqs2.memoryRequirements.size;
	memAllocInfo.memoryTypeIndex = cpuAccessMemIndex;
	
	//64-byte aligned most likely
	result = vkAllocateMemory(device, &memAllocInfo, VULKAN_ALLOCATOR, &stageBufferMemory);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	
	
	result = vkBindBufferMemory(device, stageBuffer, stageBufferMemory, 0);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	
	
	bufMemReqsInfo.buffer = lutBuffer;
	vkGetBufferMemoryRequirements2(device, &bufMemReqsInfo, &memReqs2);
	//consoleWriteLineWithNumberFast("Size: ", 6, memReqs2.memoryRequirements.size, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Alignment: ", 11, memReqs2.memoryRequirements.alignment, NUM_FORMAT_PARTIAL_HEXADECIMAL);	
	//consoleWriteLineWithNumberFast("Dedication: ", 12, dedicatedReqs.prefersDedicatedAllocation, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	memDedicatedAllocInfo.buffer = lutBuffer;
	if (dedicatedReqs.prefersDedicatedAllocation != 0) {
		memAllocInfo.pNext = &memDedicatedAllocInfo;
	}
	else {
		memAllocInfo.pNext = NULL;
	}
	
	memAllocInfo.allocationSize = memReqs2.memoryRequirements.size;
	memAllocInfo.memoryTypeIndex = deviceLocalMemIndex;
	
	result = vkAllocateMemory(device, &memAllocInfo, VULKAN_ALLOCATOR, &lutBufferMemory);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	result = vkBindBufferMemory(device, lutBuffer, lutBufferMemory, 0);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	
	
	//Command Pool, Buffer Creation, and Buffer Recording
	//Transfer Queue Command Pool and Buffers:
	VkCommandPoolCreateInfo poolInfo;
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = NULL;
	poolInfo.flags = 0; //Double check in future...
	poolInfo.queueFamilyIndex = transferQFI;
	
	result = vkCreateCommandPool(device, &poolInfo, VULKAN_ALLOCATOR, &transferCommandPool);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COMMAND_POOL_FAILED;
	}
	
	VkCommandBufferAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.commandPool = transferCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = NUM_TRANSFER_COMMAND_BUFFERS;
	
	result = vkAllocateCommandBuffers(device, &allocInfo, transferCommandBuffers);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COMMAND_BUFFER_FAILED;
	}
	
	//Staging Buffer -> Lut Buffer Transfer
	VkCommandBuffer lutTransfer = transferCommandBuffers[0];
	
	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = NULL;
	beginInfo.flags = 0; //Double check in future...
	beginInfo.pInheritanceInfo = NULL; // Optional
	
	result = vkBeginCommandBuffer(lutTransfer, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	VkBufferCopy bufferCopyRegion;
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = lutSize;
	
	vkCmdCopyBuffer(lutTransfer, stageBuffer, lutBuffer, 1, &bufferCopyRegion);
	
	result = vkEndCommandBuffer(lutTransfer);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	
	//Desktop Duplication Texture -> Staging Buffer (Offset: 0)
	VkCommandBuffer imgTransfer = transferCommandBuffers[1];
	result = vkBeginCommandBuffer(imgTransfer, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	VkDependencyInfo vulkanWindowDependencyInfo;
	vulkanWindowDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	vulkanWindowDependencyInfo.pNext = NULL;
	vulkanWindowDependencyInfo.dependencyFlags = 0;
	vulkanWindowDependencyInfo.memoryBarrierCount = 0;
	vulkanWindowDependencyInfo.pMemoryBarriers = NULL;
	vulkanWindowDependencyInfo.bufferMemoryBarrierCount = 0;
	vulkanWindowDependencyInfo.pBufferMemoryBarriers = NULL;
	vulkanWindowDependencyInfo.imageMemoryBarrierCount = 1;
	
	VkImageMemoryBarrier2 vulkanWindowImgMemBar[2];
	vulkanWindowImgMemBar[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	vulkanWindowImgMemBar[0].pNext = NULL;
	vulkanWindowImgMemBar[0].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	vulkanWindowImgMemBar[0].srcAccessMask = VK_ACCESS_2_NONE;
	vulkanWindowImgMemBar[0].dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	vulkanWindowImgMemBar[0].dstAccessMask = VK_ACCESS_2_NONE;
	vulkanWindowImgMemBar[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vulkanWindowImgMemBar[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vulkanWindowImgMemBar[0].srcQueueFamilyIndex = transferQFI;
	vulkanWindowImgMemBar[0].dstQueueFamilyIndex = transferQFI;
	vulkanWindowImgMemBar[0].image = desktopDuplicationImage;
	vulkanWindowImgMemBar[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vulkanWindowImgMemBar[0].subresourceRange.baseMipLevel = 0;
	vulkanWindowImgMemBar[0].subresourceRange.levelCount = 1;
	vulkanWindowImgMemBar[0].subresourceRange.baseArrayLayer = 0;
	vulkanWindowImgMemBar[0].subresourceRange.layerCount = 1;
	
	vulkanWindowDependencyInfo.pImageMemoryBarriers = vulkanWindowImgMemBar;
	
	vkCmdPipelineBarrier2(imgTransfer, &vulkanWindowDependencyInfo);
	
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
	imgToBufRegions[0].imageExtent.width = width;
	imgToBufRegions[0].imageExtent.height = height;
	imgToBufRegions[0].imageExtent.depth = 1;
	
	vkCmdCopyImageToBuffer(imgTransfer, desktopDuplicationImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stageBuffer, 1, imgToBufRegions);
	
	result = vkEndCommandBuffer(imgTransfer);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	
	
	
	
	
	//Staging Buffer (Offset: 1) -> Converted Texture
	VkCommandBuffer imgTransferWrite = transferCommandBuffers[2];
	result = vkBeginCommandBuffer(imgTransferWrite, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	vulkanWindowImgMemBar[0].image = yuv10Bit444PlanarTexture;
	vulkanWindowImgMemBar[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vkCmdPipelineBarrier2(imgTransferWrite, &vulkanWindowDependencyInfo);
	
	//Write and Read of Converted Texture (using staging buffer) to be used as Input of Video Encoder
	imgToBufRegions[0].bufferOffset = width * height * 4;
	imgToBufRegions[0].imageExtent.height = height * 3;
	vkCmdCopyBufferToImage(imgTransferWrite, stageBuffer, yuv10Bit444PlanarTexture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, imgToBufRegions);
	
	result = vkEndCommandBuffer(imgTransferWrite);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	//Converted Texture -> Staging Buffer (Offset: 1)
	VkCommandBuffer imgTransferRead = transferCommandBuffers[3];
	result = vkBeginCommandBuffer(imgTransferRead, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	vulkanWindowImgMemBar[0].image = yuv10Bit444PlanarTexture;
	vulkanWindowImgMemBar[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vkCmdPipelineBarrier2(imgTransferRead, &vulkanWindowDependencyInfo);
	
	imgToBufRegions[0].bufferOffset = 0;
	vkCmdCopyImageToBuffer(imgTransferRead, yuv10Bit444PlanarTexture, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stageBuffer, 1, imgToBufRegions);
	
	result = vkEndCommandBuffer(imgTransferRead);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	
	
	
	
	//Compute Queue Command Pool and Buffers:
	//Binding and Compute Pipeline later...
	poolInfo.queueFamilyIndex = computeQFI;
	
	result = vkCreateCommandPool(device, &poolInfo, VULKAN_ALLOCATOR, &computeCommandPool);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COMMAND_POOL_FAILED;
	}
	
	allocInfo.commandPool = computeCommandPool;
	allocInfo.commandBufferCount = NUM_COMPUTE_COMMAND_BUFFERS;
	
	result = vkAllocateCommandBuffers(device, &allocInfo, computeCommandBuffers);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COMMAND_BUFFER_FAILED;
	}
	
	
	
		
	
	
	//Compute Shader for Making the Converted Texture
	//Desktop Duplication Texture -> Converted Texture 
	VkComputePipelineCreateInfo computePipelineInfo;
	computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineInfo.pNext = NULL;
	computePipelineInfo.flags = 0; //Double Check
	computePipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computePipelineInfo.stage.pNext = NULL;
	computePipelineInfo.stage.flags = 0; //Double Check
	computePipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	
	//Shader Module Creation and Definition
	VkShaderModuleCreateInfo shaderModuleInfo;
	shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleInfo.pNext = NULL;
	shaderModuleInfo.flags = 0;
	shaderModuleInfo.codeSize = shader_size; //Extern Variable
	shaderModuleInfo.pCode = (uint32_t*) shader_data; //Extern Variable
	//consoleWriteLineSlow("Got Here!");
	
	result = vkCreateShaderModule(device, &shaderModuleInfo, VULKAN_ALLOCATOR, &computeShaderModule);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineSlow("Got Here!");
	
	//consoleWriteLineSlow("Vulkan Allocate for shader");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	computePipelineInfo.stage.module = computeShaderModule;
	computePipelineInfo.stage.pName = "main";
	computePipelineInfo.stage.pSpecializationInfo = NULL; //Pretty Sure
	
	//Pipeline Layout Creation and Definition
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pNext = NULL;
	pipelineLayoutInfo.flags = 0;
	pipelineLayoutInfo.setLayoutCount = 1;
	
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo;
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
	
	result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, VULKAN_ALLOCATOR, &computeDescriptorSetLayout);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;//Maybe used in the future...
	pipelineLayoutInfo.pPushConstantRanges = NULL;
	
	result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, VULKAN_ALLOCATOR, &computePipelineLayout);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineSlow("Got Here!");
	
	computePipelineInfo.layout = computePipelineLayout;
	computePipelineInfo.basePipelineHandle = 0; //Not Sure
	computePipelineInfo.basePipelineIndex = 0; //Not Sure
	
	result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, VULKAN_ALLOCATOR, &computePipeline);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineSlow("Got Here!");
	
	
	//Descriptor Set (allocated from a descriptor pool)
	VkDescriptorPoolCreateInfo descriptorPoolInfo;
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
	
	result = vkCreateDescriptorPool(device, &descriptorPoolInfo, VULKAN_ALLOCATOR, &computeDescriptorPool);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	VkDescriptorSetAllocateInfo descriptorSetAllocInfo;
	descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocInfo.pNext = NULL;
	descriptorSetAllocInfo.descriptorPool = computeDescriptorPool;
	descriptorSetAllocInfo.descriptorSetCount = 1;
	descriptorSetAllocInfo.pSetLayouts = &computeDescriptorSetLayout;
	
	VkDescriptorSet vulkanDescriptorSet = NULL;
	result = vkAllocateDescriptorSets(device, &descriptorSetAllocInfo, &vulkanDescriptorSet);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	//Create Image Views
	VkImageViewCreateInfo imgViewInfo;
	imgViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imgViewInfo.pNext = NULL;
	imgViewInfo.flags = 0; //Double Check
	imgViewInfo.image = desktopDuplicationImage;
	imgViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imgViewInfo.format = VK_FORMAT_R32_UINT; //VK_FORMAT_B8G8R8A8_UNORM;
	imgViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imgViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; //Probably neccessary
	imgViewInfo.subresourceRange.baseMipLevel = 0;
	imgViewInfo.subresourceRange.levelCount = 1;
	imgViewInfo.subresourceRange.baseArrayLayer = 0;
	imgViewInfo.subresourceRange.layerCount = 1;
	
	result = vkCreateImageView(device, &imgViewInfo, VULKAN_ALLOCATOR, &desktopDuplicationImageView);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	imgViewInfo.image = yuv10Bit444PlanarTexture;
	imgViewInfo.format = VK_FORMAT_R16_UINT;
	
	result = vkCreateImageView(device, &imgViewInfo, VULKAN_ALLOCATOR, &yuv10Bit444PlanarTextureView);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	
	VkWriteDescriptorSet writeDescriptorSets[3];
	writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSets[0].pNext = NULL;
	writeDescriptorSets[0].dstSet = vulkanDescriptorSet;
	writeDescriptorSets[0].dstBinding = 0;
	writeDescriptorSets[0].dstArrayElement = 0;
	writeDescriptorSets[0].descriptorCount = 1;
	writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;	
	
	VkDescriptorImageInfo descriptorImgInfos[2];
	descriptorImgInfos[0].sampler = VK_NULL_HANDLE; //Not needed since sampling is not performed?
	descriptorImgInfos[0].imageView = desktopDuplicationImageView;
	descriptorImgInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	descriptorImgInfos[1].sampler = VK_NULL_HANDLE; //Not needed since sampling is not performed?	
	descriptorImgInfos[1].imageView = yuv10Bit444PlanarTextureView;
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
	
	VkDescriptorBufferInfo descriptorBufInfo;
	descriptorBufInfo.buffer = lutBuffer;
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
	
	vkUpdateDescriptorSets(device, 3, writeDescriptorSets, 0, NULL);
	
	
	VkCommandBuffer computeCommand = computeCommandBuffers[0];
	result = vkBeginCommandBuffer(computeCommand, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	vulkanWindowImgMemBar[0].image = desktopDuplicationImage;
	vulkanWindowImgMemBar[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vulkanWindowImgMemBar[0].srcQueueFamilyIndex = computeQFI;
	vulkanWindowImgMemBar[0].dstQueueFamilyIndex = computeQFI;
	
	vulkanWindowImgMemBar[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	vulkanWindowImgMemBar[1].pNext = NULL;
	vulkanWindowImgMemBar[1].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	vulkanWindowImgMemBar[1].srcAccessMask = VK_ACCESS_2_NONE;
	vulkanWindowImgMemBar[1].dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	vulkanWindowImgMemBar[1].dstAccessMask = VK_ACCESS_2_NONE;
	vulkanWindowImgMemBar[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vulkanWindowImgMemBar[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
	vulkanWindowImgMemBar[1].srcQueueFamilyIndex = computeQFI;
	vulkanWindowImgMemBar[1].dstQueueFamilyIndex = computeQFI;
	vulkanWindowImgMemBar[1].image = yuv10Bit444PlanarTexture;
	vulkanWindowImgMemBar[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vulkanWindowImgMemBar[1].subresourceRange.baseMipLevel = 0;
	vulkanWindowImgMemBar[1].subresourceRange.levelCount = 1;
	vulkanWindowImgMemBar[1].subresourceRange.baseArrayLayer = 0;
	vulkanWindowImgMemBar[1].subresourceRange.layerCount = 1;
	
	vulkanWindowDependencyInfo.imageMemoryBarrierCount = 2;
	
	vkCmdPipelineBarrier2(computeCommand, &vulkanWindowDependencyInfo);
	
	vkCmdBindPipeline(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &vulkanDescriptorSet, 0, NULL);
	vkCmdDispatch(computeCommand, width >> 4, height >> 2, 1); //Based on shader local_sizes
	
	result = vkEndCommandBuffer(computeCommand);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	return 0;
}

static CUdevice cudaDevice = 0;
static NvidiaCudaFunctions nvCuFun;
static CUcontext nvidiaCudaContext = 0;
static CUexternalMemory cudaImportMem = 0;
static CUmipmappedArray cuExtMipArray = 0;
static CUarray cuExtArray = 0;

static void* nvidiaEncoderLibrary = NULL;
typedef NVENCSTATUS (NVENCAPI *PFN_NvEncodeAPICreateInstance)(NV_ENCODE_API_FUNCTION_LIST *functionList);
static NV_ENCODE_API_FUNCTION_LIST nvEncFunList;
static void* nvEncoder = NULL;
static NV_ENC_CREATE_BITSTREAM_BUFFER nvEncBitstreamBuff0;
static NV_ENC_CREATE_BITSTREAM_BUFFER nvEncBitstreamBuff1;
static NV_ENC_PIC_PARAMS nvEncPicParams;

int setupNvidiaEncoder(uint32_t width, uint32_t height, uint64_t fps, void* ioTempBuffer) {
	int error = nvidiaCudaSetup(&cudaDevice, &nvCuFun);
	RETURN_ON_ERROR(error);
	
	unsigned int cudaContexFlags = 0;
	int cudaContexState = 0;
	CUresult cuRes = nvCuFun.cuDevicePrimaryCtxGetState(cudaDevice, &cudaContexFlags, &cudaContexState);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
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
	cuRes = nvCuFun.cuDevicePrimaryCtxRetain(&nvidiaCudaContext, cudaDevice);
	//cuRes = cuCtxCreate(&nvidiaCudaContext, 0, cudaDevice); //Doesn't have an effect
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_GET_CONTEXT;
	}
	
	//consoleWriteLineSlow("Cuda Context Init");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	cuRes = nvCuFun.cuCtxPushCurrent(nvidiaCudaContext);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_PUSH_CONTEXT;
	}
	
	size_t cudaBytes = 0;
	//cuRes = cuCtxGetLimit(&cudaBytes, CU_LIMIT_STACK_SIZE);
	//if (cuRes != CUDA_SUCCESS) {
	//	nvidiaError = (int) cuRes;
	//	return ERROR_CUDA_CANNOT_GET_LIMIT;
	//}
	//consoleWriteLineWithNumberFast("Cuda Limit: ", 12, cudaBytes, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	cudaBytes = 0;
	cuRes = nvCuFun.cuCtxSetLimit(CU_LIMIT_STACK_SIZE, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	cuRes = nvCuFun.cuCtxSetLimit(CU_LIMIT_PRINTF_FIFO_SIZE, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	cuRes = nvCuFun.cuCtxSetLimit(CU_LIMIT_MALLOC_HEAP_SIZE, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	//cuRes = cuCtxSetLimit(CU_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT, cudaBytes);
	//if (cuRes != CUDA_SUCCESS) {
	//	nvidiaError = (int) cuRes;
	//	return ERROR_CUDA_CANNOT_SET_LIMIT;
	//}
	cuRes = nvCuFun.cuCtxSetLimit(CU_LIMIT_DEV_RUNTIME_SYNC_DEPTH, cudaBytes);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_SET_LIMIT;
	}
	
	//cuRes = cuCtxGetLimit(&cudaBytes, CU_LIMIT_STACK_SIZE);
	//if (cuRes != CUDA_SUCCESS) {
	//	nvidiaError = (int) cuRes;
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
	
	
	//Import Vulkan Memory:
	error = nvidiaCudaImportVulkanMemory(device, yuv10Bit444PlanarTexture, yuv10Bit444PlanarTextureMemory, &cudaImportMem);
	RETURN_ON_ERROR(error);
	
	CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC extMemArray = {};
	extMemArray.offset = 0;
	extMemArray.arrayDesc.Width = width;
	extMemArray.arrayDesc.Height = height * 3;
	extMemArray.arrayDesc.Depth = 0;
	extMemArray.arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT16; //Lossless format matching the Vulkan Exported Memory 
	extMemArray.arrayDesc.NumChannels = 1;
	extMemArray.arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST; //Manditory for NvEnc
	extMemArray.numLevels = 1;
	
	cuRes = nvCuFun.cuExternalMemoryGetMappedMipmappedArray(&cuExtMipArray, cudaImportMem, &extMemArray);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_MAP_MEMORY;
	}
	
	cuRes = nvCuFun.cuMipmappedArrayGetLevel(&cuExtArray, cuExtMipArray, 0);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_GET_ARRAY;
	}
	
	cuRes = nvCuFun.cuCtxPopCurrent(NULL);
	if (cuRes != CUDA_SUCCESS) {
		//nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_POP_CONTEXT;
	}
	
	
	error = ioLoadLibrary(&nvidiaEncoderLibrary, "nvEncodeAPI64");
	RETURN_ON_ERROR(error);
	
	PFN_NvEncodeAPICreateInstance nvidiaEncodeCreateInstance = NULL;
	error = ioGetLibraryFunction(nvidiaEncoderLibrary, "NvEncodeAPICreateInstance", (void**) &nvidiaEncodeCreateInstance);
	RETURN_ON_ERROR(error);
	
	nvEncFunList.version = NV_ENCODE_API_FUNCTION_LIST_VER;
	nvEncFunList.reserved = 0;
	nvEncFunList.reserved1 = NULL;
	NVENCSTATUS nvEncRes = nvidiaEncodeCreateInstance(&nvEncFunList);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_CREATE_INSTANCE;
	}
	
	
	NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS* nvEncSessionParams = (NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS*) ioTempBuffer;
	memzeroBasic((void*) nvEncSessionParams, sizeof(NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS));
	
	nvEncSessionParams->version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
	nvEncSessionParams->deviceType = NV_ENC_DEVICE_TYPE_CUDA;
	nvEncSessionParams->device = (void*) nvidiaCudaContext;
	nvEncSessionParams->apiVersion = NVENCAPI_VERSION;
	
	nvEncRes = nvEncFunList.nvEncOpenEncodeSessionEx(nvEncSessionParams, &nvEncoder);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_OPEN_SESSION;
	}
	
	
	GUID* nvEncGUIDs = (GUID*) ioTempBuffer;
	uint32_t nvEncGUIDcount = 0; //512 bytes = 32 * 16 since 32 is enough for all possible GUIDs for now
	nvEncRes = nvEncFunList.nvEncGetEncodeGUIDs(nvEncoder, nvEncGUIDs, 32, &nvEncGUIDcount);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
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
		//nvidiaError = (int) nvEncRes;
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
		//nvidiaError = (int) nvEncRes;
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
	
	//consoleWriteLineWithNumberFast("Size of: ", 9, sizeof(NV_ENC_PRESET_CONFIG), NUM_FORMAT_UNSIGNED_INTEGER);
	void* presetConfigBuffer = NULL;
	error = memoryAllocate(&presetConfigBuffer, sizeof(NV_ENC_PRESET_CONFIG), 0);
	RETURN_ON_ERROR(error);
	NV_ENC_PRESET_CONFIG* nvPresetConfig = (NV_ENC_PRESET_CONFIG*) presetConfigBuffer;
	nvPresetConfig->version = NV_ENC_PRESET_CONFIG_VER;
	nvPresetConfig->presetCfg.version = NV_ENC_CONFIG_VER;
	nvEncRes = nvEncFunList.nvEncGetEncodePresetConfigEx(nvEncoder, nvEncChosenGUID, nvEncPresetGUID, NV_ENC_TUNING_INFO_LOSSLESS, nvPresetConfig);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_PRESET_CONFIG;
	}	
	
	
	NV_ENC_BUFFER_FORMAT* nvEncInFmts = (NV_ENC_BUFFER_FORMAT*) ioTempBuffer;
	uint32_t nvEncInFmtCount = 0; //128 bytes = 16 * 8 bytes should be enough for all formats
	nvEncRes = nvEncFunList.nvEncGetInputFormats(nvEncoder, nvEncChosenGUID, nvEncInFmts, 16, &nvEncInFmtCount);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
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
	
	NV_ENC_CAPS_PARAM* nvEncCapability = (NV_ENC_CAPS_PARAM*) ioTempBuffer;
	memzeroBasic((void*) nvEncCapability, sizeof(NV_ENC_CAPS_PARAM));
	nvEncCapability->version = NV_ENC_CAPS_PARAM_VER;
	nvEncCapability->capsToQuery = NV_ENC_CAPS_NUM_MAX_BFRAMES;
	int nvEncCapsVal = 0;
	nvEncRes = nvEncFunList.nvEncGetEncodeCaps(nvEncoder, nvEncChosenGUID, nvEncCapability, &nvEncCapsVal);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_GET_CAPABILITY;
	}
	//consoleWriteLineWithNumberFast("Max B-Frames: ", 14, nvEncCapsVal, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//consoleWriteLineSlow("Nvidia Setup");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	//consoleWriteLineWithNumberFast("Size of: ", 9, sizeof(NV_ENC_INITIALIZE_PARAMS), NUM_FORMAT_UNSIGNED_INTEGER);
	NV_ENC_INITIALIZE_PARAMS* nvEncParams = (NV_ENC_INITIALIZE_PARAMS*) ioTempBuffer;
	memzeroBasic((void*) nvEncParams, sizeof(NV_ENC_INITIALIZE_PARAMS));
	
	nvEncParams->version = NV_ENC_INITIALIZE_PARAMS_VER;
	nvEncParams->encodeGUID.Data1 = nvEncChosenGUID.Data1;
	nvEncParams->encodeGUID.Data2 = nvEncChosenGUID.Data2;
	nvEncParams->encodeGUID.Data3 = nvEncChosenGUID.Data3;
	nvEncParams->presetGUID.Data1 = nvEncPresetGUID.Data1;
	nvEncParams->presetGUID.Data2 = nvEncPresetGUID.Data2;
	nvEncParams->presetGUID.Data3 = nvEncPresetGUID.Data3;
	for (uint32_t d=0; d<8; d++) {
		nvEncParams->encodeGUID.Data4[d] = nvEncChosenGUID.Data4[d];
		nvEncParams->presetGUID.Data4[d] = nvEncPresetGUID.Data4[d];
	}
	nvEncParams->encodeWidth = width;
	nvEncParams->encodeHeight = height;
	uint32_t gcd = greatestCommonDivisor(width, height);
	nvEncParams->darWidth = width / gcd; //16;
	nvEncParams->darHeight = height / gcd; //9;
	
	//consoleWriteLineWithNumberFast("DAR Width:  ", 12, nvEncParams->darWidth, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("DAR Height: ", 12, nvEncParams->darHeight, NUM_FORMAT_UNSIGNED_INTEGER);
	
	
	nvEncParams->frameRateNum = fps;
	nvEncParams->frameRateDen = 1;
	nvEncParams->enableEncodeAsync = 0; //Lots more work to enable Async and probably not worth it
	nvEncParams->enablePTD = 1; //Enabling the picture type decision to be made by the encoder
	nvEncParams->reportSliceOffsets = 0;
	nvEncParams->enableSubFrameWrite = 0;
	nvEncParams->enableExternalMEHints = 0;
	nvEncParams->enableMEOnlyMode = 0;
	nvEncParams->enableWeightedPrediction = 0;
	nvEncParams->splitEncodeMode = 0; //Not certain
	nvEncParams->enableOutputInVidmem = 0;
	nvEncParams->enableReconFrameOutput = 0;
	nvEncParams->enableOutputStats = 0;
	nvEncParams->reservedBitFields = 0;
	nvEncParams->privDataSize = 0;
	nvEncParams->privData = NULL;
	nvEncParams->privDataSize = 0;
	
	
	//Manual Adjust of Preset Parameters
	nvPresetConfig->presetCfg.profileGUID.Data1 = nvEncProfileGUID.Data1;
	nvPresetConfig->presetCfg.profileGUID.Data2 = nvEncProfileGUID.Data2;
	nvPresetConfig->presetCfg.profileGUID.Data3 = nvEncProfileGUID.Data3;
	for (uint32_t d=0; d<8; d++) {
		nvPresetConfig->presetCfg.profileGUID.Data4[d] = nvEncProfileGUID.Data4[d];
	}
	
	//consoleWriteLineWithNumberFast("Rate Control: ", 14, nvPresetConfig->presetCfg.gopLength, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Rate Control: ", 14, nvPresetConfig->presetCfg.frameIntervalP, NUM_FORMAT_UNSIGNED_INTEGER);
	nvPresetConfig->presetCfg.gopLength = NVENC_INFINITE_GOPLENGTH; //For realtime encoding
	nvPresetConfig->presetCfg.frameIntervalP = 1;
	
	//nvPresetConfig->presetCfg.monoChromeEncoding = 0;
	//nvPresetConfig->presetCfg.frameFieldMode = NV_ENC_PARAMS_FRAME_FIELD_MODE_FRAME;
	//nvPresetConfig->presetCfg.mvPrecision = NV_ENC_MV_PRECISION_DEFAULT;
	
	nvEncParams->encodeConfig = &(nvPresetConfig->presetCfg);
	//consoleWriteLineWithNumberFast("Rate Control: ", 14, nvPresetConfig->presetCfg.rcParams.rateControlMode, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("AQ Enable: ", 11, nvPresetConfig->presetCfg.rcParams.enableAQ, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Low Delay: ", 11, nvPresetConfig->presetCfg.rcParams.lowDelayKeyFrameScale, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//consoleWriteLineWithNumberFast("SPSPPS Disable: ", 16, nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.disableSPSPPS, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("SPSPPS Repeat:  ", 16, nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.repeatSPSPPS, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Enable Intra:   ", 16, nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.enableIntraRefresh, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Intra Period:   ", 16, nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.intraRefreshPeriod, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Intra Count:    ", 16, nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.intraRefreshCnt, NUM_FORMAT_UNSIGNED_INTEGER);
	
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.chromaFormatIDC = 3; //1 for 4:2:0, 3 for 4:4:4 to peserve all chroma data (full lossless)
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 2;
		
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.maxNumRefFramesInDPB = 2; //Good reduction of previous reference frames
	//nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.numRefL0 = NV_ENC_NUM_REF_FRAMES_2; //Needed?
	//nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.numRefL1 = NV_ENC_NUM_REF_FRAMES_2; //Needed?
	
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.videoSignalTypePresentFlag = 1;
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.videoFormat = NV_ENC_VUI_VIDEO_FORMAT_COMPONENT; //?
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.videoFullRangeFlag = 1;
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.colourDescriptionPresentFlag = 1;
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.colourPrimaries = NV_ENC_VUI_COLOR_PRIMARIES_BT709;
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.transferCharacteristics = NV_ENC_VUI_TRANSFER_CHARACTERISTIC_BT709;
	nvPresetConfig->presetCfg.encodeCodecConfig.hevcConfig.hevcVUIParameters.colourMatrix = NV_ENC_VUI_MATRIX_COEFFS_BT709;
	
	
	nvEncParams->maxEncodeWidth = width; //0?
	nvEncParams->maxEncodeHeight = height; //0?
	
	nvEncParams->tuningInfo = NV_ENC_TUNING_INFO_LOSSLESS;
	nvEncParams->bufferFormat = nvEncChosenFormat; //Only used when device is DX12
	
	nvEncRes = nvEncFunList.nvEncInitializeEncoder(nvEncoder, nvEncParams);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_INITIALIZE;
	}
	
	error = memoryDeallocate(&presetConfigBuffer);
	RETURN_ON_ERROR(error);
	
	//consoleWriteLineSlow("Nvidia Init Encoder");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	NV_ENC_REGISTER_RESOURCE* nvEncInputResource = (NV_ENC_REGISTER_RESOURCE*) ioTempBuffer;
	memzeroBasic((void*) nvEncInputResource, sizeof(NV_ENC_REGISTER_RESOURCE));
	nvEncInputResource->version = NV_ENC_REGISTER_RESOURCE_VER;
	nvEncInputResource->resourceType = NV_ENC_INPUT_RESOURCE_TYPE_CUDAARRAY;
	nvEncInputResource->width = width;
	nvEncInputResource->height = height;
	nvEncInputResource->pitch = width * 2;
	
	nvEncInputResource->subResourceIndex = 0; //0 for CUDA
	nvEncInputResource->resourceToRegister = (void*) cuExtArray;
	
	nvEncInputResource->bufferFormat = nvEncChosenFormat;
	nvEncInputResource->bufferUsage = NV_ENC_INPUT_IMAGE;
	
	nvEncInputResource->pInputFencePoint = NULL; //Used for DX12
	
	nvEncRes = nvEncFunList.nvEncRegisterResource(nvEncoder, nvEncInputResource);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_REGISTER_RES;
	}
	
	NV_ENC_MAP_INPUT_RESOURCE nvEncMappedInput = {0};
	nvEncMappedInput.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
	nvEncMappedInput.registeredResource = nvEncInputResource->registeredResource;
	
	nvEncRes = nvEncFunList.nvEncMapInputResource(nvEncoder, &nvEncMappedInput);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
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
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_CREATE_BITSTREAM;
	}
	
	//Extra safe
	nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff0.bitstreamBuffer);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_UNLOCK_BITSTREAM;
	}
	
	nvEncBitstreamBuff1.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
	nvEncRes = nvEncFunList.nvEncCreateBitstreamBuffer(nvEncoder, &nvEncBitstreamBuff1);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_CREATE_BITSTREAM;
	}
	
	//Extra safe
	nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff1.bitstreamBuffer);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = (int) nvEncRes;
		return ERROR_NVENC_CANNOT_UNLOCK_BITSTREAM;
	}
	
	//consoleWriteLineSlow("Nvidia Bitstreams Create");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	
	nvEncPicParams.version = NV_ENC_PIC_PARAMS_VER;
	nvEncPicParams.inputWidth = width;
	nvEncPicParams.inputHeight = height;
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

int loadVulkanLUT() {
	
	uint32_t* lutBufferPtr = NULL;
	VkResult result = vkMapMemory(device, stageBufferMemory, 0, VK_WHOLE_SIZE, 0, (void**) &lutBufferPtr);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_MAP_FAILED;
	}
	
	populateSRGBtoXVYCbCrLUT(lutBufferPtr, 1, 1);
	
	vkUnmapMemory(device, stageBufferMemory);
	
	//Copy from lut data from staging to LUT
	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = NULL;
	submitInfo.pWaitDstStageMask = NULL;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffers[0];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = NULL;

	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue); //Fence in future
	
	return 0;
}

int encodeOneFrame() {
	//Load Up Output Bitstream File
	void* imgFile = NULL;
	int error = ioOpenFile(&imgFile, "image0.rgb", -1, IO_FILE_WRITE_NORMAL);
	RETURN_ON_ERROR(error);
	
	void* h265File = NULL;
	error = ioOpenFile(&h265File, "bitstream0.h265", -1, IO_FILE_WRITE_NORMAL);
	RETURN_ON_ERROR(error);
	
	//Copy Image to Staging
	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = NULL;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = NULL;
	submitInfo.pWaitDstStageMask = NULL;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffers[1];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = NULL;
	
	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue); //Fence in future
	
	void* imgData = NULL;
	VkResult result = vkMapMemory(device, stageBufferMemory, 0, VK_WHOLE_SIZE, 0, &imgData);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_MAP_FAILED;
	}
	
	//Write Image Data to file
	error = ioWriteFile(imgFile, imgData, 8294400); 
	RETURN_ON_ERROR(error);
	
	error = ioCloseFile(&imgFile);
	RETURN_ON_ERROR(error);
	
	vkUnmapMemory(device, stageBufferMemory);
	
	uint64_t sTime = getCurrentTime();
	submitInfo.pCommandBuffers = &computeCommandBuffers[0];
	vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(computeQueue); //Fence in future
	uint64_t eTime = getCurrentTime();
	uint64_t uTime = getDiffTimeMicroseconds(sTime, eTime);
	consoleWriteLineWithNumberFast("Microseconds: ", 14, uTime, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//NvEnc Run
	NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = nvEncRes;
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
		//nvidiaError = nvEncRes;
		return ERROR_NVENC_EXTRA_INFO;
	}
	
	//Write output to file
	error = ioWriteFile(h265File, nvEncBitstreamLock.bitstreamBufferPtr, nvEncBitstreamLock.bitstreamSizeInBytes); 
	RETURN_ON_ERROR(error);
	
	error = ioCloseFile(&h265File);
	RETURN_ON_ERROR(error);
	
	nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff0.bitstreamBuffer);
	if (nvEncRes != NV_ENC_SUCCESS) {
		//nvidiaError = nvEncRes;
		return ERROR_NVENC_EXTRA_INFO;
	}
	
	return 0;
}

static NV_ENC_LOCK_BITSTREAM ddEncodeBitstreamLock0 = {0};
static NV_ENC_LOCK_BITSTREAM ddEncodeBitstreamLock1 = {0};

static void* ddThreadEndEvent = NULL;
static void* ddEncodeEvent = NULL;
static void* ddLockEvent = NULL;

static int ddEncodeLockThread() {
	//consolePrintLine(41);
	uint64_t bitTest = 0;
	NV_ENC_LOCK_BITSTREAM* bitstreamToLock = &ddEncodeBitstreamLock0;
	uint64_t signal = 0;
	int error = syncEventCheck(ddThreadEndEvent, &signal);
	RETURN_ON_ERROR(error);
	while (signal == 0) {
		//consolePrintLine(41);
		error = syncEventWait(ddEncodeEvent);
		RETURN_ON_ERROR(error);
		
		NVENCSTATUS nvEncRes = nvEncFunList.nvEncLockBitstream(nvEncoder, bitstreamToLock);
		if (nvEncRes != NV_ENC_SUCCESS) {
			return nvEncRes;
		}
		
		error = syncSetEvent(ddLockEvent);
		RETURN_ON_ERROR(error);
		
		bitTest ^= 1; //XOR with 1
		if (bitTest == 0) {
			bitstreamToLock = &ddEncodeBitstreamLock0;
		}
		else {
			bitstreamToLock = &ddEncodeBitstreamLock1;
		}
		
		error = syncEventCheck(ddThreadEndEvent, &signal);
	}
	return 0;
}

static void* ddEncodeLockThreadHandle = NULL;

static uint8_t ddReservedNAL[10];
static uint32_t* ddReservedNALsize = NULL;
static uint64_t ddWriteOffset = 0;

static VkSubmitInfo ddComputeSubmitInfo;
static VkFence ddComputeFence = VK_NULL_HANDLE;

static uint64_t ddAcquireLatencySum = 0;
static uint64_t ddComputeLatencySum = 0;
static uint64_t ddEncodeLatencySum = 0;
static uint64_t ddAcquireCount = 0;
static uint64_t ddComputeCount = 0;
static uint64_t ddEncodeCount = 0;
static uint64_t ddComputeStartTime = 0;
static uint64_t ddEncodeStartTime = 0;
static uint64_t ddRepeatCount = 0;
static uint64_t ddAcquireMissedTiming = 0;
static uint64_t ddMiscIssues = 0;
static uint64_t ddAccumulatedFramesSum = 0;

static uint64_t ddState = 0;
static uint64_t ddNextFrame = 0;
static uint64_t ddCounterIDRreset = 0;
static uint64_t ddCounterIDR = 0;
static uint64_t ddFrameIntervalTime = 0;
static uint64_t ddFirstFrameStartTime = 0;
static uint64_t ddAcquireOffset = 0;

int ddEncodeStart(uint64_t fps) {
	//Release Frame
	int error = graphicsDesktopDuplicationReleaseFrame();
	RETURN_ON_ERROR(error);
	
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
	
	
	error = syncCreateEvent(&ddThreadEndEvent, 0, 0);
	RETURN_ON_ERROR(error);
	error = syncCreateEvent(&ddEncodeEvent, 0, 0);
	RETURN_ON_ERROR(error);
	error = syncCreateEvent(&ddLockEvent, 0, 0);
	RETURN_ON_ERROR(error);
	
	PFN_ThreadStart threadStart = ddEncodeLockThread;
	error = syncStartThread(&ddEncodeLockThreadHandle, threadStart, 0);
	RETURN_ON_ERROR(error);
	
	ddReservedNAL[0] = 0;
	ddReservedNAL[1] = 0;
	ddReservedNAL[2] = 0;
	ddReservedNAL[3] = 1;
	ddReservedNAL[4] = 84;
	ddReservedNAL[5] = 1;
	ddReservedNALsize = (uint32_t*) (&(ddReservedNAL[6]));
	
	ddWriteOffset = 0;
		
	//Create the Vulkan Compute Finish Fence
	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = NULL;
	fenceInfo.flags = 0;
	
	VkResult result = vkCreateFence(device, &fenceInfo, VULKAN_ALLOCATOR, &ddComputeFence);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	
	//Fill in Compute and Bitstream Lock Structures
	ddComputeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	ddComputeSubmitInfo.pNext = NULL;
	ddComputeSubmitInfo.waitSemaphoreCount = 0;
	ddComputeSubmitInfo.pWaitSemaphores = NULL;
	ddComputeSubmitInfo.pWaitDstStageMask = NULL;
	ddComputeSubmitInfo.commandBufferCount = 1;
	ddComputeSubmitInfo.pCommandBuffers = &computeCommandBuffers[0];
	ddComputeSubmitInfo.signalSemaphoreCount = 0;//1;
	ddComputeSubmitInfo.pSignalSemaphores = NULL;//&vulkanComputeSemaphore;
	//vkQueueSubmit(computeQueue, 1, &ddComputeSubmitInfo, VK_NULL_HANDLE);
	
	consoleBufferFlush();
	
	uint64_t presentationInfo = 0;
	uint64_t accumulatedFrames = 0;
	error = graphicsDesktopDuplicationAcquireNextFrame(1000 / fps, &presentationInfo, &accumulatedFrames);
	RETURN_ON_ERROR(error);
	//if (error == ERROR_DESKDUPL_ACQUIRE_TIMEOUT) {
	//	
	//}
	
	uint64_t currentTime = getCurrentTime();
	
	//Start Compute Immediately:
	vkQueueSubmit(computeQueue, 1, &ddComputeSubmitInfo, ddComputeFence);
	ddAcquireLatencySum = 0;//currentTime - ddLastPresentationTime;
	ddComputeLatencySum = 0;
	ddEncodeLatencySum = 0;
	ddAcquireCount = 0; //1
	ddComputeCount = 0;
	ddEncodeCount = 0;
	ddComputeStartTime = currentTime;
	ddEncodeStartTime = 0;
	ddRepeatCount = 0; //Num Duplicate Frames Encoded
	ddAcquireMissedTiming = 0;
	ddMiscIssues = 0;
	ddAccumulatedFramesSum = 0;
	
	//Setup Run Variables:
	ddState = 0b0001000; //Bits: Frame Released | Compute Start Wait | Encode Start Wait | Compute Stage Active | Encoding Active | Write 1 | Write 0
	ddNextFrame = 1;
	ddCounterIDR = 0;
	ddCounterIDRreset = fps * 3;
	
	ddFrameIntervalTime = getFrameIntervalTime(fps);
	ddFirstFrameStartTime = currentTime; // + (ddFrameIntervalTime >> 1);
	ddAcquireOffset = 500 * getMicrosecondDivider();
	
	return 0;
}

int ddEncodeRun(void* bitstreamFilePtr, uint64_t* frameWriteCount) {
	int error = 0;
	uint64_t signaled = 0;
	
	if ((ddState & 64) > 0) { //Frame Released
		uint64_t frameStartTime = ddFirstFrameStartTime + (ddNextFrame * ddFrameIntervalTime);
		uint64_t frameEndTime = frameStartTime + ddFrameIntervalTime;
		uint64_t acquireStartTime = frameStartTime + ddAcquireOffset;
		uint64_t acquireEndTime = frameEndTime + ddAcquireOffset;
		uint64_t currentTime = getCurrentTime();
		if (currentTime >= acquireStartTime) {
			if (currentTime < acquireEndTime) {
				//consoleWriteLineFast("Acquiring Image", 15);
				uint64_t presentationTime = 0;
				uint64_t accumulatedFrames = 0;
				error = graphicsDesktopDuplicationAcquireNextFrame(1, &presentationTime, &accumulatedFrames);
				if (error == 0) { //Acquired Something (Might just be mouse stuff)
					if (presentationTime >= frameStartTime) { //Actually acquired image and it is in the expected time
						currentTime = getCurrentTime();
						ddAcquireLatencySum += currentTime - presentationTime;
						ddAcquireCount++;
						ddAccumulatedFramesSum += accumulatedFrames;
						if (presentationTime < frameEndTime) { //Image is valid for current frame
							ddNextFrame++;
							if ((ddState & 32) > 0) {
								ddMiscIssues++;
							}
							ddState |= 32;
							ddState &= ~64;
						}
						else { //Got a frame but its for the next acquire period so first step is to encode the duplicate
							ddNextFrame += 2;
							if ((ddState & 0b110000) > 0) {
								ddMiscIssues++;
							}
							ddRepeatCount++;
							ddState |= 16 | 32;
							ddState &= ~64;
						}
					}
					else { //probably acquired mouse change info... need to release frame
						error = graphicsDesktopDuplicationReleaseFrame();
						RETURN_ON_ERROR(error);
					}
				}
				else if (error != ERROR_DESKDUPL_ACQUIRE_TIMEOUT) { //Failed to acquire next frame but not to timeout
					return error;
				}
			}
			else {
				ddAcquireMissedTiming++; // Missed Timing
				//ddNextFrame++;
			}
			if ((ddState & 64) > 0) { //Encode duplicate frame if did not acquire new frame
				currentTime = getCurrentTime();
				if (currentTime >= frameEndTime) {
					ddNextFrame++;
					if ((ddState & 16) > 0) {
						ddMiscIssues++;
					}
					ddRepeatCount++;
					ddState |= 16;
				}
			}
		}
	}
	
	if ((ddState & 1) > 0) { //Write Output 0 Check
		//Check on async file write here
		//consoleWriteLineFast("Write 0 Check", 13);
		error = ioAsyncSignalCheck(0, &signaled);
		RETURN_ON_ERROR(error);
		if (signaled == 1) { //Write Event Signaled
			NVENCSTATUS nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff0.bitstreamBuffer);
			if (nvEncRes != NV_ENC_SUCCESS) {
				//nvidiaError = nvEncRes;
				return ERROR_NVENC_EXTRA_INFO;
			}
			(*frameWriteCount)++;
			//consoleWriteLineFast("Wrote to File 0", 15);
			
			error = ioAsyncSignalWait(2);
			RETURN_ON_ERROR(error);
			ddState &= ~1;
		}
	}
	if ((ddState & 2) > 0) { //Write Output 1 Check
		//Check on async file write here
		//consoleWriteLineFast("Write 1 Check", 13);
		error = ioAsyncSignalCheck(1, &signaled);
		RETURN_ON_ERROR(error);
		if (signaled == 1) { //Write Event Signaled
			NVENCSTATUS nvEncRes = nvEncFunList.nvEncUnlockBitstream(nvEncoder, nvEncBitstreamBuff1.bitstreamBuffer);
			if (nvEncRes != NV_ENC_SUCCESS) {
				//nvidiaError = nvEncRes;
				return ERROR_NVENC_EXTRA_INFO;
			}
			(*frameWriteCount)++;
			//consoleWriteLineFast("Wrote to File 1", 15);
			
			error = ioAsyncSignalWait(3);
			RETURN_ON_ERROR(error);
			ddState &= ~2;
		}
	}
	
	if ((ddState & 4) > 0) { //Encoding Wait Check
		//Lock Bitstream to "finish" encoding step
		//consoleWriteLineFast("Encode Check", 12);
		error = syncEventCheck(ddLockEvent, &signaled);
		RETURN_ON_ERROR(error);
		if (signaled == 1) { //Lock Event Signaled
			//consoleWriteLineFast("Locked Bitstream", 16);
			
			uint64_t currentTime = getCurrentTime();
			ddEncodeLatencySum += currentTime - ddEncodeStartTime;
			ddEncodeCount++;			
			
			if ((ddEncodeCount & 1) > 0) { //Odd Frame
				//consoleWriteLineFast("Write 0", 7);
				
				*ddReservedNALsize = (uint32_t) ddEncodeBitstreamLock0.bitstreamSizeInBytes;
				error = ioAsyncWriteFile(bitstreamFilePtr, ddReservedNAL, 10, 2, ddWriteOffset);
				RETURN_ON_ERROR(error);
				ddWriteOffset += 10;
				
				//Start Async Write Here
				error = ioAsyncWriteFile(bitstreamFilePtr, ddEncodeBitstreamLock0.bitstreamBufferPtr, ddEncodeBitstreamLock0.bitstreamSizeInBytes, 0, ddWriteOffset);
				RETURN_ON_ERROR(error);
				ddWriteOffset += ddEncodeBitstreamLock0.bitstreamSizeInBytes;
				
				ddState |= 1;
			}
			else { //Even Frame
				//consoleWriteLineFast("Write 1", 7);
				
				*ddReservedNALsize = (uint32_t) ddEncodeBitstreamLock1.bitstreamSizeInBytes;
				error = ioAsyncWriteFile(bitstreamFilePtr, ddReservedNAL, 10, 3, ddWriteOffset);
				RETURN_ON_ERROR(error);
				ddWriteOffset += 10;
				
				//Start Async Write Here
				error = ioAsyncWriteFile(bitstreamFilePtr, ddEncodeBitstreamLock1.bitstreamBufferPtr, ddEncodeBitstreamLock1.bitstreamSizeInBytes, 1, ddWriteOffset);
				RETURN_ON_ERROR(error);
				ddWriteOffset += ddEncodeBitstreamLock1.bitstreamSizeInBytes;
				
				ddState |= 2;
			}
			ddState &= ~4;
		}
	}
	
	if ((ddState & 8) > 0) { //Compute Wait Check
		//consoleWriteLineFast("Compute Check", 13);
		VkResult vkRes = vkGetFenceStatus(device, ddComputeFence);
		if (vkRes == VK_SUCCESS) { //Can Now Encode and Release Desktop Duplication
			
			uint64_t currentTime = getCurrentTime();
			ddComputeLatencySum += currentTime - ddComputeStartTime;
			ddComputeCount++;
			
			error = graphicsDesktopDuplicationReleaseFrame();
			RETURN_ON_ERROR(error);
			
			vkResetFences(device, 1, &ddComputeFence);
			
			ddState |= 64 | 16; //Released Frame | Encode Start Wait
			ddState &= ~8;
		}
		else if (vkRes == VK_NOT_READY) {
			return 0; //The Compute is a bottleneck / showstopper
		}
		else {
			return ERROR_VULKAN_EXTRA_INFO;
		}
	}
	
	if ((ddState & 16) > 0) { //Encoding Start Wait Check
		//consoleWriteLineFast("Encode Start Check", 18);
		uint64_t writeCheck = ddState & 1;
		if ((ddEncodeCount & 1) > 0) {
			writeCheck = ddState & 2;
		}
		if (((ddState & 0b1100) == 0) && (writeCheck == 0)) {
			ddEncodeStartTime = getCurrentTime();
			
			if (ddCounterIDR > 0) {
			nvEncPicParams.encodePicFlags = 0; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_P;
			ddCounterIDR--;
			}
			else {
				nvEncPicParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEINTRA; //nvEncPicParams.pictureType = NV_ENC_PIC_TYPE_IDR; //NV_ENC_PIC_TYPE_I;
				ddCounterIDR = ddCounterIDRreset;
			}
			if ((ddEncodeCount & 1) > 0) {
				nvEncPicParams.outputBitstream = nvEncBitstreamBuff1.bitstreamBuffer;
			}
			else {
				nvEncPicParams.outputBitstream = nvEncBitstreamBuff0.bitstreamBuffer;
			}
			
			NVENCSTATUS nvEncRes = nvEncFunList.nvEncEncodePicture(nvEncoder, &nvEncPicParams);
			if (nvEncRes != NV_ENC_SUCCESS) {
				//nvidiaError = nvEncRes;
				return ERROR_NVENC_EXTRA_INFO;
			}
			
			error = syncSetEvent(ddEncodeEvent);
			RETURN_ON_ERROR(error);
			
			ddState |= 4;
			ddState &= ~16;
		}
	}
	
	if ((ddState & 32) > 0) { //Compute Start Wait Check
		//consoleWriteLineFast("Compute Start Check", 19);
		if ((ddState & 0b11100) == 0) {
			ddComputeStartTime = getCurrentTime();
			vkQueueSubmit(computeQueue, 1, &ddComputeSubmitInfo, ddComputeFence);
			
			ddState |= 8;
			ddState &= ~32;
		}
	}
	//consoleBufferFlush();
	
	return 0;
}

int ddEncodePrintStats() {
	uint64_t microsecondDivider = getMicrosecondDivider();
	if (ddAcquireCount > 0) {
		uint64_t latency = ddAcquireLatencySum / ddAcquireCount;
		latency /= microsecondDivider;
		consolePrintLineWithNumber(44, latency, NUM_FORMAT_UNSIGNED_INTEGER);
	}
	if (ddComputeCount > 0) {
		uint64_t latency = ddComputeLatencySum / ddComputeCount;
		latency /= microsecondDivider;
		consolePrintLineWithNumber(45, latency, NUM_FORMAT_UNSIGNED_INTEGER);
	}
	if (ddEncodeCount > 0) {
		uint64_t latency = ddEncodeLatencySum / ddEncodeCount;
		latency /= microsecondDivider;
		consolePrintLineWithNumber(46, latency, NUM_FORMAT_UNSIGNED_INTEGER);
	}
	
	consolePrintLineWithNumber(47, ddRepeatCount, NUM_FORMAT_UNSIGNED_INTEGER);
	consolePrintLineWithNumber(48, ddAcquireMissedTiming, NUM_FORMAT_UNSIGNED_INTEGER);
	consolePrintLineWithNumber(49, ddMiscIssues, NUM_FORMAT_UNSIGNED_INTEGER);
	consolePrintLineWithNumber(50, ddAccumulatedFramesSum, NUM_FORMAT_UNSIGNED_INTEGER);
	
	return 0;
}


//Program Main Function
int programMain() {
	int error = 0;
	//consoleControl(CON_NEW_LINE, 0);
	//consolePrintLine(25);
	
	uint64_t fps = 60;
	uint64_t recordSeconds = 60;
	
	//Desktop Duplication Setup:
	consolePrintLine(26);
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t venderID = 0;
	error = graphicsDesktopDuplicationSetup(&width, &height, &venderID);
	RETURN_ON_ERROR(error);
	consolePrintLine(27);
	
	if (venderID != NVIDIA_PCI_VENDER_ID) {
		consolePrintLine(36);
		graphicsDesktopDuplicationCleanup();
		return 1;
	}
	
	//Vulkan Compute Setup:
	consolePrintLine(28);
	error = setupVulkanCompute(width, height);
	RETURN_ON_ERROR(error);
	consolePrintLine(29);
	
	//Nvidia Cuda Setup:
	consolePrintLine(30);
	void* memPagePtr = NULL;
	uint64_t memPageBytes = 0;
	error = memoryAllocateOnePage(&memPagePtr, &memPageBytes);
	RETURN_ON_ERROR(error);
	error = setupNvidiaEncoder(width, height, fps, memPagePtr);
	RETURN_ON_ERROR(error);
	consolePrintLine(31);
	
	error = loadVulkanLUT();
	RETURN_ON_ERROR(error);
	
	//error = encodeOneFrame();
	//RETURN_ON_ERROR(error);
	
	//*
	consolePrintLine(37);
	void* h265File = NULL;
	error = ioOpenFile(&h265File, "bitstream.h265", -1, IO_FILE_WRITE_ASYNC);
	RETURN_ON_ERROR(error);
	error = ioAsyncSetup(4);
	RETURN_ON_ERROR(error);
	consolePrintLine(38);
	
	consolePrintLine(39);
	consoleBufferFlush();
	consoleWaitForEnter();
	consolePrintLine(40);
	
	error = ddEncodeStart(fps);
	RETURN_ON_ERROR(error);
	
	consoleBufferFlush();
		
	//*
	
	uint64_t numOfFrames = fps * recordSeconds;
	uint64_t numWrittenFrames = 0;
	while (numWrittenFrames < numOfFrames) {
		error = ddEncodeRun(h265File, &numWrittenFrames);
		if (error != 0) {
			break; //Need to handle the possible errors in the future
		}
	}
	
	//*/
	int errorBackup = error;
	
	//Close (and Save) Output Bitstream File
	error = ioCloseFile(&h265File);
	RETURN_ON_ERROR(error);
	
	if (errorBackup == 0) {
		consolePrintLine(42);
	}
	else {
		consolePrintLine(43);
		RETURN_ON_ERROR(errorBackup);
	}
	
	//ddEncodeStop();
	
	//consoleWriteLineSlow("Got Here!");
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	ddEncodePrintStats();
	consoleBufferFlush();
	
	//Cleanup here in the future
	
	return 0;
}

