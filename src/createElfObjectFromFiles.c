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


//Mini helper program that creates an x64 ELF object file to contain the binary
//data from inputted files that can be used in the program linker such as ld
//Each file with create two symbols: filename_size and filename_data
//and can be accessable with code such as:
//  extern uint64_t filename_size;
//  extern uint8_t  filename_data[];
//The size part will reside in the .binSize section and be 8-byte aligned while
//the data part will reside in the .binData section and be 64-byte aligned
//Program will return the number of successful converted input files otherwise
//an error will be indicated by a negative return value


//Include C runtime library headers for simple portable mini helper program
#include <stdint.h>	//Defines Data Types: https://en.wikipedia.org/wiki/C_data_types
#include <stdlib.h>	//Needed for easy dynamic memory operations malloc & free
#include <stdio.h>	//Needed for printf statements and general file operations

#include "elf.h"   //Defines the various ELF headers

#define ERROR_NOT_ENOUGH_ARGUMENTS -1
#define ERROR_TOO_MANY_ARGUMENTS -2
#define ERROR_ELF_OUTPUT_NOT_OPENABLE -3
#define ERROR_ELF_WRITE_PROBLEM -4
#define ERROR_MALLOC_RETURN_NULL -5
#define ERROR_INPUT_READ_PROBLEM -6
#define ERROR_BYTE_SIZE_MISMATCH -7

//ELF File Object Writer for everything but the raw data
int elfFileWrite(FILE* elfFile, uint32_t symbNameSize, uint32_t numInputs, uint8_t* symbNames, uint64_t* dataSizes) {
	uint8_t sectionNames[] = "\0.shstrtab\0.strtab\0.symtab\0.binSize\0.binData\0"; //An extra 0 will be added due to C strings
	uint16_t sectNumEntries = 6;
	uint64_t sectNameSize = sizeof(sectionNames);
	
	uint16_t fileHeaderSize = sizeof(struct elfHeader64); //64 bytes 0x40
	uint16_t sectHeaderSize = sizeof(struct elfSectionHeader64); //64 bytes 0x40
	struct elfHeader64 fileHeader = {
		0x464C457F, 		//Magic Number ID
		2,							//Bit Format (2 = 64-bit)
		1,							//Endianness Format (1 = little endian)
		1,							//ELF Version Field
		0,							//Target ABI
		0,							//Version ABI
		1,							//Object Type (1 = Relocatable File)
		0x3E,						//Target Architecture (0x3E = x86-64)
		1,							//Elf Version Field 2
		0,							//Entry Point (0 = Not an Executable)
		0,							//Program Header Offset (0 = No Program Header)
		fileHeaderSize,	//Section Header Offset (File Header Size = immediately following)
		0,							//Architecture Flags
		fileHeaderSize,	//File Header Size
		0,							//Program Header Size
		0,							//Program Header Entries
		sectHeaderSize,	//Section Header Size
		sectNumEntries,	//Section Header Entries
		1								//Section Header String Name Index
	};
	
	size_t elementsWritten = fwrite(&fileHeader, (size_t) fileHeaderSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	//Always Present Section 0
	struct elfSectionHeader64 sect0Header = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	elementsWritten = fwrite(&sect0Header, (size_t) sectHeaderSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	uint64_t shstrtabOff = fileHeaderSize + (sectHeaderSize * sectNumEntries);
	struct elfSectionHeader64 shstrtabHeader = {
		1,						//Name Offset within section names (.shstrtab)
		3,						//Section Type (3 = String Table)
		0,						//Section Flags
		0,						//Virtual Address
		shstrtabOff,	//Section Offset from Start of File
		sectNameSize,	//Section Size
		0,						//SectionLink
		0,						//Section Info
		1,						//Required Section Alignment (1 = 1 Byte Alignment) in-file 64-byte
		0							//Section Entry Size (0 = Variable String Entry Size)
	};
	elementsWritten = fwrite(&shstrtabHeader, (size_t) sectHeaderSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	uint64_t strtabOff = shstrtabOff + sectNameSize;
	struct elfSectionHeader64 strtabHeader = {
		11,						//Name Offset within section names (.strtab)
		3,						//Section Type (3 = String Table)
		0,						//Section Flags
		0,						//Virtual Address
		strtabOff,		//Section Offset from Start of File
		symbNameSize,	//Section Size
		0,						//SectionLink
		0,						//Section Info
		1,						//Required Section Alignment (1 = 1 Byte Alignment)
		0							//Section Entry Size (0 = Variable String Entry Size)
	};
	elementsWritten = fwrite(&strtabHeader, (size_t) sectHeaderSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	
	uint64_t symtabOff = ((strtabOff + symbNameSize) + 63) & ~63; //with 64 byte padding
	uint64_t symbEntrySize = sizeof(struct elfSymbolTableEntry64); //24 bytes
	uint64_t symtabSize = (1 + (numInputs << 1)) * symbEntrySize;
	struct elfSectionHeader64 symtabHeader = {
		19,						//Name Offset within section names (.symtab)
		2,						//Section Type (2 = Symbol Table)
		0,						//Section Flags
		0,						//Virtual Address
		symtabOff,		//Section Offset from Start of File
		symtabSize,		//Section Size
		2,						//SectionLink (2 = Index of .strtab Section Header)
		1,						//Section Info (1 = No STB_LOCAL ...???)
		8,						//Required Section Alignment (8 = 8 Byte Alignment) in-file 64-byte
		symbEntrySize	//Section Entry Size
	};
	elementsWritten = fwrite(&symtabHeader, (size_t) sectHeaderSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	uint64_t binSizeOff = ((symtabOff + symtabSize) + 63) & ~63; //with 64 byte padding
	uint64_t binSizeSize = numInputs * sizeof(uint64_t);
	struct elfSectionHeader64 binSizeHeader = {
		27,						//Name Offset within section names (.binSize)
		1,						//Section Type (1 = Program Data)
		2,						//Section Flags (2 = Allocated but not writeable)
		0,						//Virtual Address
		binSizeOff,		//Section Offset from Start of File
		binSizeSize,	//Section Size
		0,						//SectionLink
		0,						//Section Info
		8,						//Required Section Alignment (8 = 8 Byte Alignment) in-file 64-byte
		0							//Section Entry Size (0 = Variable String Entry Size)
	};
	elementsWritten = fwrite(&binSizeHeader, (size_t) sectHeaderSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	uint64_t binDataOff = ((binSizeOff + binSizeSize) + 63) & ~63; //with 64 byte padding
	uint64_t binDataSize = 0;
	for (uint32_t i=0; i<numInputs; i++) {
		binDataSize = ((binDataSize + dataSizes[i]) + 63) & ~63;
	}
	struct elfSectionHeader64 binDataHeader = {
		36,						//Name Offset within section names (.binData)
		1,						//Section Type (1 = Program Data)
		2,						//Section Flags (2 = Allocated but not writeable)
		0,						//Virtual Address
		binDataOff,		//Section Offset from Start of File
		binDataSize,	//Section Size
		0,						//SectionLink
		0,						//Section Info
		64,						//Required Section Alignment (64 = 64 Byte Alignment)
		0							//Section Entry Size (0 = Variable String Entry Size)
	};
	elementsWritten = fwrite(&binDataHeader, (size_t) sectHeaderSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	
	elementsWritten = fwrite(sectionNames, (size_t) sectNameSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	elementsWritten = fwrite(symbNames, (size_t) symbNameSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	//64-byte aligned padding in file
	uint64_t padding64[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	size_t paddingSize = symtabOff - (strtabOff + symbNameSize);
	elementsWritten = fwrite(padding64, paddingSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	
	struct elfSymbolTableEntry64 symb0Entry = {0, 0, 0, 0, 0, 0};
	elementsWritten = fwrite(&symb0Entry, (size_t) symbEntrySize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	
	uint32_t symbolNameOffset = 1;
	uint64_t symbolSizeOffset = 0;
	uint64_t symbolDataOffset = 0;
	for (uint32_t i=0; i<numInputs; i++) {
		struct elfSymbolTableEntry64 binSizeSymbolEntry = {
			symbolNameOffset,			//Name Offset within symbol names
			0x11,									//Symbol Binding (0x11 = Global Binding: visible to all objects)
			0,										//Symbol Visibility (0 = Default)
			4,										//Section Index (4 = .binSize Section)
			symbolSizeOffset,			//Section Offset for Symbol Data
			8											//Symbol Data Size in bytes (8 for uint64_t size)
		};
		elementsWritten = fwrite(&binSizeSymbolEntry, (size_t) symbEntrySize, 1, elfFile);
		if (elementsWritten != 1) {
			return ERROR_ELF_WRITE_PROBLEM;
		}
		
		while (symbNames[symbolNameOffset] != 0) {
			symbolNameOffset++;
		}
		symbolNameOffset++;
		
		symbolSizeOffset += 8;
		
		
		struct elfSymbolTableEntry64 binDataSymbolEntry = {
			symbolNameOffset,			//Name Offset within symbol names
			0x11,									//Symbol Binding (0x11 = Global Binding: visible to all objects)
			0,										//Symbol Visibility (0 = Default)
			5,										//Section Index (5 = .binData Section)
			symbolDataOffset,			//Section Offset for Symbol Data
			dataSizes[i]					//Symbol Data Size in bytes
		};
		elementsWritten = fwrite(&binDataSymbolEntry, (size_t) symbEntrySize, 1, elfFile);
		if (elementsWritten != 1) {
			return ERROR_ELF_WRITE_PROBLEM;
		}
		
		while (symbNames[symbolNameOffset] != 0) {
			symbolNameOffset++;
		}
		symbolNameOffset++;
		
		symbolDataOffset = ((symbolDataOffset + dataSizes[i]) + 63) & ~63; //with 64 byte padding
		
	}	
	
	paddingSize = binSizeOff - (symtabOff + symtabSize);
	elementsWritten = fwrite(padding64, paddingSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	elementsWritten = fwrite(dataSizes, 8, (size_t) numInputs, elfFile);
	if (elementsWritten != ((size_t) numInputs)) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	paddingSize = binDataOff - (binSizeOff + binSizeSize);
	elementsWritten = fwrite(padding64, paddingSize, 1, elfFile);
	if (elementsWritten != 1) {
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	return 0;
}


//Main C runtime entry point
int main(int argc, char* argv[]) {
	printf("\nCreating ELF object file using binary data from inputted files!\n");
	
	if (argc < 3) {
		printf("Usage: <program> <ELF output file path> <space seperated input files>\n");
		return ERROR_NOT_ENOUGH_ARGUMENTS;
	}
	
	if (argc > 34) {
		printf("ERROR: Too Many Input Files... The max is currently 32!\n");
		return ERROR_TOO_MANY_ARGUMENTS;
	}
	
	char* filePath = argv[1];
	FILE* elfFile = fopen(filePath, "wb");
	if (elfFile == NULL) {
		printf("ELF output file <%s> could NOT be opened for writing\n", filePath);
		return ERROR_ELF_OUTPUT_NOT_OPENABLE;
	}
	printf("ELF output file <%s> opened!\n", filePath);
	
	size_t pageSize = 4096;
	void* memPage = malloc(pageSize);
	if (memPage == NULL) {
		printf("Malloc Page Failed\n");
		return ERROR_MALLOC_RETURN_NULL;
	}
	
	uint8_t* strtabStrings = (uint8_t*) memPage;
	strtabStrings[0] = 0;
	uint32_t strtabStringsIndex = 1;
	
	uint32_t numInputs = (argc - 2);
	FILE* binFiles[32];
	
	uint64_t fileSizes[32];
	
	uint32_t numOfBinaryFilesOpened = 0;
	for (uint32_t i=0; i<numInputs; i++) {
		filePath = argv[i + 2];
		binFiles[i] = fopen(filePath, "rb");
		if (binFiles[i] != NULL) {
			printf("Input file <%s> opened!\n", filePath);
			
			char* fileNameStart = filePath;
			char* filePathTester = filePath;
			while (*filePathTester != 0) {
				if (*filePathTester == '\\') {
					fileNameStart = filePathTester + 1;
				}
				else if (*filePathTester == '/') {
					fileNameStart = filePathTester + 1;
				}
				filePathTester++;
			}
			uint32_t fileNameSize = filePathTester - fileNameStart;
			
			filePathTester = fileNameStart;
			while (*filePathTester != 0) {
				if (*filePathTester == '.') {
					fileNameSize = filePathTester - fileNameStart;
					break;
				}
				filePathTester++;
			}
			
			for (uint32_t c=0; c<fileNameSize; c++) {
				strtabStrings[strtabStringsIndex] = fileNameStart[c];
				strtabStringsIndex++;
			}
			strtabStrings[strtabStringsIndex + 0] = '_';
			strtabStrings[strtabStringsIndex + 1] = 's';
			strtabStrings[strtabStringsIndex + 2] = 'i';
			strtabStrings[strtabStringsIndex + 3] = 'z';
			strtabStrings[strtabStringsIndex + 4] = 'e';
			strtabStrings[strtabStringsIndex + 5] = 0;
			strtabStringsIndex += 6;
			
			for (uint32_t c=0; c<fileNameSize; c++) {
				strtabStrings[strtabStringsIndex] = fileNameStart[c];
				strtabStringsIndex++;
			}
			strtabStrings[strtabStringsIndex + 0] = '_';
			strtabStrings[strtabStringsIndex + 1] = 'd';
			strtabStrings[strtabStringsIndex + 2] = 'a';
			strtabStrings[strtabStringsIndex + 3] = 't';
			strtabStrings[strtabStringsIndex + 4] = 'a';
			strtabStrings[strtabStringsIndex + 5] = 0;
			strtabStringsIndex += 6;
			
			fseek(binFiles[i], 0, SEEK_END); // seek to end of file
			uint64_t fileSize = ftell(binFiles[i]); // get current file pointer
			fseek(binFiles[i], 0, SEEK_SET); // seek back to beginning of file... not always compatible
			
			fileSizes[numOfBinaryFilesOpened] = fileSize;
			numOfBinaryFilesOpened++;
		}
		else {
			printf("Problems opening input file <%s> for reading\n", filePath);
		}
	}
	
	int error = elfFileWrite(elfFile, strtabStringsIndex, numOfBinaryFilesOpened, strtabStrings, fileSizes);
	if (error != 0) {
		printf("ELF file write problem\n");
		return ERROR_ELF_WRITE_PROBLEM;
	}
	
	
	uint64_t padding64[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	uint32_t numFilesConverted = 0;
	for (uint32_t i=0; i<numInputs; i++) {
		if (binFiles[i] != NULL) {
			FILE* workingFile = binFiles[i];
			
			uint64_t fileSize = fileSizes[numFilesConverted];
			uint64_t fileReadBytes = 0;
			
			while (fileSize > fileReadBytes) {
				size_t elementsRead = fread(memPage, 1, pageSize, workingFile);
				if (elementsRead == 0) {
					printf("Input file read problem\n");
					return ERROR_INPUT_READ_PROBLEM;
				}
				size_t elementsWritten = fwrite(memPage, 1, elementsRead, elfFile);
				if (elementsWritten != elementsRead) {
					printf("ELF file write problem\n");
					return ERROR_ELF_WRITE_PROBLEM;
				}
				fileReadBytes += elementsWritten;
			}
			if (fileSize != fileReadBytes) {
				printf("Byte Size Mismatch!\n");
				return ERROR_BYTE_SIZE_MISMATCH;
			}
			
			
			size_t paddingSize = (64 - (fileSize & 63)) & 63;
			size_t elementsWritten = fwrite(padding64, paddingSize, 1, elfFile);
			if (elementsWritten != 1) {
				return ERROR_ELF_WRITE_PROBLEM;
			}
			
			numFilesConverted++;
			
			fclose(workingFile);
		}
	}
	
	
	free(memPage);
	
	fclose(elfFile);
	
	printf("Program Successfully Created the ELF Object File!\n%d input files were used\n", numFilesConverted);
  return 0;
}

