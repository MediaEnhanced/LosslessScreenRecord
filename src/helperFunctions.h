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


//Media Enhanced x64 Helper Functions
#ifndef MEDIA_ENHANCED_HELPER_FUNCTIONS_H
#define MEDIA_ENHANCED_HELPER_FUNCTIONS_H

#include <stdint.h> //Defines Data Types

#define CALLING_CONVENTION __attribute__((ms_abi))

// Necessary Fast Helper (Assembly) Functions:
uint64_t CALLING_CONVENTION numToFHexStr(uint64_t number, char* strPtr);
uint64_t CALLING_CONVENTION numToPHexStr(uint64_t number, char* strPtr);
uint64_t CALLING_CONVENTION numToUDecStr(char* strPtr, uint64_t number);
//uint64_t CALLING_CONVENTION numToSDecStr(uint64_t number, char* strPtr);
uint64_t CALLING_CONVENTION shortToDecStr(char* strPtr, uint64_t number);
int32_t CALLING_CONVENTION roundDouble(double value);

#endif //MEDIA_ENHANCED_HELPER_FUNCTIONS_H
