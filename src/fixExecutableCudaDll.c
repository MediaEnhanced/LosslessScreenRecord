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


//Helper program that takes a UTF-8 text file (with a Line Feed (LF) character for End of Line (EOF) termination)
// and converts to line seperated UTF-8 "string" data array in obj(ect) form
//gcc / ld will link this in with the final program

#include <stdint.h>	//Defines Data Types: https://en.wikipedia.org/wiki/C_data_types
#include <stdio.h>	//Needed for printf statements and general file operations

#define EXE_FILE "run.exe"  //Default input location

int main(int argc, char* argv[]) {
  int error = 0;
	printf("\nFix Executable Cuda DLL String Program Started\n");
	
	char* exeFileLocation = EXE_FILE;
	if (argc >= 2) {
		exeFileLocation = argv[1];
	}
	
	FILE* exeFile = fopen(exeFileLocation, "rb+");
	if (exeFile == NULL) {
		printf("Text File %s could not be opened for writing!\n", exeFileLocation);
		error = -1;
	}
	else {
		printf("Executable File %s opened for reading!\n", exeFileLocation);
		
		//size_t numObjectsWritten = fwrite(replaceStr, 1, 5, exeFileLocation);
		char lookStr[] = "nvcuda_loader.dll";
		uint32_t maxChars = 17;
		char currentChar = lookStr[0];
		
		char testChars[8];
		
		uint32_t fileIndex = 0;
		uint32_t matchingChars = 0;
		
		while (matchingChars < maxChars) {
			size_t objectsRead = fread(testChars, 1, 1, exeFile);
			//printf("Objects Read %d\n", objectsRead);
			if (objectsRead == 1) {
				if (testChars[0] != currentChar) {
					fileIndex += matchingChars+1;
					matchingChars = 0;
					currentChar = lookStr[matchingChars];
				}
				else {
					matchingChars++;
					currentChar = lookStr[matchingChars];
				}
			}
			else {
				printf("Error Getting Characters\n");
				break;
			}
		}
		if (matchingChars == maxChars) {
			printf("Found String at %d\n", fileIndex);
			fseek(exeFile, fileIndex, SEEK_SET);
			
			//size_t objectsRead = fread(testChars, 1, 4, exeFile);
			//printf("Read %s\n", testChars);
			
			char replaceStr[] = "nvcuda.dll\0";
			size_t objectsWritten = fwrite(replaceStr, 1, 11, exeFile);
		}
		
		fclose(exeFile);
	}
	
	printf("Program Ended\n");
  return error;
}