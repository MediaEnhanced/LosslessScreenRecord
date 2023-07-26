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


//Media Enhanced Windows Graphics Compatibility Implementation
//All input strings are in UTF-8 but converts to UTF-16
//when neccessary to communicate with the win32 API
//Complete error checking is still a work in progress
#define COMPATIBILITY_NETWORK_UNNEEDED
#include "compatibility.h" //Includes stdint.h
//#include "math.h" //Includes the math function definitions (needed for gcd)

#define UNICODE //defined before any includes, dictates that Windows function defaults will work with Unicode UTF-16(LE) encoded strings
#define _UNICODE //similar definition
#define WIN32_LEAN_AND_MEAN //excludes several unnecessary includes when using windows.h
//#define _WIN32_WINNT _WIN32_WINNT_WIN10 //Define that Windows 10 is targeted
//#define WINVER _WIN32_WINNT_WIN10 //similar definition
#include <windows.h> //Includes win32 functions and helper macros (uses the above defines)

// Graphics State Codes:
//#define GRAPHICS_STATE_UNDEFINED 0
//#define GRAPHICS_STATE_SETUP 1
//static uint64_t graphicsState = GRAPHICS_STATE_UNDEFINED;

//Graphics Definitions and Includes
#define INITGUID //So there is no need to link to libdxguid.a (smaller .rdata section size too)
#include <dxgi1_6.h> //Needed to get adapters (GPU devices)
#include <d3d11.h>   //Windows DirectX 11: Version 11.1 is needed for Desktop Duplication

static HRESULT graphicsError = S_OK;
void graphicsGetError(int* error) {
	*error = (int) graphicsError;
}

static IDXGIAdapter* graphicsAdapter = NULL; //Primary Adapter Interface Pointer
static LUID graphicsAdapterID = {0, 0};
static UINT graphicsVenderID = 0; //Necessary in the future?

static int graphicsSetupAdapter(uint64_t temporary) {
	if (graphicsAdapter != NULL) {
		return 0;
	}
	
	IDXGIFactory6* dxgiFactoryInterface = NULL;
	HRESULT hrRes = CreateDXGIFactory1(&IID_IDXGIFactory6, (void**) &dxgiFactoryInterface);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_CREATE_FACTORY;
	}
	
	//DXGI_GPU_PREFERENCE_UNSPECIFIED: First returns the adapter (GPU device) with the output on which the desktop primary is displayed
	//DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE: Get the adapter (GPU device) by choosing the most performant discrete one (if it exisits and there is no "external" GPU device)
	hrRes = dxgiFactoryInterface->lpVtbl->EnumAdapterByGpuPreference(dxgiFactoryInterface, 0, DXGI_GPU_PREFERENCE_UNSPECIFIED, &IID_IDXGIAdapter, (void**) &graphicsAdapter);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_ENUM_ADAPTER;
	}
	dxgiFactoryInterface->lpVtbl->Release(dxgiFactoryInterface);
	
	IDXGIAdapter1* adapter1Interface = NULL;
	hrRes = graphicsAdapter->lpVtbl->QueryInterface(graphicsAdapter, &IID_IDXGIAdapter1, (void**) &adapter1Interface);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_ENUM_OUTPUT;
	}
	
	DXGI_ADAPTER_DESC1 adapterDescription; //GPU Info
	hrRes = adapter1Interface->lpVtbl->GetDesc1(adapter1Interface, &adapterDescription);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_ADAPTER_DESC;
	}
	
	//Save the adapter ID
	graphicsAdapterID.LowPart = adapterDescription.AdapterLuid.LowPart;
	graphicsAdapterID.HighPart = adapterDescription.AdapterLuid.HighPart;
	//consoleWriteLineWithNumberFast("UUID Low:  ", 11, graphicsAdapterID.LowPart, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("UUID High: ", 11, graphicsAdapterID.HighPart, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	graphicsVenderID = adapterDescription.VendorId;
	//consoleWriteLineWithNumberFast("Vender ID: ", 11, graphicsVenderID, NUM_FORMAT_UNSIGNED_INTEGER);
	
	adapter1Interface->lpVtbl->Release(adapter1Interface);
	
	if (temporary > 0) {
		graphicsAdapter->lpVtbl->Release(graphicsAdapter);
		graphicsAdapter = NULL;
	}
	
	return 0;
}

static ID3D11Device* graphicsDesktopDuplicationDevice = NULL; //Device Interface Pointer
static IDXGIOutputDuplication* graphicsDesktopDuplicationPtr = NULL;
static uint32_t graphicsDesktopDuplicationWidth = 0;
static uint32_t graphicsDesktopDuplicationHeight = 0;
static HANDLE graphicsDesktopDuplicationTextureHandle = NULL;
static IDXGIKeyedMutex* graphicsDesktopDuplicationKeyedMutex = NULL;

int graphicsDesktopDuplicationSetup(uint32_t* width, uint32_t* height, uint32_t* venderID) {
	if (width == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (height == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (venderID == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	
	int error = graphicsSetupAdapter(0);
	RETURN_ON_ERROR(error);
	
	//Creates a DirectX 11.1 Device by using chosen adapter
	//Necessary to target DirectX 11.1 which supports at least DXGI 1.2 which is used for desktop duplication
	//This function allocates a lot of memory... try to reduce in the future...
	D3D_FEATURE_LEVEL minDXfeatureTarget = D3D_FEATURE_LEVEL_11_1; //Minimum Direct X 11 Feature Level Target to Perform Desktop Duplication
	UINT creationFlags = 0;//D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT; //Maybe used to debug in future
	HRESULT hrRes = D3D11CreateDevice(graphicsAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, creationFlags, &minDXfeatureTarget, 1, D3D11_SDK_VERSION, &graphicsDesktopDuplicationDevice, NULL, NULL);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_CREATE_DEVICE;
	}
	
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	//Create DXGI Output (Represents a monitor)
	IDXGIOutput* dxgiOutputInterface = NULL; 
	hrRes = graphicsAdapter->lpVtbl->EnumOutputs(graphicsAdapter, 0, &dxgiOutputInterface);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_ENUM_OUTPUT;
	}
	
	//Get Version 6 of the DXGI Output
	IDXGIOutput6* dxgiOutput6Interface = NULL;
	hrRes = dxgiOutputInterface->lpVtbl->QueryInterface(dxgiOutputInterface, &IID_IDXGIOutput6, (void**) &dxgiOutput6Interface);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_ENUM_OUTPUT;
	}
	dxgiOutputInterface->lpVtbl->Release(dxgiOutputInterface);
	
	DXGI_OUTPUT_DESC1 outputDescription; //Monitor Info
	hrRes = dxgiOutput6Interface->lpVtbl->GetDesc1(dxgiOutput6Interface, &outputDescription);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_OUTPUT_DESC;
	}
	//uint64_t dpiWidth = outputDescription.DesktopCoordinates.right - outputDescription.DesktopCoordinates.left;
	//uint64_t dpiHeight = outputDescription.DesktopCoordinates.bottom - outputDescription.DesktopCoordinates.top;
	//consoleWriteLineWithNumberFast("DPI Width: ", 11, dpiWidth, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("DPI Height: ", 12, dpiHeight, NUM_FORMAT_UNSIGNED_INTEGER);
	
	//This function allocates an appropriate amount of memory...
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
	hrRes = dxgiOutput6Interface->lpVtbl->DuplicateOutput1(dxgiOutput6Interface, (IUnknown*) graphicsDesktopDuplicationDevice, 0, 7, outputFormats, &graphicsDesktopDuplicationPtr);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_CREATE_OUTPUT_DUPLICATION;
	}
	dxgiOutput6Interface->lpVtbl->Release(dxgiOutput6Interface);
	
	//consoleBufferFlush();
	//consoleWaitForEnter();
	
	DXGI_OUTDUPL_DESC outputDuplicationDescription;
	graphicsDesktopDuplicationPtr->lpVtbl->GetDesc(graphicsDesktopDuplicationPtr, &outputDuplicationDescription);
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
	graphicsDesktopDuplicationWidth = (uint32_t) modeDesc->Width;
	graphicsDesktopDuplicationHeight = (uint32_t) modeDesc->Height;
	consoleWriteLineWithNumberFast("Width: ", 7, graphicsDesktopDuplicationWidth, NUM_FORMAT_UNSIGNED_INTEGER);
	consoleWriteLineWithNumberFast("Height: ", 8, graphicsDesktopDuplicationHeight, NUM_FORMAT_UNSIGNED_INTEGER);
	
	
	//Get the first frame to confirm desktop dimensions, texture format, and sharable windows handle
	// and then lowers background desktop duplication resources by not releasing the frame
	IDXGIResource* resourceInterface = NULL;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;
	const uint64_t acquireFrameTimeoutMS = 100;
	uint64_t aquireFrameTries = 10;
	while (aquireFrameTries > 0) {
		hrRes = graphicsDesktopDuplicationPtr->lpVtbl->ReleaseFrame(graphicsDesktopDuplicationPtr); //Releases resourceInterface ...?
		if (hrRes != S_OK) {
			if (hrRes != DXGI_ERROR_INVALID_CALL) {
				graphicsError = hrRes;
				return ERROR_DESKDUPL_RELEASE_FAILED;
			}
			//Frame already released
		}
		
		hrRes = graphicsDesktopDuplicationPtr->lpVtbl->AcquireNextFrame(graphicsDesktopDuplicationPtr, acquireFrameTimeoutMS, &frameInfo, &resourceInterface);
		if (hrRes != S_OK) {
			if (hrRes != DXGI_ERROR_WAIT_TIMEOUT) {
				graphicsError = hrRes;
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
		graphicsError = hrRes;
		return ERROR_DESKDUPL_TEXTURE_QUERY;
	}
	
	D3D11_TEXTURE2D_DESC textureDescription;
	desktopDuplicationTexturePtr->lpVtbl->GetDesc(desktopDuplicationTexturePtr, &textureDescription);
	if (textureDescription.Width != graphicsDesktopDuplicationWidth) {
		return ERROR_DESKDUPL_TEXTURE_INVALID;
	}
	if (textureDescription.Height != graphicsDesktopDuplicationHeight) {
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
	
	//Get a windows texture handle to share with Vulkan
	IDXGIResource1* resource1Interface = NULL;
	hrRes = resourceInterface->lpVtbl->QueryInterface(resourceInterface, &IID_IDXGIResource1, (void**) &resource1Interface);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_RESOURCE_QUERY;
	}
	
	graphicsDesktopDuplicationTextureHandle = NULL;
	hrRes = resource1Interface->lpVtbl->CreateSharedHandle(resource1Interface, NULL, DXGI_SHARED_RESOURCE_READ, NULL, &graphicsDesktopDuplicationTextureHandle);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_CREATE_SHARED_HANDLE;
	}
	resource1Interface->lpVtbl->Release(resource1Interface);
	
	//Confirm Shared Handle
	if (graphicsDesktopDuplicationTextureHandle == NULL) {
		return ERROR_DESKDUPL_CREATE_SHARED_HANDLE;
	}
	//consoleWriteLineWithNumberFast("Pointer: ", 9, (uint64_t) graphicsDesktopDuplicationTextureHandle, NUM_FORMAT_FULL_HEXADECIMAL);
	
	hrRes = desktopDuplicationTexturePtr->lpVtbl->QueryInterface(desktopDuplicationTexturePtr, &IID_IDXGIKeyedMutex, (void**) &graphicsDesktopDuplicationKeyedMutex);
	if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_KEYEDMUTEX_QUERY;
	}
	desktopDuplicationTexturePtr->lpVtbl->Release(desktopDuplicationTexturePtr);
	
	*width = graphicsDesktopDuplicationWidth;
	*height = graphicsDesktopDuplicationHeight;
	
	//Check if Card is NVIDIA brand
	//if (graphicsVenderID != NVIDIA_PCI_VENDER_ID) {
	//	return ERROR_DESKDUPL_ADAPTER_NOT_VALID;
	//}
	*venderID = graphicsVenderID;
	
	return 0;
}

int graphicsDesktopDuplicationReleaseFrame() {
	//if (graphicsDesktopDuplicationPtr == NULL) {
	//	return ERROR_DESKDUPL_NOT_VALID;
	//}
	
	HRESULT hrRes = graphicsDesktopDuplicationPtr->lpVtbl->ReleaseFrame(graphicsDesktopDuplicationPtr);
	if (hrRes == DXGI_ERROR_INVALID_CALL) {
		return 0;
	}
	else if (hrRes != S_OK) {
		graphicsError = hrRes;
		return ERROR_DESKDUPL_RELEASE_FAILED;
	}
	/*
	if (hrRes != S_OK) {
		if (hrRes != DXGI_ERROR_INVALID_CALL) {
			graphicsError = hrRes;
			return ERROR_DESKDUPL_RELEASE_FAILED;
		}
		//Frame already released
	}
	//*/
	
	return 0;
}

int graphicsDesktopDuplicationAcquireNextFrame(uint64_t millisecondTimeout, uint64_t* presentationTime, uint64_t* accumulatedFrames) {
	///if (graphicsDesktopDuplicationPtr == NULL) {
	//	return ERROR_DESKDUPL_NOT_VALID;
	//}
	
	DXGI_OUTDUPL_FRAME_INFO frameInfo;
	IDXGIResource* desktopResourcePtr = NULL;
	HRESULT hrRes = graphicsDesktopDuplicationPtr->lpVtbl->AcquireNextFrame(graphicsDesktopDuplicationPtr, millisecondTimeout, &frameInfo, &desktopResourcePtr);
	if (hrRes == DXGI_ERROR_WAIT_TIMEOUT) { 
		return ERROR_DESKDUPL_ACQUIRE_TIMEOUT;
	}
	else if (hrRes != S_OK) { //Something happened
		graphicsError = hrRes;
		return ERROR_DESKDUPL_ACQUIRE_FAILED;
	}
	
	//Acquired Something But it might just be mouse stuff (indicated by a zero presentation time)
	*presentationTime = (uint64_t) frameInfo.LastPresentTime.QuadPart;
	*accumulatedFrames = (uint64_t) frameInfo.AccumulatedFrames;
	return 0;
}

void graphicsDesktopDuplicationCleanup() {
	graphicsDesktopDuplicationKeyedMutex->lpVtbl->Release(graphicsDesktopDuplicationKeyedMutex);
	graphicsDesktopDuplicationKeyedMutex = NULL;
	
	CloseHandle(graphicsDesktopDuplicationTextureHandle);
	graphicsDesktopDuplicationTextureHandle = NULL;
	
	graphicsDesktopDuplicationWidth = 0;
	graphicsDesktopDuplicationHeight = 0;
	
	//Release Big Ticket Items
	graphicsDesktopDuplicationPtr->lpVtbl->ReleaseFrame(graphicsDesktopDuplicationPtr);
	graphicsDesktopDuplicationPtr->lpVtbl->Release(graphicsDesktopDuplicationPtr);
	graphicsDesktopDuplicationPtr = NULL;
	graphicsDesktopDuplicationDevice->lpVtbl->Release(graphicsDesktopDuplicationDevice);
	graphicsDesktopDuplicationDevice = NULL;
	graphicsAdapter->lpVtbl->Release(graphicsAdapter);
	graphicsAdapter = NULL;
	
	//Need to do a better job releasing the memory
}


#include "include/vulkan/vulkan_win32.h" //Include Win32 Vulkan specifics

static VkResult vulkanExtraInfo = VK_SUCCESS;
void vulkanGetError(int* error) {
	*error = vulkanExtraInfo;
}

//*
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkanDebugMsgCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
		
		consoleWriteLineFast("Callback Called!", 16);
    consoleWriteLineSlow((char*) (pCallbackData->pMessage));
		
    return VK_FALSE;
}
//*/

static VkInstance vulkanInstance = VK_NULL_HANDLE;
static void* vulkanTempBuffer = NULL;
static uint64_t vulkanTempBufferByteSize = 0;
static VkDebugUtilsMessengerEXT vulkanDebugMsg = VK_NULL_HANDLE;
static int vulkanCreateInstance(uint64_t useValidationLayers) {
	if (vulkanInstance != VK_NULL_HANDLE) {
		return 0;//ERROR_VULKAN_TBD;
	}
	
	VkInstanceCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pNext = NULL;
	createInfo.flags = 0;
	
	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = "App Name";
	appInfo.applicationVersion = 1;
	appInfo.pEngineName = "Vulkan Win32 Engine";
	appInfo.engineVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_3;
	
	createInfo.pApplicationInfo = &appInfo;
	
	const char* const layerNames[] = {
		"VK_LAYER_KHRONOS_validation",
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME//,
		//"VK_LAYER_LUNARG_api_dump"
	};
	if (useValidationLayers > 0) {
		/*
		uint32_t layerCount = 4;
		VkLayerProperties* layerProperties = (VkLayerProperties*) vulkanTempBuffer;
		VkResult res = vkEnumerateInstanceLayerProperties(&layerCount, layerProperties);
		if (res != VK_SUCCESS) {
			vulkanExtraInfo = res;
			return ERROR_VULKAN_CREATE_INSTANCE_FAILED;
		}
		consoleWriteLineWithNumberFast("LC: ", 4, layerCount, NUM_FORMAT_UNSIGNED_INTEGER);
		return ERROR_VULKAN_CREATE_INSTANCE_FAILED;
		//*/
		
		createInfo.enabledLayerCount = 1;
		createInfo.ppEnabledLayerNames = layerNames;
	}
	else {
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = NULL;
	}
	
	const char* const extensionNames[] = {
		VK_KHR_SURFACE_EXTENSION_NAME, 
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME
	};
	
	createInfo.enabledExtensionCount = 2;
	createInfo.ppEnabledExtensionNames = extensionNames;
	
	VkResult result = vkCreateInstance(&createInfo, VULKAN_ALLOCATOR, &vulkanInstance);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_CREATE_INSTANCE_FAILED;
	}
	
	int error = memoryAllocateOnePage(&vulkanTempBuffer, &vulkanTempBufferByteSize);
	RETURN_ON_ERROR(error);
	
	if (useValidationLayers > 1) {
		consoleWriteLineSlow("Got Here!");
		PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT2 = (PFN_vkCreateDebugUtilsMessengerEXT) (vkGetInstanceProcAddr(vulkanInstance, "vkCreateDebugUtilsMessengerEXT"));
		
		VkDebugUtilsMessengerCreateInfoEXT debugMsgCreateInfo;
		debugMsgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugMsgCreateInfo.pNext = NULL;
		debugMsgCreateInfo.flags = 0;
		debugMsgCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		debugMsgCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
		debugMsgCreateInfo.pfnUserCallback = vulkanDebugMsgCallback;
		debugMsgCreateInfo.pUserData = NULL;
		
		result = vkCreateDebugUtilsMessengerEXT2(vulkanInstance, &debugMsgCreateInfo, VULKAN_ALLOCATOR, &vulkanDebugMsg);
		if (result != VK_SUCCESS) {
			vulkanExtraInfo = result;
			return ERROR_VULKAN_CREATE_INSTANCE_FAILED;
		}
	}
	
	
	return 0;
}

static VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
static uint32_t deviceLocalOnlyMemoryTypeIndex = 0;
static uint32_t basicCPUaccessMemoryTypeIndex = 0;
static int vulkanChoosePhysicalDevice(LUID* id) {
	if (vulkanPhysicalDevice != VK_NULL_HANDLE) {
		return 0;//ERROR_VULKAN_TBD;
	}
	
	VkPhysicalDevice* pDevices = (VkPhysicalDevice*) vulkanTempBuffer;
	uint32_t deviceCount = 32; //256 Bytes: 32 * 8 (sizeof(VkPhysicalDevice))
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
	
	vulkanPhysicalDevice = VK_NULL_HANDLE;
	for (uint32_t d = 0; d < deviceCount; d++) {
		vkGetPhysicalDeviceProperties2(pDevices[d], &deviceProperties2);
		
		if (devicePropertiesID.deviceLUIDValid == VK_TRUE) {
			uint32_t* deviceUUID = (uint32_t*) &(devicePropertiesID.deviceLUID);
			if (deviceUUID[0] == id->LowPart) {
				if (deviceUUID[1] == id->HighPart) {
					vulkanPhysicalDevice = pDevices[d];
					d = deviceCount; //break;
				}
			}
		}		
	}
	if (vulkanPhysicalDevice == VK_NULL_HANDLE) {
		return ERROR_VULKAN_CANNOT_FIND_GPU;
	}
	
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(vulkanPhysicalDevice, VK_FORMAT_B8G8R8A8_UNORM, &formatProperties);
	uint32_t formatFeatures = (uint32_t) formatProperties.optimalTilingFeatures;
	if (!((formatFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && (formatFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) && (formatFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))) {
		return ERROR_VULKAN_BAD_OPTIMAL_FEATURES;
	}
	
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(vulkanPhysicalDevice, &memProperties);
	uint32_t memTypeCnt = memProperties.memoryTypeCount;
	deviceLocalOnlyMemoryTypeIndex = memTypeCnt;
	basicCPUaccessMemoryTypeIndex = memTypeCnt;
	for (uint32_t i = 0; i < memTypeCnt; i++) {
		VkMemoryPropertyFlags memTypeProp = memProperties.memoryTypes[i].propertyFlags;
		if (deviceLocalOnlyMemoryTypeIndex == memTypeCnt) {
			if (memTypeProp == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
				deviceLocalOnlyMemoryTypeIndex = i;
			}
		}
		if (basicCPUaccessMemoryTypeIndex == memTypeCnt) {
			if (memTypeProp == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
				basicCPUaccessMemoryTypeIndex = i;
			}
		}
		//consoleWriteLineWithNumberFast("Prop: ", 6, memTypeProp, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("Ind: ", 5, memProperties.memoryTypes[i].heapIndex, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	}
	
	if (deviceLocalOnlyMemoryTypeIndex == memTypeCnt) {
		return ERROR_VULKAN_TBD;
	}
	if (basicCPUaccessMemoryTypeIndex == memTypeCnt) {
		return ERROR_VULKAN_TBD;
	}
	
	
	return 0;
}

void vulkanCleanup() {
	deviceLocalOnlyMemoryTypeIndex = 0;
	basicCPUaccessMemoryTypeIndex = 0;
	vulkanPhysicalDevice = VK_NULL_HANDLE;
	
	//Destroy vulkanDebugMsg here in future
	
	if (vulkanTempBuffer != NULL) {
		memoryDeallocate(&vulkanTempBuffer);
		vulkanTempBufferByteSize = 0;
	}
	
	vkDestroyInstance(vulkanInstance, VULKAN_ALLOCATOR);
	vulkanInstance = VK_NULL_HANDLE;
}

int vulkanGetMemoryTypeIndex(VkDevice device, uint32_t* deviceLocalMemIndex, uint32_t* cpuAccessMemIndex) {
	if (device == VK_NULL_HANDLE) {
		return ERROR_ARGUMENT_DNE;
	}
	
	*deviceLocalMemIndex = deviceLocalOnlyMemoryTypeIndex;
	*cpuAccessMemIndex = basicCPUaccessMemoryTypeIndex;
	
	return 0;
}

const char* const deviceExtensions[] = {
	VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
	VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
	VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME,
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
	VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME,
	VK_KHR_VIDEO_DECODE_H265_EXTENSION_NAME
};

//static VkImage vulkanDesktopDuplicationImage = VK_NULL_HANDLE;
//static VkDeviceMemory vulkanDesktopDuplicationImportMem = VK_NULL_HANDLE;
//Import Mutex in future...
int vulkanImportDesktopDuplicationImage(VkDevice device, VkImage* ddImage, VkDeviceMemory* ddImportMem) {
	if (device == VK_NULL_HANDLE) {
		return ERROR_ARGUMENT_DNE;
	}
	if (ddImage == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (ddImportMem == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (graphicsDesktopDuplicationTextureHandle == NULL) {
		return ERROR_DESKDUPL_NOT_VALID;
	}
	
	//Create Image that Points to DX11 Desktop Duplication Texture
	VkImageCreateInfo imageInfo;
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	
	VkExternalMemoryImageCreateInfo imageInfoExternal;
	imageInfoExternal.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	imageInfoExternal.pNext = NULL;
	imageInfoExternal.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
	
	imageInfo.pNext = &imageInfoExternal;
	imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_B8G8R8A8_UNORM; //Verified Earlier
	imageInfo.extent.width = graphicsDesktopDuplicationWidth;
	imageInfo.extent.height = graphicsDesktopDuplicationHeight;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1; //desktopDuplicationMipLevels;
	imageInfo.arrayLayers = 1; //desktopDuplicationArrayLayers;
	imageInfo.samples = 1; //desktopDuplicationSamples;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL; //Optimal Texture
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.queueFamilyIndexCount = 0;
	imageInfo.pQueueFamilyIndices = NULL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	
	VkResult result = vkCreateImage(device, &imageInfo, VULKAN_ALLOCATOR, ddImage);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_IMAGE_CREATION_FAILED;
	}
	
	//"Allocate" Memory for Importing the DX11 Image Texture
	VkMemoryRequirements imageMemReqs;
	vkGetImageMemoryRequirements(device, *ddImage, &imageMemReqs);
	//consoleWriteLineWithNumberFast("Number: ", 8, imageMemReqs.memoryTypeBits, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	VkMemoryAllocateInfo memAllocInfo;
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	
	VkImportMemoryWin32HandleInfoKHR win32HandleImportInfo;
	win32HandleImportInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	win32HandleImportInfo.pNext = NULL;
	
	VkExternalMemoryHandleTypeFlags handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
	win32HandleImportInfo.handleType = handleType;
	
	win32HandleImportInfo.handle = graphicsDesktopDuplicationTextureHandle;
	win32HandleImportInfo.name = NULL; //Null when handle is not null
	
	
	memAllocInfo.pNext = &win32HandleImportInfo;
	memAllocInfo.allocationSize = imageMemReqs.size; //Ignored for Imported Memory
	
	PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR2 = (PFN_vkGetMemoryWin32HandlePropertiesKHR) (vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandlePropertiesKHR"));
	
	VkMemoryWin32HandlePropertiesKHR win32HandleProp;
	win32HandleProp.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;
	win32HandleProp.pNext = NULL;
	result = vkGetMemoryWin32HandlePropertiesKHR2(device, handleType, graphicsDesktopDuplicationTextureHandle, &win32HandleProp);
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
	
	result = vkAllocateMemory(device, &memAllocInfo, VULKAN_ALLOCATOR, ddImportMem);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	
	VkBindImageMemoryInfo bindInfo;
	bindInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
	bindInfo.pNext = NULL;
	bindInfo.image = *ddImage;
	bindInfo.memory = *ddImportMem;
	bindInfo.memoryOffset = 0;
	
	result = vkBindImageMemory2(device, 1, &bindInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	return 0;
}

int vulkanCreateExportImageMemory(VkDevice device, VkImageCreateInfo* imgCreateInfo, char* nameUTF8, VkImage* image, VkDeviceMemory* exportMem) {
	if (device == VK_NULL_HANDLE) {
		return ERROR_ARGUMENT_DNE;
	}
	if (imgCreateInfo->pNext != NULL) {
		return ERROR_ARGUMENT_DNE; //better error
	}
	if (nameUTF8 == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (image == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (exportMem == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	
	VkExternalMemoryImageCreateInfo imageInfoExternal;
	imageInfoExternal.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	imageInfoExternal.pNext = NULL;
	imageInfoExternal.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	
	imgCreateInfo->pNext = &imageInfoExternal;
	
	VkResult result = vkCreateImage(device, imgCreateInfo, VULKAN_ALLOCATOR, image);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_IMAGE_CREATION_FAILED;
	}
	
	LPWSTR nameUTF16 = (LPWSTR) vulkanTempBuffer;
	int characters = MultiByteToWideChar(CP_UTF8, 0, nameUTF8, -1, nameUTF16, vulkanTempBufferByteSize);
	if (characters == 0) {
		return ERROR_IO_UNICODE_TRANSLATE;
	}
	nameUTF16[characters] = 0; //Needed?
	
	
	VkImageMemoryRequirementsInfo2 imageMemReqsInfo;
	imageMemReqsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
	imageMemReqsInfo.pNext = NULL;
	imageMemReqsInfo.image = *image;
	
	VkMemoryRequirements2 memReqs2;
	memReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	
	VkMemoryDedicatedRequirementsKHR dedicatedReqs;
	dedicatedReqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;
	dedicatedReqs.pNext = NULL;
	dedicatedReqs.requiresDedicatedAllocation = 0;
	
	memReqs2.pNext = &dedicatedReqs;
	
	vkGetImageMemoryRequirements2(device, &imageMemReqsInfo, &memReqs2);
	//consoleWriteLineWithNumberFast("Size: ", 6, memReqs2.memoryRequirements.size, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Alignment: ", 11, memReqs2.memoryRequirements.alignment, NUM_FORMAT_PARTIAL_HEXADECIMAL);	
	//consoleWriteLineWithNumberFast("Dedication: ", 12, dedicatedReqs.requiresDedicatedAllocation, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	VkMemoryAllocateInfo memAllocInfo;
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	
	VkMemoryDedicatedAllocateInfoKHR memDedicatedAllocInfo;
	memDedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	
	VkExportMemoryAllocateInfo exportMemoryInfo;
	exportMemoryInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
	
	VkExportMemoryWin32HandleInfoKHR win32HandleExportInfo;
	win32HandleExportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	win32HandleExportInfo.pNext = NULL;
	
	SECURITY_ATTRIBUTES securityAttributes = {0};
	securityAttributes.nLength = sizeof(securityAttributes);
	securityAttributes.lpSecurityDescriptor = NULL;
	securityAttributes.bInheritHandle = FALSE;
	
	win32HandleExportInfo.pAttributes = &securityAttributes; //Does not matter if pAttributes is null...
	win32HandleExportInfo.dwAccess = DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE; // | GENERIC_ALL;
	win32HandleExportInfo.name = nameUTF16;
	
	exportMemoryInfo.pNext = &win32HandleExportInfo;
	exportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	
	memDedicatedAllocInfo.pNext = &exportMemoryInfo;
	memDedicatedAllocInfo.image = *image;
	memDedicatedAllocInfo.buffer = VK_NULL_HANDLE;
	
	memAllocInfo.pNext = &memDedicatedAllocInfo;
	memAllocInfo.allocationSize = memReqs2.memoryRequirements.size;
	memAllocInfo.memoryTypeIndex = deviceLocalOnlyMemoryTypeIndex;
	
	result = vkAllocateMemory(device, &memAllocInfo, VULKAN_ALLOCATOR, exportMem);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_ALLOC_FAILED;
	}
	
	VkBindImageMemoryInfo bindInfo;
	bindInfo.sType = VK_STRUCTURE_TYPE_BIND_IMAGE_MEMORY_INFO;
	bindInfo.pNext = NULL;
	bindInfo.image = *image;
	bindInfo.memory = *exportMem;
	bindInfo.memoryOffset = 0;
	
	result = vkBindImageMemory2(device, 1, &bindInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_MEM_BIND_FAILED;
	}
	
	return 0;
}

int vulkanComputeSetup(VkDevice* device, uint32_t* computeQFI, uint32_t* transferQFI) {
	if (device == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (computeQFI == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (transferQFI == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	
	//Create Vulkan Instance and Choose Physical Device if not already done:
	int error = vulkanCreateInstance(0);
	RETURN_ON_ERROR(error);
	
	// Get graphics adapter for primary monitor
	error = graphicsSetupAdapter(1);
	RETURN_ON_ERROR(error);
	
	error = vulkanChoosePhysicalDevice(&graphicsAdapterID);
	RETURN_ON_ERROR(error);
	
	//Get graphics queue index
	uint8_t* tempBuffer = (uint8_t*) vulkanTempBuffer;
	uint32_t queueFamilyCount = 32; //??? = 32 * sizeof(VkQueueFamilyProperties2) + 32 * sizeof(VkQueueFamilyVideoPropertiesKHR)
	VkQueueFamilyProperties2* qFProperties = (VkQueueFamilyProperties2*) tempBuffer;
	for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
		qFProperties[qf].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		qFProperties[qf].pNext = NULL;
	}
	vkGetPhysicalDeviceQueueFamilyProperties2(vulkanPhysicalDevice, &queueFamilyCount, qFProperties);
	
	
	VkDeviceQueueCreateInfo queueCreateInfos[2];
	float queuePriorities[2];
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].pNext = NULL;
	queueCreateInfos[0].flags = 0;
	
	uint64_t seperateTransferQueue = 1;
	
	*computeQFI = 256;
	for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
		VkQueueFlags qFlags = qFProperties[qf].queueFamilyProperties.queueFlags;
		if ((qFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
			*computeQFI = qf;
			if ((qFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
				if ((qFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
					seperateTransferQueue = 0;
				}
				break;
			}
		}
	}
	if (*computeQFI == 256) {
		return ERROR_VULKAN_TBD;
	}
	
	queueCreateInfos[0].queueFamilyIndex = *computeQFI;
	queueCreateInfos[0].queueCount = 1;
	queuePriorities[0] = 1.0;
	queueCreateInfos[0].pQueuePriorities = &(queuePriorities[0]);
	
	uint32_t queueNum = 1;
	
	if (seperateTransferQueue == 1) {
		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].pNext = NULL;
		queueCreateInfos[1].flags = 0;
		
		*transferQFI = 256;
		for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
			VkQueueFlags qFlags = qFProperties[qf].queueFamilyProperties.queueFlags;
			if ((qFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
				*transferQFI = qf;
				if ((qFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
					break;
				}
			}
		}
		if (*transferQFI == 256) {
			return ERROR_VULKAN_TBD;
		}
		
		//Should never happen...
		if (*transferQFI == *computeQFI) {
			return ERROR_VULKAN_TBD;
		}
		
		queueCreateInfos[1].queueFamilyIndex = *transferQFI;
		queueCreateInfos[1].queueCount = 1;
		queuePriorities[1] = 1.0;
		queueCreateInfos[1].pQueuePriorities = &(queuePriorities[1]);
		
		queueNum++;
	}
	
	
	VkPhysicalDeviceSynchronization2Features sync2Features;
	sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	sync2Features.pNext = NULL;
	//sync2Features.synchronization2 = VK_TRUE;
	
	VkPhysicalDeviceFeatures2 deviceFeatures2;
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &sync2Features;
	vkGetPhysicalDeviceFeatures2(vulkanPhysicalDevice, &deviceFeatures2);
	if (sync2Features.synchronization2 != VK_TRUE) {
		return ERROR_VULKAN_TBD;
	}
	
	VkDeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &sync2Features;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = queueNum;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
	deviceCreateInfo.enabledLayerCount = 0; //deprecated and ignored
	deviceCreateInfo.ppEnabledLayerNames = NULL; //deprecated and ignored
	
	deviceCreateInfo.enabledExtensionCount = 3;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(vulkanPhysicalDevice, &deviceFeatures);
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	
	VkResult result = vkCreateDevice(vulkanPhysicalDevice, &deviceCreateInfo, VULKAN_ALLOCATOR, device);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_DEVICE_CREATION_FAILED;
	}
	
	
	return 0;
}

int vulkanVideoSetup(VkDevice* device, uint32_t* graphicsComputeTransferQFI, uint32_t* videoQFI, VkVideoProfileInfoKHR* videoProfileInfo, VkVideoCapabilitiesKHR* videoCapabilities, uint32_t* fmtCount, VkVideoFormatPropertiesKHR* videoFormatProps) {
	if (device == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (graphicsComputeTransferQFI == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (videoQFI == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	
	//Create Vulkan Instance and Choose Physical Device if not already done:
	int error = vulkanCreateInstance(1);
	RETURN_ON_ERROR(error);
	
	// Get graphics adapter for primary monitor
	error = graphicsSetupAdapter(1);
	RETURN_ON_ERROR(error);
	
	error = vulkanChoosePhysicalDevice(&graphicsAdapterID);
	RETURN_ON_ERROR(error);
	
	uint8_t* tempBuffer = (uint8_t*) vulkanTempBuffer;
	uint32_t queueFamilyCount = 32; //??? = 32 * sizeof(VkQueueFamilyProperties2) + 32 * sizeof(VkQueueFamilyVideoPropertiesKHR)
	VkQueueFamilyProperties2* qFProperties = (VkQueueFamilyProperties2*) tempBuffer;
	VkQueueFamilyVideoPropertiesKHR* qFVideoProperties = (VkQueueFamilyVideoPropertiesKHR*) (&tempBuffer[vulkanTempBufferByteSize>>1]);
	for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
		qFProperties[qf].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		qFVideoProperties[qf].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
		qFVideoProperties[qf].pNext = NULL;
		qFProperties[qf].pNext = &qFVideoProperties[qf];
	}
	vkGetPhysicalDeviceQueueFamilyProperties2(vulkanPhysicalDevice, &queueFamilyCount, qFProperties);
		
	VkDeviceQueueCreateInfo queueCreateInfos[2];
	float queuePriorities[2];
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].pNext = NULL;
	queueCreateInfos[0].flags = 0;
	
	*graphicsComputeTransferQFI = 256;
	for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
		VkQueueFlags qFlags = qFProperties[qf].queueFamilyProperties.queueFlags;
		if ((qFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
			if ((qFlags & VK_QUEUE_COMPUTE_BIT) != 0) {
				if ((qFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
					*graphicsComputeTransferQFI = qf;
				}
				break;
			}
		}
	}
	if (*graphicsComputeTransferQFI == 256) {
		return ERROR_VULKAN_TBD;
	}
	
	queueCreateInfos[0].queueFamilyIndex = *graphicsComputeTransferQFI;
	queueCreateInfos[0].queueCount = 1;
	queuePriorities[0] = 1.0;
	queueCreateInfos[0].pQueuePriorities = &(queuePriorities[0]);
	
	queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[1].pNext = NULL;
	queueCreateInfos[1].flags = 0;
	
	*videoQFI = 256;
	for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
		VkQueueFlags qFlags = qFProperties[qf].queueFamilyProperties.queueFlags;
		if ((qFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0) {
			if ((qFVideoProperties[qf].videoCodecOperations & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) != 0) {
				*videoQFI = qf;
				//if ((qFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
					//consoleWriteLineFast("Video Queue also supports transfers!", 36);
				//}
				break;
			}
		}
	}
	if (*videoQFI == 256) {
		return ERROR_VULKAN_TBD;
	}
	
	//Should never happen...
	if (*videoQFI == *graphicsComputeTransferQFI) {
		return ERROR_VULKAN_TBD;
	}
	
	queueCreateInfos[1].queueFamilyIndex = *videoQFI;
	queueCreateInfos[1].queueCount = 1;
	queuePriorities[1] = 1.0;
	queueCreateInfos[1].pQueuePriorities = &(queuePriorities[1]);	
	
	VkPhysicalDeviceSynchronization2Features sync2Features;
	sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	sync2Features.pNext = NULL;
	//sync2Features.synchronization2 = VK_TRUE;
	
	VkPhysicalDeviceFeatures2 deviceFeatures2;
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &sync2Features;
	vkGetPhysicalDeviceFeatures2(vulkanPhysicalDevice, &deviceFeatures2);
	if (sync2Features.synchronization2 != VK_TRUE) {
		return ERROR_VULKAN_TBD;
	}
	
	VkDeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &sync2Features;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = 2;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
	deviceCreateInfo.enabledLayerCount = 0; //deprecated and ignored
	deviceCreateInfo.ppEnabledLayerNames = NULL; //deprecated and ignored
	
	deviceCreateInfo.enabledExtensionCount = 7;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceFeatures(vulkanPhysicalDevice, &deviceFeatures);
	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	
	VkResult result = vkCreateDevice(vulkanPhysicalDevice, &deviceCreateInfo, VULKAN_ALLOCATOR, device);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_DEVICE_CREATION_FAILED;
	}
	
	//Vulkan Video Check
	PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR2 = (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR) (vkGetInstanceProcAddr(vulkanInstance, "vkGetPhysicalDeviceVideoCapabilitiesKHR"));
	PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR2 = (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR) (vkGetInstanceProcAddr(vulkanInstance, "vkGetPhysicalDeviceVideoFormatPropertiesKHR"));
	
	//Maybe use VkVideoDecodeUsageInfoKHR in future
	result = vkGetPhysicalDeviceVideoCapabilitiesKHR2(vulkanPhysicalDevice, videoProfileInfo, videoCapabilities);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	//consoleWriteLineWithNumberFast("Capability: ", 12, videoCapabilities->maxDpbSlots, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("Capability: ", 12, videoCapabilities->flags, NUM_FORMAT_UNSIGNED_INTEGER);
		
	VkPhysicalDeviceVideoFormatInfoKHR videoFormatInfo;
	videoFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR;
	
	VkVideoProfileListInfoKHR videoProfileInfoList;
	videoProfileInfoList.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
	videoProfileInfoList.pNext = NULL;
	videoProfileInfoList.profileCount = 1;
	videoProfileInfoList.pProfiles = videoProfileInfo;
	
	videoFormatInfo.pNext = &videoProfileInfoList;
	videoFormatInfo.imageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
	videoFormatInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; //VK_IMAGE_USAGE_STORAGE_BIT not allowed
	
	result = vkGetPhysicalDeviceVideoFormatPropertiesKHR2(vulkanPhysicalDevice, &videoFormatInfo, fmtCount, videoFormatProps);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	
	
	
	return 0;
}



#define VULKAN_TARGET_PRESENTATION_MODE VK_PRESENT_MODE_IMMEDIATE_KHR
static int vulkanCreateSurface(HWND windowHandle, HINSTANCE appInstance, VkSurfaceKHR* surface) {
	if (surface == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	
	 VkSurfaceKHR vulkanSurface = VK_NULL_HANDLE;
	
	// Windows Surface Choose
	VkWin32SurfaceCreateInfoKHR surfaceInfo;
	surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.pNext = NULL;
	surfaceInfo.flags = 0;
	surfaceInfo.hinstance = appInstance;
	surfaceInfo.hwnd = windowHandle;
	
	VkResult result = vkCreateWin32SurfaceKHR(vulkanInstance, &surfaceInfo, VULKAN_ALLOCATOR, &vulkanSurface);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	//Make sure typical 8-bit Windows sRGB format is available
	VkSurfaceFormatKHR* surfaceFormats = (VkSurfaceFormatKHR*) vulkanTempBuffer;
	uint32_t surfaceFormatCount = 64;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanSurface, &surfaceFormatCount, surfaceFormats);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	if (surfaceFormatCount == 0) {
		return ERROR_VULKAN_TBD;
	}
	
	VkFormat pickedFormat = VK_FORMAT_UNDEFINED;
	for (uint32_t sF = 0; sF < surfaceFormatCount; sF++) {
		//consoleWriteLineWithNumberFast("ColorSpace: ", 12, surfaceFormats[sF].colorSpace, NUM_FORMAT_PARTIAL_HEXADECIMAL);
		if (surfaceFormats[sF].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			//consoleWriteLineWithNumberFast("Format: ", 8, surfaceFormats[sF].format, NUM_FORMAT_PARTIAL_HEXADECIMAL);
			if (surfaceFormats[sF].format == VK_FORMAT_B8G8R8A8_UNORM) {  //sRGB format for linear filtering?
				pickedFormat = VK_FORMAT_B8G8R8A8_UNORM;
			}
		}
	}
	if (pickedFormat == VK_FORMAT_UNDEFINED) {
		return ERROR_VULKAN_TBD;
	}
	
	//Make sure that both VK_PRESENT_MODE_IMMEDIATE_KHR and VK_PRESENT_MODE_MAILBOX_KHR are available
	//VK_PRESENT_MODE_FIFO_KHR is guaranteed
	VkPresentModeKHR* presentationModes = (VkPresentModeKHR*) vulkanTempBuffer;
	uint32_t presentationModeCount = 32;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanSurface, &presentationModeCount, presentationModes);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	if (presentationModeCount == 0) {
		return ERROR_VULKAN_TBD;
	}
	
	uint64_t presentationModeExists = 0;
	for (uint32_t pM = 0; pM < presentationModeCount; pM++) {
		if (presentationModes[pM] == VULKAN_TARGET_PRESENTATION_MODE) {
			presentationModeExists = 1;
			break;
		}
	}
	if (presentationModeExists == 0) {
		return ERROR_VULKAN_TBD;
	}
	
	/*
	presentationModeExists = 0;
	for (uint32_t pM = 0; pM < presentationModeCount; pM++) {
		if (presentationModes[pM] == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentationModeExists = 1;
			break;
		}
	}
	if (presentationModeExists == 0) {
		return ERROR_VULKAN_TBD;
	}
	//*/
	
	*surface = vulkanSurface;
	return 0;
}


static uint64_t vulkanWindowResizeDisabled = 0; //merge into vulkanWindowState in future
static LRESULT CALLBACK vulkanWindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	//consoleWriteLineWithNumberFast("Msg: ", 5, uMsg, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	switch (uMsg)	{ //look at winuser.h for possibilities
	case WM_KEYUP:
		if (wParam == VK_ESCAPE) { //Quit with Escape Key
			vulkanWindowCleanup();
		}
		if (wParam == VK_F11) { //Toggle Fullscreen with F11 Key
			vulkanWindowFullscreenToggle();
		}
		return 0;
	case WM_WINDOWPOSCHANGED: //Also prevents WM_SIZE and WM_MOVE from being default triggered
		if (vulkanWindowResizeDisabled == 0) {
			vulkanWindowResize();
		}
		return 0;
	case WM_ENTERSIZEMOVE: //Resize / Move Starting
		vulkanWindowResizeDisabled = 1;
		return 0;
	case WM_EXITSIZEMOVE:
		vulkanWindowResizeDisabled = 0;
		vulkanWindowResize();
		return 0;
	case WM_CLOSE: //Red X clicked, Alt-F4, etc.
		vulkanWindowCleanup();
		return 0;
	case WM_DESTROY: //Start Close
		PostQuitMessage(0);
		//return 0;
	//case WM_NCDESTROY:
		//return 0;	
	}
	
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

//Vulkan Window State Codes:
#define VULKAN_WINDOW_STATE_UNDEFINED 0
#define VULKAN_WINDOW_STATE_CREATED 1
#define VULKAN_WINDOW_STATE_RUNNING 2
#define VULKAN_WINDOW_STATE_PAUSED 3
static uint64_t vulkanWindowState = VULKAN_WINDOW_STATE_UNDEFINED;

static HWND vulkanWindowHandle = NULL;
static WINDOWPLACEMENT vulkanWindowPlacement;

#define VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT 3

static VkDevice vulkanWindowDevice = VK_NULL_HANDLE;
static VkQueue vulkanWindowGraphicsPresentationQueue = VK_NULL_HANDLE;
static VkSwapchainCreateInfoKHR vulkanWindowSwapchainInfo; //Contains the window surface
static VkSwapchainKHR vulkanWindowSwapchain = VK_NULL_HANDLE;
static VkImage vulkanWindowSwapchainImages[VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT];
static VkImageMemoryBarrier2 vulkanWindowImgMemBar;
static VkCommandPool vulkanWindowCommandPool = VK_NULL_HANDLE;
//static VkCommandBuffer vulkanWindowCommandBuffers[VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT];
static VkCommandBufferSubmitInfo vulkanWindowCommandBufferSubmitInfos[VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT];

int vulkanWindowSetup(uint64_t windowWidth, uint64_t windowHeight, VkDevice* device, uint32_t* graphicsTransferPresentationQFI, VkQueue* graphicsTransferQueue, uint32_t* videoQFI) {
	if (device == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (graphicsTransferPresentationQFI == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (graphicsTransferQueue == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	
	//Create Vulkan Instance and Choose Physical Device if not already done:
	int error = vulkanCreateInstance(0);
	RETURN_ON_ERROR(error);
	
	// Get graphics adapter for primary monitor
	error = graphicsSetupAdapter(1);
	RETURN_ON_ERROR(error);
	
	error = vulkanChoosePhysicalDevice(&graphicsAdapterID);
	RETURN_ON_ERROR(error);
	
	
	//Create Windows Window:
	HMODULE hInstance = GetModuleHandle(NULL);
	const WCHAR vulkanWindowClassName[] = L"Vulkan Window Class";
	WNDCLASSEX wc;
	wc.cbSize				 = sizeof(WNDCLASSEX);
	wc.style				 = 0;
	wc.lpfnWndProc   = vulkanWindowProcedure;
	wc.cbClsExtra		 = 0;
	wc.cbWndExtra		 = 0;
	wc.hInstance     = hInstance;
	wc.hIcon				 = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor			 = LoadCursor(NULL, IDC_CROSS); //Load System Cursor
	wc.hbrBackground = (HBRUSH) (COLOR_BACKGROUND+1); //COLOR_WINDOW+1);
	wc.lpszMenuName	 = NULL;
	wc.lpszClassName = vulkanWindowClassName;
	wc.hIconSm			 = LoadIcon(NULL, IDI_QUESTION);
	RegisterClassEx(&wc);// Register the window class... How to clean?
	
	DWORD style = WS_OVERLAPPEDWINDOW;
	
	//Corrected Window Dimensions:
	RECT r = {};
	r.left = 0;
	r.top = 0;
	r.right = (LONG) windowWidth;
	r.bottom = (LONG) windowHeight;
	AdjustWindowRectEx(&r, style, FALSE, 0);
	
	int windowWidthC = (int) (r.right - r.left);
	int windowHeightC = (int) (r.bottom - r.top);
	
	const WCHAR vulkanWindowTitle[] = L"Vulkan Window";
	vulkanWindowHandle = CreateWindowEx( 	// Create the window.
		0,                            // Optional window styles.
		vulkanWindowClassName,        // Window class
		vulkanWindowTitle,						// Window text
		style,          							// Window style
		CW_USEDEFAULT, CW_USEDEFAULT,	// Window position
		windowWidthC, windowHeightC,	// Window size
		NULL,       									// Parent window    
		NULL,       									// Menu
		hInstance,  									// Instance handle
		NULL        									// Additional application data
	);
	
	ShowWindow(vulkanWindowHandle, SW_SHOWNORMAL);
	
	vulkanWindowResizeDisabled = 0;
	vulkanWindowPlacement.length = sizeof(WINDOWPLACEMENT);
	
	//Vulkan Create Surface
	error = vulkanCreateSurface(vulkanWindowHandle, hInstance, &(vulkanWindowSwapchainInfo.surface));
	RETURN_ON_ERROR(error);
	
	VkResult result = VK_SUCCESS;
	
	//Get graphics queue index
	uint8_t* tempBuffer = (uint8_t*) vulkanTempBuffer;
	uint32_t queueFamilyCount = 32; //??? = 32 * sizeof(VkQueueFamilyProperties2) + 32 * sizeof(VkQueueFamilyVideoPropertiesKHR)
	VkQueueFamilyProperties2* qFProperties = (VkQueueFamilyProperties2*) tempBuffer;
	VkQueueFamilyVideoPropertiesKHR* qFVideoProperties = (VkQueueFamilyVideoPropertiesKHR*) (&tempBuffer[vulkanTempBufferByteSize>>1]);
	for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
		qFProperties[qf].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
		qFVideoProperties[qf].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
		qFVideoProperties[qf].pNext = NULL;
		qFProperties[qf].pNext = &qFVideoProperties[qf];
	}
	vkGetPhysicalDeviceQueueFamilyProperties2(vulkanPhysicalDevice, &queueFamilyCount, qFProperties);
	
	
	VkDeviceQueueCreateInfo queueCreateInfos[2];
	float queuePriorities[2];
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].pNext = NULL;
	queueCreateInfos[0].flags = 0;
	
	*graphicsTransferPresentationQFI = 256;
	for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
		VkQueueFlags qFlags = qFProperties[qf].queueFamilyProperties.queueFlags;
		VkBool32 presentationSupport = VK_FALSE;
		result = vkGetPhysicalDeviceSurfaceSupportKHR(vulkanPhysicalDevice, qf, vulkanWindowSwapchainInfo.surface, &presentationSupport);
		if (result != VK_SUCCESS) {
			vulkanExtraInfo = result;
			return ERROR_VULKAN_TBD;
		}
		if ((qFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
			if (presentationSupport == VK_TRUE) {
				if ((qFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
					*graphicsTransferPresentationQFI = qf;
					break;
				}
			}
		}
	}
	if (*graphicsTransferPresentationQFI == 256) {
		//Could potentially split up graphics and presentation queues later for more support
		return ERROR_VULKAN_TBD;
	}
	
	queueCreateInfos[0].queueFamilyIndex = *graphicsTransferPresentationQFI;
	queueCreateInfos[0].queueCount = 1;
	queuePriorities[0] = 1.0;
	queueCreateInfos[0].pQueuePriorities = &(queuePriorities[0]);
	
	uint32_t queueNum = 1;
	
	if (videoQFI != NULL) {
		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].pNext = NULL;
		queueCreateInfos[1].flags = 0;
		
		*videoQFI = 256;
		for (uint32_t qf = 0; qf < queueFamilyCount; qf++) {
			VkQueueFlags qFlags = qFProperties[qf].queueFamilyProperties.queueFlags;
			if ((qFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) != 0) {
				if ((qFVideoProperties[qf].videoCodecOperations & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) != 0) {
					*videoQFI = qf;
					if ((qFlags & VK_QUEUE_TRANSFER_BIT) != 0) {
						consoleWriteLineFast("Video Queue also supports transfers!", 36);
					}
					break;
				}
			}
		}
		if (*videoQFI == 256) {
			return ERROR_VULKAN_TBD;
		}
		
		//Same queue family index...? what devices to handle better in future
		if (*videoQFI == *graphicsTransferPresentationQFI) {
			return ERROR_VULKAN_TBD;
		}
		
		queueCreateInfos[1].queueFamilyIndex = *videoQFI;
		queueCreateInfos[1].queueCount = 1;
		queuePriorities[1] = 1.0;
		queueCreateInfos[1].pQueuePriorities = &(queuePriorities[1]);
		
		queueNum++;
	}
	
	
	//Vulkan Create Device
	VkPhysicalDeviceSynchronization2Features sync2Features;
	sync2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	sync2Features.pNext = NULL;
	//sync2Features.synchronization2 = VK_TRUE;
	
	VkPhysicalDeviceFeatures2 deviceFeatures2;
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &sync2Features;
	vkGetPhysicalDeviceFeatures2(vulkanPhysicalDevice, &deviceFeatures2);
	if (sync2Features.synchronization2 != VK_TRUE) {
		return ERROR_VULKAN_TBD;
	}
	
	VkDeviceCreateInfo deviceCreateInfo;
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &sync2Features;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = queueNum;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
	deviceCreateInfo.enabledLayerCount = 0; //deprecated and ignored
	deviceCreateInfo.ppEnabledLayerNames = NULL; //deprecated and ignored
	
	deviceCreateInfo.enabledExtensionCount = 4;
	if (videoQFI != NULL) {
		deviceCreateInfo.enabledExtensionCount = 7;
	}
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	
	deviceCreateInfo.pEnabledFeatures = &(deviceFeatures2.features);
	
	result = vkCreateDevice(vulkanPhysicalDevice, &deviceCreateInfo, VULKAN_ALLOCATOR, &vulkanWindowDevice);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_DEVICE_CREATION_FAILED;
	}
	*device = vulkanWindowDevice;
	
	//Vulkan Get Queue(s)
	vkGetDeviceQueue(vulkanWindowDevice, *graphicsTransferPresentationQFI, 0, &vulkanWindowGraphicsPresentationQueue);
	*graphicsTransferQueue = vulkanWindowGraphicsPresentationQueue;
	//if (videoQueue != NULL) {
		//vkGetDeviceQueue(device, videoTransferQueueFamilyIndex, 0, videoQueue);
	//}
	//Should not be using video queue after this point
	
	
	//Create Swapchain and get Swapchain Images:
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanPhysicalDevice, vulkanWindowSwapchainInfo.surface, &surfaceCapabilities);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	if (surfaceCapabilities.maxImageCount != 0) {
		if (surfaceCapabilities.maxImageCount < VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT) {
			return ERROR_VULKAN_TBD;
		}
	}
	if ((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0) {
		return ERROR_VULKAN_TBD;
	}
	
	//VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT is guaranteed
	VkImageUsageFlags surfaceUsageFlags = surfaceCapabilities.supportedUsageFlags;
	if ((surfaceUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
		return ERROR_VULKAN_TBD;
	}
	
	vulkanWindowSwapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	vulkanWindowSwapchainInfo.pNext = NULL;
	vulkanWindowSwapchainInfo.flags = 0;
	//vulkanWindowSwapchainInfo.surface = vulkanWindowSurface;
	vulkanWindowSwapchainInfo.minImageCount = VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT;
	vulkanWindowSwapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM; //sRGB format for linear filtering?
	vulkanWindowSwapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; //Double check
	vulkanWindowSwapchainInfo.imageExtent.width = surfaceCapabilities.currentExtent.width; //Look into VkSwapchainPresentScalingCreateInfoEXT
	vulkanWindowSwapchainInfo.imageExtent.height = surfaceCapabilities.currentExtent.height;
	vulkanWindowSwapchainInfo.imageArrayLayers = 1; // 1 for 2d screen visual
	
	vulkanWindowSwapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT; //| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT ... Add usage based on operations to the images
	
	vulkanWindowSwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //Since Graphics and Presentation Queue are the same
	vulkanWindowSwapchainInfo.queueFamilyIndexCount = 0;
	vulkanWindowSwapchainInfo.pQueueFamilyIndices = NULL;
	
	vulkanWindowSwapchainInfo.preTransform = surfaceCapabilities.currentTransform;
	vulkanWindowSwapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	vulkanWindowSwapchainInfo.presentMode = VULKAN_TARGET_PRESENTATION_MODE;
	vulkanWindowSwapchainInfo.clipped = VK_TRUE;
	vulkanWindowSwapchainInfo.oldSwapchain = VK_NULL_HANDLE;
	
	result = vkCreateSwapchainKHR(vulkanWindowDevice, &vulkanWindowSwapchainInfo, VULKAN_ALLOCATOR, &vulkanWindowSwapchain);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	uint32_t swapchainImageCount = VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT;
	result = vkGetSwapchainImagesKHR(vulkanWindowDevice, vulkanWindowSwapchain, &swapchainImageCount, vulkanWindowSwapchainImages);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	if (swapchainImageCount != VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT) {
		return ERROR_VULKAN_TBD;
	}
	
	vulkanWindowImgMemBar.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	vulkanWindowImgMemBar.pNext = NULL;
	vulkanWindowImgMemBar.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	vulkanWindowImgMemBar.srcAccessMask = VK_ACCESS_2_NONE;
	vulkanWindowImgMemBar.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	vulkanWindowImgMemBar.dstAccessMask = VK_ACCESS_2_NONE;
	vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	vulkanWindowImgMemBar.srcQueueFamilyIndex = *graphicsTransferPresentationQFI;
	vulkanWindowImgMemBar.dstQueueFamilyIndex = *graphicsTransferPresentationQFI;
	vulkanWindowImgMemBar.image = VK_NULL_HANDLE;
	vulkanWindowImgMemBar.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vulkanWindowImgMemBar.subresourceRange.baseMipLevel = 0;
	vulkanWindowImgMemBar.subresourceRange.levelCount = 1;
	vulkanWindowImgMemBar.subresourceRange.baseArrayLayer = 0;
	vulkanWindowImgMemBar.subresourceRange.layerCount = 1;
	
	//Swapchain Command Pool/Buffer Creation
	VkCommandPoolCreateInfo poolInfo;
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.pNext = NULL;
	poolInfo.flags = 0; //Double check in future...
	poolInfo.queueFamilyIndex = *graphicsTransferPresentationQFI;
	
	result = vkCreateCommandPool(vulkanWindowDevice, &poolInfo, VULKAN_ALLOCATOR, &vulkanWindowCommandPool);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COMMAND_POOL_FAILED;
	}
	
	
	//Command Buffer Allocation:
	VkCommandBufferAllocateInfo cmdBufAllocInfo;
	cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocInfo.pNext = NULL;
	cmdBufAllocInfo.commandPool = vulkanWindowCommandPool;
	cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocInfo.commandBufferCount = 1;
	
	for (uint32_t i=0; i<VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT; i++) {
		vulkanWindowCommandBufferSubmitInfos[i].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		vulkanWindowCommandBufferSubmitInfos[i].pNext = NULL;
		
		result = vkAllocateCommandBuffers(vulkanWindowDevice, &cmdBufAllocInfo, &(vulkanWindowCommandBufferSubmitInfos[i].commandBuffer));
		if (result != VK_SUCCESS) {
			return ERROR_VULKAN_COMMAND_BUFFER_FAILED;
		}
		
		vulkanWindowCommandBufferSubmitInfos[i].deviceMask = 0;		
	}
	
	vulkanWindowState = VULKAN_WINDOW_STATE_CREATED;
	return 0;
}

int vulkanWindowProcessMessages(uint64_t* windowInformation) { //Maybe check state
	if (vulkanWindowState == VULKAN_WINDOW_STATE_UNDEFINED) {
		return ERROR_VULKAN_TBD;
	}
	
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0) { //vulkanWindowHandle maybe... check again in future
		DispatchMessage(&msg);
		//TranslateMessage(&msg);
		//LRESULT result = DispatchMessage(&msg);
	}
	
	if (vulkanWindowState == VULKAN_WINDOW_STATE_UNDEFINED) {
		*windowInformation |= 1;
	}
	else if (vulkanWindowState == VULKAN_WINDOW_STATE_RUNNING) {
		*windowInformation |= 2;
	}
	
	return 0;
}

static VkDependencyInfo vulkanWindowDependencyInfo;
static VkBlitImageInfo2 vulkanWindowBlitInfo;
static VkImageBlit2 vulkanWindowBlitImageInfo;

static VkSemaphoreSubmitInfo vulkanWindowSemaphoreAcquiredSwapchainImage;
static VkSemaphoreSubmitInfo vulkanWindowSemaphoreCommandBufferFinished;
static VkSubmitInfo2 vulkanWindowSubmitInfo;
static VkPresentInfoKHR vulkanWindowPresentationInfo;

int vulkanWindowStart(uint64_t renderWidth, uint64_t renderHeight, VkImage inputImage, VkFence copyFence) {
	if (vulkanWindowState != VULKAN_WINDOW_STATE_CREATED) {
		return ERROR_VULKAN_TBD;
	}
	
	//Input Image to Swapchain Images
	VkCommandBuffer transfer0 = vulkanWindowCommandBufferSubmitInfos[0].commandBuffer;
	
	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = NULL;
	beginInfo.flags = 0; //Double check in future...
	beginInfo.pInheritanceInfo = NULL; // Optional
	
	VkResult result = vkBeginCommandBuffer(transfer0, &beginInfo);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
	}
	
	vulkanWindowDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	vulkanWindowDependencyInfo.pNext = NULL;
	vulkanWindowDependencyInfo.dependencyFlags = 0;
	vulkanWindowDependencyInfo.memoryBarrierCount = 0;
	vulkanWindowDependencyInfo.pMemoryBarriers = NULL;
	vulkanWindowDependencyInfo.bufferMemoryBarrierCount = 0;
	vulkanWindowDependencyInfo.pBufferMemoryBarriers = NULL;
	vulkanWindowDependencyInfo.imageMemoryBarrierCount = 1;
	vulkanWindowDependencyInfo.pImageMemoryBarriers = &vulkanWindowImgMemBar;
	
	vulkanWindowImgMemBar.image = inputImage;
	vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vkCmdPipelineBarrier2(transfer0, &vulkanWindowDependencyInfo);
	
	vulkanWindowImgMemBar.image = vulkanWindowSwapchainImages[0];
	//vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vkCmdPipelineBarrier2(transfer0, &vulkanWindowDependencyInfo);
	
	vulkanWindowBlitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	vulkanWindowBlitInfo.pNext = NULL;
	vulkanWindowBlitInfo.srcImage = inputImage;
	vulkanWindowBlitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	
	vulkanWindowBlitInfo.dstImage = vulkanWindowSwapchainImages[0];
	vulkanWindowBlitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	
	vulkanWindowBlitInfo.regionCount = 1;
	
	vulkanWindowBlitImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	vulkanWindowBlitImageInfo.pNext = NULL;
	
	vulkanWindowBlitImageInfo.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vulkanWindowBlitImageInfo.srcSubresource.mipLevel = 0;
	vulkanWindowBlitImageInfo.srcSubresource.baseArrayLayer = 0;
	vulkanWindowBlitImageInfo.srcSubresource.layerCount = 1;
	
	vulkanWindowBlitImageInfo.srcOffsets[0].x = 0;
	vulkanWindowBlitImageInfo.srcOffsets[0].y = 0;
	vulkanWindowBlitImageInfo.srcOffsets[0].z = 0;
	vulkanWindowBlitImageInfo.srcOffsets[1].x = renderWidth;
	vulkanWindowBlitImageInfo.srcOffsets[1].y = renderHeight;
	vulkanWindowBlitImageInfo.srcOffsets[1].z = 1;
	
	vulkanWindowBlitImageInfo.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	vulkanWindowBlitImageInfo.dstSubresource.mipLevel = 0;
	vulkanWindowBlitImageInfo.dstSubresource.baseArrayLayer = 0;
	vulkanWindowBlitImageInfo.dstSubresource.layerCount = 1;
	
	vulkanWindowBlitImageInfo.dstOffsets[0].x = 0;
	vulkanWindowBlitImageInfo.dstOffsets[0].y = 0;
	vulkanWindowBlitImageInfo.dstOffsets[0].z = 0;
	vulkanWindowBlitImageInfo.dstOffsets[1].x = vulkanWindowSwapchainInfo.imageExtent.width;
	vulkanWindowBlitImageInfo.dstOffsets[1].y = vulkanWindowSwapchainInfo.imageExtent.height;
	vulkanWindowBlitImageInfo.dstOffsets[1].z = 1;
	
	vulkanWindowBlitInfo.pRegions = &vulkanWindowBlitImageInfo;
	
	vulkanWindowBlitInfo.filter = VK_FILTER_NEAREST; //VK_FILTER_LINEAR
	
	vkCmdBlitImage2(transfer0, &vulkanWindowBlitInfo);
	
	vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	
	vkCmdPipelineBarrier2(transfer0, &vulkanWindowDependencyInfo);
	
	result = vkEndCommandBuffer(transfer0);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_COM_BUF_END_FAILED;
	}
	
	for (uint32_t i=1; i<VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT; i++) {
		VkCommandBuffer cmdBuffer = vulkanWindowCommandBufferSubmitInfos[i].commandBuffer;	
		result = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		if (result != VK_SUCCESS) {
			return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
		}
		
		vulkanWindowImgMemBar.image = vulkanWindowBlitInfo.srcImage;
		vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		vkCmdPipelineBarrier2(cmdBuffer, &vulkanWindowDependencyInfo);
		
		vulkanWindowImgMemBar.image = vulkanWindowSwapchainImages[i];
		//vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier2(cmdBuffer, &vulkanWindowDependencyInfo);
		
		vulkanWindowBlitInfo.dstImage = vulkanWindowSwapchainImages[i];
		vkCmdBlitImage2(cmdBuffer, &vulkanWindowBlitInfo);
		
		//vulkanWindowImgMemBar.image = vulkanWindowSwapchainImages[i];
		vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier2(cmdBuffer, &vulkanWindowDependencyInfo);
		
		result = vkEndCommandBuffer(cmdBuffer);
		if (result != VK_SUCCESS) {
			return ERROR_VULKAN_COM_BUF_END_FAILED;
		}
	}
	
	
	//Create Semaphores:
	vulkanWindowSemaphoreAcquiredSwapchainImage.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	vulkanWindowSemaphoreAcquiredSwapchainImage.pNext = NULL;
	vulkanWindowSemaphoreAcquiredSwapchainImage.semaphore = VK_NULL_HANDLE;
	vulkanWindowSemaphoreAcquiredSwapchainImage.value = 0;
	vulkanWindowSemaphoreAcquiredSwapchainImage.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; //Wait BEFORE every command gets executed
	vulkanWindowSemaphoreAcquiredSwapchainImage.deviceIndex = 0;
	
	vulkanWindowSemaphoreCommandBufferFinished.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	vulkanWindowSemaphoreCommandBufferFinished.pNext = NULL;
	vulkanWindowSemaphoreCommandBufferFinished.semaphore = VK_NULL_HANDLE;
	vulkanWindowSemaphoreCommandBufferFinished.value = 0;
	vulkanWindowSemaphoreCommandBufferFinished.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT; //Signal AFTER all commands are finished
	vulkanWindowSemaphoreCommandBufferFinished.deviceIndex = 0;
	
	VkSemaphoreCreateInfo semaphoreInfo;
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = NULL;
	semaphoreInfo.flags = 0; //Always zero since it is currently reserved
	
	result = vkCreateSemaphore(vulkanWindowDevice, &semaphoreInfo, VULKAN_ALLOCATOR, &(vulkanWindowSemaphoreAcquiredSwapchainImage.semaphore));
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	result = vkCreateSemaphore(vulkanWindowDevice, &semaphoreInfo, VULKAN_ALLOCATOR, &(vulkanWindowSemaphoreCommandBufferFinished.semaphore));
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	
	const uint64_t timeoutSecond = 1000000000;
	uint32_t imageIndex;
  result = vkAcquireNextImageKHR(vulkanWindowDevice, vulkanWindowSwapchain, timeoutSecond, vulkanWindowSemaphoreAcquiredSwapchainImage.semaphore, VK_NULL_HANDLE, &imageIndex);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD; //This function should be called before stuff goes out of date...
	}	
	
	vulkanWindowSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	vulkanWindowSubmitInfo.pNext = NULL;
	vulkanWindowSubmitInfo.flags = 0;
	vulkanWindowSubmitInfo.waitSemaphoreInfoCount = 1;
	vulkanWindowSubmitInfo.pWaitSemaphoreInfos = &vulkanWindowSemaphoreAcquiredSwapchainImage;
	vulkanWindowSubmitInfo.commandBufferInfoCount = 1;
	vulkanWindowSubmitInfo.pCommandBufferInfos = &(vulkanWindowCommandBufferSubmitInfos[imageIndex]);
	vulkanWindowSubmitInfo.signalSemaphoreInfoCount = 1;
	vulkanWindowSubmitInfo.pSignalSemaphoreInfos = &vulkanWindowSemaphoreCommandBufferFinished;
	
	result = vkQueueSubmit2(vulkanWindowGraphicsPresentationQueue, 1, &vulkanWindowSubmitInfo, copyFence);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	vulkanWindowPresentationInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	vulkanWindowPresentationInfo.waitSemaphoreCount = 1;
	vulkanWindowPresentationInfo.pWaitSemaphores = &(vulkanWindowSemaphoreCommandBufferFinished.semaphore);
	vulkanWindowPresentationInfo.swapchainCount = 1;
	vulkanWindowPresentationInfo.pSwapchains = &vulkanWindowSwapchain;
	vulkanWindowPresentationInfo.pImageIndices = &imageIndex;
	vulkanWindowPresentationInfo.pResults = NULL; // Optional
	
	vkQueuePresentKHR(vulkanWindowGraphicsPresentationQueue, &vulkanWindowPresentationInfo);
	
	
	vulkanWindowState = VULKAN_WINDOW_STATE_RUNNING;
	return 0;
}

int vulkanWindowRenderNext(VkFence copyFence) {
	if (vulkanWindowState < VULKAN_WINDOW_STATE_RUNNING) {
		return ERROR_VULKAN_TBD;
	}
	
	if (vulkanWindowState == VULKAN_WINDOW_STATE_PAUSED) {
		return ERROR_VULKAN_WINDOW_IS_PAUSED;
	}
	
	//Display Updated Frame
	const uint64_t timeoutSecond = 1000000000;
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(vulkanWindowDevice, vulkanWindowSwapchain, timeoutSecond, vulkanWindowSemaphoreAcquiredSwapchainImage.semaphore, VK_NULL_HANDLE, &imageIndex);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			return ERROR_VULKAN_WINDOW_MUST_FIX;
		}
		else if (result == VK_SUBOPTIMAL_KHR) {
			return ERROR_VULKAN_WINDOW_SHOULD_FIX;
		}
		else {
			return ERROR_VULKAN_TBD;
		}
	}
	
	vulkanWindowSubmitInfo.pCommandBufferInfos = &(vulkanWindowCommandBufferSubmitInfos[imageIndex]);
	result = vkQueueSubmit2(vulkanWindowGraphicsPresentationQueue, 1, &vulkanWindowSubmitInfo, copyFence);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	vulkanWindowPresentationInfo.pImageIndices = &imageIndex;
	vkQueuePresentKHR(vulkanWindowGraphicsPresentationQueue, &vulkanWindowPresentationInfo);
	
	
	return 0;
}

int vulkanWindowResize() {
	if (vulkanWindowState < VULKAN_WINDOW_STATE_RUNNING) {
		return ERROR_VULKAN_TBD;
	}
	
	//consoleWriteLineFast("Resize Called!", 14);
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanPhysicalDevice, vulkanWindowSwapchainInfo.surface, &surfaceCapabilities);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	uint32_t width = surfaceCapabilities.currentExtent.width;
	uint32_t height = surfaceCapabilities.currentExtent.height;
	
	if ((width == vulkanWindowSwapchainInfo.imageExtent.width) && (height == vulkanWindowSwapchainInfo.imageExtent.height)) {
		return 0;
	}
	
	//consoleWriteLineWithNumberFast("New Width:  ", 12, width,  NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleWriteLineWithNumberFast("New Height: ", 12, height, NUM_FORMAT_UNSIGNED_INTEGER);
	//consoleBufferFlush();
	
	if (vulkanWindowState == VULKAN_WINDOW_STATE_RUNNING) {
		result = vkQueueWaitIdle(vulkanWindowGraphicsPresentationQueue);
		if (result != VK_SUCCESS) {
			vulkanExtraInfo = result;
			return ERROR_VULKAN_TBD;
		}
		
		//vkFreeCommandBuffers(vulkanWindowDevice, vulkanWindowCommandPool, VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT, vulkanWindowCommandBuffers);
		vkResetCommandPool(vulkanWindowDevice, vulkanWindowCommandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
		vkDestroySwapchainKHR(vulkanWindowDevice, vulkanWindowSwapchain, VULKAN_ALLOCATOR);
	}
	
	vulkanWindowSwapchainInfo.imageExtent.width = width;
	vulkanWindowSwapchainInfo.imageExtent.height = height;
	
	if ((width == 0) || (height == 0)) {
		vulkanWindowState = VULKAN_WINDOW_STATE_PAUSED;
		return 0;
	}
	
	//vulkanWindowSwapchainInfo.preTransform = surfaceCapabilities.currentTransform;
	//vulkanWindowSwapchainInfo.oldSwapchain = VK_NULL_HANDLE;
	
	result = vkCreateSwapchainKHR(vulkanWindowDevice, &vulkanWindowSwapchainInfo, VULKAN_ALLOCATOR, &vulkanWindowSwapchain);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	
	uint32_t swapchainImageCount = VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT;
	result = vkGetSwapchainImagesKHR(vulkanWindowDevice, vulkanWindowSwapchain, &swapchainImageCount, vulkanWindowSwapchainImages);
	if (result != VK_SUCCESS) {
		vulkanExtraInfo = result;
		return ERROR_VULKAN_TBD;
	}
	if (swapchainImageCount != VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT) {
		return ERROR_VULKAN_TBD;
	}
	
	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = NULL;
	beginInfo.flags = 0; //Double check in future...
	beginInfo.pInheritanceInfo = NULL; // Optional
	
	vulkanWindowBlitImageInfo.dstOffsets[1].x = width;
	vulkanWindowBlitImageInfo.dstOffsets[1].y = height;
	
	for (uint32_t i=0; i<swapchainImageCount; i++) {
		VkCommandBuffer cmdBuffer = vulkanWindowCommandBufferSubmitInfos[i].commandBuffer;	
		result = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		if (result != VK_SUCCESS) {
			return ERROR_VULKAN_COM_BUF_BEGIN_FAILED;
		}
		
		vulkanWindowImgMemBar.image = vulkanWindowBlitInfo.srcImage;
		vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		vkCmdPipelineBarrier2(cmdBuffer, &vulkanWindowDependencyInfo);
		
		vulkanWindowImgMemBar.image = vulkanWindowSwapchainImages[i];
		//vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vkCmdPipelineBarrier2(cmdBuffer, &vulkanWindowDependencyInfo);
		
		vulkanWindowBlitInfo.dstImage = vulkanWindowSwapchainImages[i];
		vkCmdBlitImage2(cmdBuffer, &vulkanWindowBlitInfo);
		
		//vulkanWindowImgMemBar.image = vulkanWindowSwapchainImages[i];
		vulkanWindowImgMemBar.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		vulkanWindowImgMemBar.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCmdPipelineBarrier2(cmdBuffer, &vulkanWindowDependencyInfo);
		
		result = vkEndCommandBuffer(cmdBuffer);
		if (result != VK_SUCCESS) {
			return ERROR_VULKAN_COM_BUF_END_FAILED;
		}
	}
	
	vulkanWindowState = VULKAN_WINDOW_STATE_RUNNING;
	return 0;
}

int vulkanWindowFullscreenToggle() {
	if (vulkanWindowState != VULKAN_WINDOW_STATE_RUNNING) {
		return ERROR_VULKAN_TBD;
	}
	
	DWORD dwStyle = GetWindowLong(vulkanWindowHandle, GWL_STYLE);
	if ((dwStyle & WS_OVERLAPPEDWINDOW) != 0) { //Window'd Mode
		MONITORINFO mi = { sizeof(mi) };
		if (GetWindowPlacement(vulkanWindowHandle, &vulkanWindowPlacement) && 
				GetMonitorInfo(MonitorFromWindow(vulkanWindowHandle, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowLong(vulkanWindowHandle, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(vulkanWindowHandle, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
										mi.rcMonitor.right - mi.rcMonitor.left,
										mi.rcMonitor.bottom - mi.rcMonitor.top,
										SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else { //Fullscreen Mode
		SetWindowLong(vulkanWindowHandle, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(vulkanWindowHandle, &vulkanWindowPlacement);
		SetWindowPos(vulkanWindowHandle, NULL, 0, 0, 0, 0,
									SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
									SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
	
	
	return 0;
}

void vulkanWindowCleanup() {
	if (vulkanWindowState >= VULKAN_WINDOW_STATE_RUNNING) {
		vkQueueWaitIdle(vulkanWindowGraphicsPresentationQueue); //Wait for good measure...
		vkDestroySemaphore(vulkanWindowDevice, vulkanWindowSemaphoreCommandBufferFinished.semaphore, VULKAN_ALLOCATOR);
		vulkanWindowSemaphoreCommandBufferFinished.semaphore = VK_NULL_HANDLE;
		vkDestroySemaphore(vulkanWindowDevice, vulkanWindowSemaphoreAcquiredSwapchainImage.semaphore, VULKAN_ALLOCATOR);
		vulkanWindowSemaphoreAcquiredSwapchainImage.semaphore = VK_NULL_HANDLE;
	}
	if (vulkanWindowState >= VULKAN_WINDOW_STATE_CREATED) {
		//vkFreeCommandBuffers(vulkanWindowDevice, vulkanWindowCommandPool, VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT, vulkanWindowCommandBuffers);
		vkDestroyCommandPool(vulkanWindowDevice, vulkanWindowCommandPool, VULKAN_ALLOCATOR); //calls vkFreeCommandBuffersInternally
		vulkanWindowCommandPool = VK_NULL_HANDLE;
		vkDestroySwapchainKHR(vulkanWindowDevice, vulkanWindowSwapchain, VULKAN_ALLOCATOR);
		for (uint32_t i=0; i<VULKAN_TARGET_SWAPCHAIN_IMAGE_COUNT; i++) {
			vulkanWindowSwapchainImages[i] = VK_NULL_HANDLE;
		}
		vulkanWindowSwapchain = VK_NULL_HANDLE;
		vkDestroySurfaceKHR(vulkanInstance, vulkanWindowSwapchainInfo.surface, VULKAN_ALLOCATOR);
		vulkanWindowSwapchainInfo.surface = VK_NULL_HANDLE;
		
		vulkanWindowGraphicsPresentationQueue = VK_NULL_HANDLE;
		vulkanWindowDevice = VK_NULL_HANDLE;
		
		vulkanWindowResizeDisabled = 0;
		vulkanWindowState = VULKAN_WINDOW_STATE_UNDEFINED;
		
		DestroyWindow(vulkanWindowHandle);
		MSG msg;
		while (GetMessage(&msg, NULL, 0,  0) != 0) {
			DispatchMessage(&msg);
			//TranslateMessage(&msg);
			//LRESULT result = DispatchMessage(&msg);
		}
		vulkanWindowHandle = NULL;
	}
}





static int nvidiaError = 0;
void nvidiaGetError(int* error) {
	*error = nvidiaError;
}

//Nvidia Cuda State:
#define NVIDIA_CUDA_STATE_UNDEFINED 0
#define NVIDIA_CUDA_STATE_SETUP 1
static uint64_t nvidiaCudaState = NVIDIA_CUDA_STATE_UNDEFINED;

struct NvidiaCudaFunctionsPrivate {
	PFN_cuInit cuInit;
	PFN_cuDriverGetVersion cuDriverGetVersion;
	PFN_cuDeviceGetCount cuDeviceGetCount;
	PFN_cuDeviceGet cuDeviceGet;
	PFN_cuDeviceGetLuid cuDeviceGetLuid;
	PFN_cuImportExternalMemory cuImportExternalMemory;
};

static struct NvidiaCudaFunctionsPrivate nvCuFunPrivate;
static void* nvidiaCudaLibrary = NULL;
int nvidiaCudaSetup(CUdevice* cudaDevice, NvidiaCudaFunctions* nvCuFun) {
	if (nvidiaCudaState != NVIDIA_CUDA_STATE_UNDEFINED) {
		return 0;
	}
	
	//consoleWriteLineFast("Load Library!", 13);
	
	int error = ioLoadLibrary(&nvidiaCudaLibrary, "nvcuda");
	RETURN_ON_ERROR(error);
	
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuInit", (void**) &nvCuFunPrivate.cuInit);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuDriverGetVersion", (void**) &nvCuFunPrivate.cuDriverGetVersion);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuDeviceGetCount", (void**) &nvCuFunPrivate.cuDeviceGetCount);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuDeviceGet", (void**) &nvCuFunPrivate.cuDeviceGet);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuDeviceGetLuid", (void**) &nvCuFunPrivate.cuDeviceGetLuid);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuImportExternalMemory", (void**) &nvCuFunPrivate.cuImportExternalMemory);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuDevicePrimaryCtxGetState", (void**) &nvCuFun->cuDevicePrimaryCtxGetState);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuDevicePrimaryCtxRetain", (void**) &nvCuFun->cuDevicePrimaryCtxRetain);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuCtxPushCurrent", (void**) &nvCuFun->cuCtxPushCurrent);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuCtxPopCurrent", (void**) &nvCuFun->cuCtxPopCurrent);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuCtxGetLimit", (void**) &nvCuFun->cuCtxGetLimit);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuCtxSetLimit", (void**) &nvCuFun->cuCtxSetLimit);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuExternalMemoryGetMappedMipmappedArray", (void**) &nvCuFun->cuExternalMemoryGetMappedMipmappedArray);
	RETURN_ON_ERROR(error);
	error = ioGetLibraryFunction(nvidiaCudaLibrary, "cuMipmappedArrayGetLevel", (void**) &nvCuFun->cuMipmappedArrayGetLevel);
	RETURN_ON_ERROR(error);
	
	
	// Get graphics adapter for primary monitor
	error = graphicsSetupAdapter(1);
	RETURN_ON_ERROR(error);
	
	//Setup CUDA interface first
	CUresult cuRes = nvCuFunPrivate.cuInit(0);
	if (cuRes != CUDA_SUCCESS) {
		nvidiaError = (int) cuRes;
		return ERROR_CUDA_NO_INIT;
	}
	
	//Do CUDA Version check in future once knowing what to compare it to
	int cudaVersion = 0;
	cuRes = nvCuFunPrivate.cuDriverGetVersion(&cudaVersion);
	if (cuRes != CUDA_SUCCESS) {
		nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_GET_VERSION;
	}
	//consoleWriteLineWithNumberFast("Cuda Version: ", 14, (uint64_t) cudaVersion, NUM_FORMAT_UNSIGNED_INTEGER);
	if (cudaVersion < 10000) {
		return ERROR_CUDA_LOW_VERSION;
	}
	
	
	CUdevice nvidiaCudaDevice = 0;
	int numCudaDevices = 0;
	cuRes = nvCuFunPrivate.cuDeviceGetCount(&numCudaDevices);
	if (cuRes != CUDA_SUCCESS) {
		nvidiaError = (int) cuRes;
		return ERROR_CUDA_NO_DEVICES;
	}
	if (numCudaDevices == 0) {
		return ERROR_CUDA_NO_DEVICES;
	}
	
	//int cudaDeviceNum = 0;
	for (int d=0; d<numCudaDevices; d++) {
		cuRes = nvCuFunPrivate.cuDeviceGet(&nvidiaCudaDevice, 0);
		if (cuRes != CUDA_SUCCESS) {
			nvidiaError = (int) cuRes;
			return ERROR_CUDA_CANNOT_GET_DEVICE;
		}
		
		char luid[16] = {};
		unsigned int deviceNodeMask = 0;
		cuRes = nvCuFunPrivate.cuDeviceGetLuid(luid, &deviceNodeMask, nvidiaCudaDevice);
		if (cuRes != CUDA_SUCCESS) {
			nvidiaError = (int) cuRes;
			return ERROR_CUDA_CANNOT_GET_DEVICE_LUID;
		}
		
		uint32_t* luidConversion = (uint32_t*) luid;
		//consoleWriteLineWithNumberFast("UUID Low:  ", 11, luidConversion[0], NUM_FORMAT_PARTIAL_HEXADECIMAL);
		//consoleWriteLineWithNumberFast("UUID High: ", 11, luidConversion[1], NUM_FORMAT_PARTIAL_HEXADECIMAL);
		if (luidConversion[0] == graphicsAdapterID.LowPart) {
			if (luidConversion[1] == graphicsAdapterID.HighPart) {
				//cudaDeviceNum = d;
				d = numCudaDevices; //break
			}
		}
	}
	
	*cudaDevice = nvidiaCudaDevice;
	nvidiaCudaState = NVIDIA_CUDA_STATE_SETUP;
	return 0;
}

int nvidiaCudaImportVulkanMemory(VkDevice device, VkImage exportImage, VkDeviceMemory exportMemory, CUexternalMemory* cuExtMem) {
	if (device == VK_NULL_HANDLE) {
		return ERROR_ARGUMENT_DNE;
	}
	if (exportMemory == VK_NULL_HANDLE) {
		return ERROR_ARGUMENT_DNE;
	}
	if (cuExtMem == NULL) {
		return ERROR_ARGUMENT_DNE;
	}
	if (nvidiaCudaState != NVIDIA_CUDA_STATE_SETUP) {
		return 0;
	}
	
	VkImageMemoryRequirementsInfo2 imageMemReqsInfo;
	imageMemReqsInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
	imageMemReqsInfo.pNext = NULL;
	imageMemReqsInfo.image = exportImage;
	
	VkMemoryRequirements2 memReqs2;
	memReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
	memReqs2.pNext = NULL;
	vkGetImageMemoryRequirements2(device, &imageMemReqsInfo, &memReqs2);
	//consoleWriteLineWithNumberFast("Size: ", 6, memReqs2.memoryRequirements.size, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	VkDeviceSize byteSize = memReqs2.memoryRequirements.size;
	
	PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR2 = (PFN_vkGetMemoryWin32HandleKHR) (vkGetDeviceProcAddr(device, "vkGetMemoryWin32HandleKHR"));
	
	VkMemoryGetWin32HandleInfoKHR win32HandleInfo;
	win32HandleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
	win32HandleInfo.pNext = NULL;
	win32HandleInfo.memory = exportMemory;
	win32HandleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	
	HANDLE memoryHandle = NULL;
	VkResult result = vkGetMemoryWin32HandleKHR2(device, &win32HandleInfo, &memoryHandle);
	if (result != VK_SUCCESS) {
		return ERROR_VULKAN_EXTRA_INFO;
	}
	//consoleWriteLineWithNumberFast("Size B: ", 8, (uint64_t) byteSize, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	//consoleWriteLineWithNumberFast("Handle: ", 8, (uint64_t) memoryHandle, NUM_FORMAT_PARTIAL_HEXADECIMAL);
	
	
	CUDA_EXTERNAL_MEMORY_HANDLE_DESC extMemHandle = {0};
	extMemHandle.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32;
	extMemHandle.handle.win32.handle = memoryHandle;
	extMemHandle.handle.win32.name = NULL; //Doesn't work right?
	extMemHandle.size = byteSize;
	extMemHandle.flags = CUDA_EXTERNAL_MEMORY_DEDICATED; //correct based on vulkan external memory allocation
	
	CUresult cuRes = nvCuFunPrivate.cuImportExternalMemory(cuExtMem, &extMemHandle);
	if (cuRes != CUDA_SUCCESS) {
		nvidiaError = (int) cuRes;
		return ERROR_CUDA_CANNOT_IMPORT_MEMORY;
	}
	
	CloseHandle(memoryHandle);
	
	return 0;
}

void nvidiaCudaCleanup() {
	
}



