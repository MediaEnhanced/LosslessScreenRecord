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

#include "programEntry.h" //Includes "programStrings.h" & "compatibility.h" & <stdint.h>

//Program Main Function
int programMain() {
	consolePrintLine(32);
	
	//Vulkan Create Window
	
	//Open Bitstream File and Extract the Dimensions
	consolePrintLine(33);
	void* h265File = NULL;
	char* inputFileName = "bitstream.h265";
	uint64_t inputFileNameSize = 14;
	int error = ioOpenFile(&h265File, inputFileName, inputFileNameSize, IO_FILE_READ_NORMAL);
	RETURN_ON_ERROR(error);
	
	//Create the Vulkan Video
	
	
	//Show First Frame
	
	//Close File
	error = ioCloseFile(&h265File);
	RETURN_ON_ERROR(error);
	
	//Cleanup ALL Vulkan Elements
	
	
	return 0; //Exit Program Successfully
}

