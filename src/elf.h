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


//ELF Structures Header for x64 architectures
#ifndef MEDIA_ENHANCED_ELF_H
#define MEDIA_ENHANCED_ELF_H

#include <stdint.h>	//Defines Data Types: https://en.wikipedia.org/wiki/C_data_types

//Executable and Linkable Format Structures
struct elfHeader64 { //ELF File Header (always at beginning of file)
	uint32_t magicNumberID; //"Magic Number" File ID Code = 0x7F,"ELF"
	uint8_t bitFormat; //Should be 2 to indicate 64-bit format
	uint8_t endiannessFormat; //Most likely should be 1 to indicate little endianness format
	uint8_t elfVersion; //Should be 1
	uint8_t targetABI; //Most likely should be 0
	uint64_t versionABI; //Only first byte, then 0 padding... most likely should be 0
	uint16_t objectType; //Object file type... 1 for relocatable file in this case
	uint16_t architectureTarget; //Target computer architecture... 0x3E for x86-64 in this case
	uint32_t elfVersion2; //Should be 1
	uint64_t entryPoint; //Executable Entry Point... 0 for no entry point in this case (no program header)
	uint64_t programHeaderOffset; //Program Header Offset from beginning of file... 0 for no program header in this case
	uint64_t sectionHeaderOffset; //Section Header Offset from beginning of file... right after the ELF Header in this case (0x40)
	uint32_t architectureFlags; //Flags for target architecture... 0 in this case
	uint16_t headerSize; //Size of this header (0x40)
	uint16_t programHeaderSize; //Size of a Program Header... 0 in this case
	uint16_t programHeaderEntries; //Number of Program Header Entries... 0 in this case
	uint16_t sectionHeaderSize; //Size of a Section Header... 0x40 in this case
	uint16_t sectionHeaderEntries; //Number of Section Header Entries
	uint16_t sectionHeaderNames; //Section Header Entry that containes the section names
};

struct elfSectionHeader64 { //ELF Section Header
	uint32_t nameOffset; //Offset to null terminated ASCII string in ".shstrtab" section
	uint32_t sectionType; //Section Type (Refer to table of possibilities)
	uint64_t sectionFlags; //Section Flags (Refer to table of possible combinations)
	uint64_t virtualAddress; //Virtual address of the section in memory, for sections that are loaded...?
	uint64_t sectionOffset; //Section offset from beginning of file
	uint64_t sectionSize; //Section size in bytes
	uint32_t sectionLink; //Section Header Index of an associated section, depends on the section type
	uint32_t sectionInfo; //Contains extra information about the section...?
	uint64_t sectionAlignment; //Required Alignment of section (only applies if allocated...?)
	uint64_t sectionEntrySize; //Contains the byte size of each entry, for sections that contain fixed-size entries... Otherwise is 0 
};


struct elfSymbolTableEntry64 {
	uint32_t nameOffset; //Offset to null terminated ASCII string in ".strtab" section
	uint8_t symbolInfo; //Symbol binding and type... used by ld when creating executable
	uint8_t symbolVisibility; //Symbol Visibility... runtime symbol visibility...?
	uint16_t sectionIndex; //Related Section Index... could be a special value like 0xFFF1 which specifies that the symbol value is a constant
	uint64_t symbolValue;
	uint64_t symbolSize;
};

#endif //MEDIA_ENHANCED_ELF_H
