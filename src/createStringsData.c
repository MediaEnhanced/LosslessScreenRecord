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
#include <stdlib.h>	//Needed for dynamic memory operations malloc & free
#include <stdio.h>	//Needed for printf statements and general file operations

#include "elf.h"

#define STRINGS_FILE "en-us.txt"  //Default input location

#define NUM_CHARACTERS 128

static uint8_t readCharacters[NUM_CHARACTERS];

int main(int argc, char* argv[]) {
  int error = 0;
	printf("\nCreate Strings Data Program Started\n");
	
	char* txtFileLocation = STRINGS_FILE;
	if (argc >= 3) {
		txtFileLocation = argv[2];
	}
	
	FILE* txtFile = fopen(txtFileLocation, "rb");
	if (txtFile == NULL) {
		printf("Text File %s could not be opened and read!\n", txtFileLocation);
		error = -1;
	}
	else {
		char* outFileLocation = "stringsData.o"; //Default output location
		if (argc >= 2) {
			outFileLocation = argv[1];
		}
		
		FILE* dataFile = fopen(outFileLocation, "wb");
		if (dataFile == NULL) {
			printf("Output data file %s could not be opened, or overwritten!\n", outFileLocation);
			error = -1;
		}
		else {
			uint64_t paddingZero = 0;
			uint8_t sectionNames[] = "\0.shstrtab\0.strtab\0.symtab\0.strData\0";
			uint8_t symbolNames[] = "\0stringsData\0stringsIndicies\0";
			
			
			struct elfHeader64 head = {0};
			head.magicNumberID = 0x464C457F;
			head.bitFormat = 2;
			head.endiannessFormat = 1;
			head.elfVersion = 1;
			head.objectType = 1;
			head.architectureTarget = 0x3E;
			head.elfVersion2 = 1;
			head.sectionHeaderOffset = 0x40;
			head.headerSize = sizeof(struct elfHeader64); //0x40
			head.sectionHeaderSize = sizeof(struct elfSectionHeader64); //0x40
			head.sectionHeaderEntries = 5;
			head.sectionHeaderNames = 1; //Section Header Index of .shstrtab
			
			uint64_t fileOffset = head.headerSize;
			fileOffset += head.sectionHeaderSize * head.sectionHeaderEntries;  //ELF Header + (5 * Section Header)
			
			
			struct elfSectionHeader64 sectionHeader0 = {0};
			
			struct elfSectionHeader64 shstrtab = {0};
			shstrtab.nameOffset = 1;
			shstrtab.sectionType = 3; //String Table
			//shstrtab.sectionFlags = 0;
			shstrtab.sectionOffset = fileOffset;
			shstrtab.sectionSize = sizeof(sectionNames);
			shstrtab.sectionAlignment = 1; //8
			
			fileOffset += shstrtab.sectionSize + (8 - (shstrtab.sectionSize % 8)); 
			
			struct elfSectionHeader64 strtab = {0};
			strtab.nameOffset = 11;
			strtab.sectionType = 3; //String Table
			//strtab.sectionFlags = 0;
			strtab.sectionOffset = fileOffset;
			strtab.sectionSize = sizeof(symbolNames);
			strtab.sectionAlignment = 1; //8
			
			fileOffset += strtab.sectionSize + (8 - (strtab.sectionSize % 8)); 
			
			struct elfSectionHeader64 symtab = {0};
			symtab.nameOffset = 19;
			symtab.sectionType = 2; //Symbol Table
			//symtab.sectionFlags = 0;
			symtab.sectionOffset = fileOffset;
			symtab.sectionLink = 2; //Section Header Index of .strtab
			symtab.sectionInfo = 1; //One greater than the symbol table index of the last local symbol (binding STB_LOCAL)... no stb locals, so = 1
			symtab.sectionAlignment = 8;
			symtab.sectionEntrySize = sizeof(struct elfSymbolTableEntry64); //24
			symtab.sectionSize = symtab.sectionEntrySize * 3;
			
			fileOffset += symtab.sectionSize;
			
			struct elfSectionHeader64 rodata = {0};
			rodata.nameOffset = 27; //".strData"
			rodata.sectionType = 1; //Program Data
			rodata.sectionFlags = 2; //Allocated (but not writeable)
			rodata.sectionOffset = fileOffset;
			rodata.sectionSize = 0; //To be seeked to and modified after conversion
			rodata.sectionAlignment = 64; //Cache-Line Alignment
			
			
			struct elfSymbolTableEntry64 symbol0 = {0};
			
			struct elfSymbolTableEntry64 stringsData = {0};
			stringsData.nameOffset = 1; //"stringsData"
			stringsData.symbolInfo = 0x11; //Global Binding (Visible to all objects being combined) and Object Type (data array)
			stringsData.symbolVisibility = 0; //Default... might want to change it based on more research and what gcc/ld does with it anyway when making the executable
			stringsData.sectionIndex = 4; //.rodata Program Data Section
			stringsData.symbolValue = 0; //Offset within specified section
			stringsData.symbolSize = 0; //To be seeked to and modified after conversion //Bytes in the data array
			
			struct elfSymbolTableEntry64 stringsIndicies = {0};
			stringsIndicies.nameOffset = 13; //"stringsIndicies"
			stringsIndicies.symbolInfo = 0x11; //Global Binding (Visible to all objects being combined) and Object Type (data array)
			stringsIndicies.symbolVisibility = 0; //Default... might want to change it based on more research and what gcc/ld does with it anyway when making the executable
			stringsIndicies.sectionIndex = 4; //.rodata Program Data Section
			stringsIndicies.symbolValue = 0; //To be seeked to and modified after conversion //Offset within specified section
			stringsIndicies.symbolSize = 0; //To be seeked to and modified after conversion //Bytes in the data array
			
			
			
			
			size_t elementsWritten = fwrite(&head, 1, head.headerSize, dataFile);
			if (elementsWritten != head.headerSize) {
				printf("File Write Failed!\n");
				error = -2;
			}
			
			if (error == 0) {
				elementsWritten = fwrite(&sectionHeader0, 1, head.sectionHeaderSize, dataFile);
				if (elementsWritten != head.sectionHeaderSize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			if (error == 0) {
				elementsWritten = fwrite(&shstrtab, 1, head.sectionHeaderSize, dataFile);
				if (elementsWritten != head.sectionHeaderSize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			if (error == 0) {
				elementsWritten = fwrite(&strtab, 1, head.sectionHeaderSize, dataFile);
				if (elementsWritten != head.sectionHeaderSize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			if (error == 0) {
				elementsWritten = fwrite(&symtab, 1, head.sectionHeaderSize, dataFile);
				if (elementsWritten != head.sectionHeaderSize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			fpos_t rodataPos;
			fgetpos(dataFile, &rodataPos);
			
			if (error == 0) {
				elementsWritten = fwrite(&rodata, 1, head.sectionHeaderSize, dataFile);
				if (elementsWritten != head.sectionHeaderSize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			if (error == 0) {
				elementsWritten = fwrite(sectionNames, 1, shstrtab.sectionSize, dataFile);
				if (elementsWritten != shstrtab.sectionSize) {
					printf("File Write Failed!\n");
					error = -2;
				}
				
				size_t paddingSize = 8 - (shstrtab.sectionSize % 8);
				fwrite(&paddingZero, 1, paddingSize, dataFile);
			}
			
			if (error == 0) {
				elementsWritten = fwrite(symbolNames, 1, strtab.sectionSize, dataFile);
				if (elementsWritten != strtab.sectionSize) {
					printf("File Write Failed!\n");
					error = -2;
				}
				
				size_t paddingSize = 8 - (strtab.sectionSize % 8);
				fwrite(&paddingZero, 1, paddingSize, dataFile);
			}
			
			if (error == 0) {
				elementsWritten = fwrite(&symbol0, 1, symtab.sectionEntrySize, dataFile);
				if (elementsWritten != symtab.sectionEntrySize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			fpos_t stringsDataPos;
			fgetpos(dataFile, &stringsDataPos);
			
			if (error == 0) {
				elementsWritten = fwrite(&stringsData, 1, symtab.sectionEntrySize, dataFile);
				if (elementsWritten != symtab.sectionEntrySize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			if (error == 0) {
				elementsWritten = fwrite(&stringsIndicies, 1, symtab.sectionEntrySize, dataFile);
				if (elementsWritten != symtab.sectionEntrySize) {
					printf("File Write Failed!\n");
					error = -2;
				}
			}
			
			uint32_t numberOfLines = 2;
			size_t elementsRead = 0;
			do {
				elementsRead = fread(readCharacters, 1, NUM_CHARACTERS, txtFile);
				
				for (size_t c=0; c<elementsRead; c++) {
					if (readCharacters[c] == 0x0A) {
						numberOfLines++;
						readCharacters[c] = 0;
					}
				}
				
				elementsWritten = fwrite(readCharacters, 1, elementsRead, dataFile);
				stringsData.symbolSize += elementsWritten;
			
			} while (elementsRead >= NUM_CHARACTERS);
			
			//fclose(txtFile);
			
			size_t paddingSize = 8 - (stringsData.symbolSize % 8);
			fwrite(&paddingZero, 1, paddingSize, dataFile);
			
			
			printf("Number of Lines: %d\n", numberOfLines);
			
			stringsIndicies.symbolValue = stringsData.symbolSize + paddingSize;
			stringsIndicies.symbolSize = numberOfLines * sizeof(uint32_t);
			
			rodata.sectionSize = stringsIndicies.symbolValue + stringsIndicies.symbolSize;
			
			if ((numberOfLines & 1) == 1) {
				rodata.sectionSize += sizeof(uint32_t);
			}
			
			
			fsetpos(dataFile, &rodataPos);
			fwrite(&rodata, 1, head.sectionHeaderSize, dataFile);
			
			
			
			fsetpos(dataFile, &stringsDataPos);
			fwrite(&stringsData, 1, symtab.sectionEntrySize, dataFile);
			fwrite(&stringsIndicies, 1, symtab.sectionEntrySize, dataFile);
			
			
			fseek(txtFile, 0, SEEK_SET);
			fseek(dataFile, stringsIndicies.symbolValue, SEEK_CUR);
			
			uint32_t* stringIndData = malloc(stringsIndicies.symbolSize);
			
			stringIndData[0] = numberOfLines - 2;
			
			uint32_t* strIndDataPtr = stringIndData;
			strIndDataPtr++;
			*strIndDataPtr = 0;
			strIndDataPtr++;
			uint32_t byteOffset = 1;
			do {
				elementsRead = fread(readCharacters, 1, NUM_CHARACTERS, txtFile);
				
				//printf("%.*s\n", elementsRead, readCharacters);
				
				for (uint32_t c=0; c<elementsRead; c++) {
					if (readCharacters[c] == 0x0A) {
						
						*strIndDataPtr = byteOffset + c;
						strIndDataPtr++;
					}
				}
				
				byteOffset += elementsRead;
			
			} while (elementsRead >= NUM_CHARACTERS);			
			
			fwrite(stringIndData, sizeof(uint32_t), numberOfLines, dataFile);
			if ((numberOfLines & 1) == 1) {
				fwrite(&paddingZero, 1, sizeof(uint32_t), dataFile);
			}
			
			
			free(stringIndData);
			
			fclose(dataFile);
			printf("Strings data saved to file: %s\n", outFileLocation);
		}
	
		fclose(txtFile);
	}
	
	printf("Program Ended\n");
  return error;
}