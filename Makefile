#MIT License
#Copyright (c) 2023 Jared Loewenthal
#
#Permission is hereby granted, free of charge, to any person obtaining a copy
#of this software and associated documentation files (the "Software"), to deal
#in the Software without restriction, including without limitation the rights
#to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#copies of the Software, and to permit persons to whom the Software is
#furnished to do so, subject to the following conditions:
#
#The above copyright notice and this permission notice shall be included in all
#copies or substantial portions of the Software.
#
#THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#SOFTWARE.

#Makefile to create the Windows Lossless Screen Record program executable
#designed to be run by MinGW-w64's make (mingw32-make.exe)
#Can only currently be run on Windows and expects the following programs
#to be on the System PATH:
# MinGW-w64 Binaries (bin folder in downloadable: https://winlibs.com/)
# FASM.exe from flatassembler for Windows: https://flatassembler.net/
# glslangValidator.exe from the Vulkan SDK: https://vulkan.lunarg.com/

#The default target is for Windows 
default_target: WindowsExecutables

#create directories when needed
./bin/:
	cmd /c mkdir .\bin

./bin/obj/:
	cmd /c mkdir .\bin\obj

./bin/spv/:
	cmd /c mkdir .\bin\spv

#Common Compiler (gcc) arguments
CompilerArguments = -std=c11 -O2 -m64 -mconsole -mabi=ms
  # -ffreestanding... look into later
CompilerWarnings = -Wall
 # -Wextra -Wwrite-strings -pedantic -g3 More Warnings in the future

./bin/obj/compatibility.o: ./src/compatibility.c ./src/compatibility.h | ./bin/obj/
	gcc $(CompilerArguments) -O1 $(CompilerWarnings) -c -o ./bin/obj/compatibility.o ./src/compatibility.c
 #-O1 to counter the automatic memset insert until functions get converted to assembly

./bin/obj/compatibilityAssembly.o: ./src/compatibilityAssembly.asm | ./bin/obj/
	FASM ./src/compatibilityAssembly.asm ./bin/obj/compatibilityAssembly.o

./bin/obj/compatibilityWin32.o: ./src/compatibilityWin32.c ./src/compatibility.h ./src/math.h | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/compatibilityWin32.o ./src/compatibilityWin32.c
 #-fstack-usage to output stack usage per function so that it can be adjusted to be lower than 4096 bytes

CompatibilityWindowsObjects = ./bin/obj/compatibilityWin32.o ./bin/obj/compatibility.o ./bin/obj/compatibilityAssembly.o

./bin/obj/math.o: ./src/math.c ./src/math.h | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/math.o ./src/math.c

./bin/obj/mathAssembly.o: ./src/mathAssembly.asm | ./bin/obj/
	FASM ./src/mathAssembly.asm ./bin/obj/mathAssembly.o

./bin/obj/log2.o: ./src/log2.c ./src/math.h | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/log2.o ./src/log2.c
	
./bin/obj/exp2.o: ./src/exp2.c ./src/math.h | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/exp2.o ./src/exp2.c

MathObjects = ./bin/obj/log2.o ./bin/obj/exp2.o ./bin/obj/math.o ./bin/obj/mathAssembly.o 

ProgramEntry = ./src/compatibility.h ./src/programStrings.h ./src/programEntry.h

./bin/obj/main.o: ./src/main.c $(ProgramEntry) ./src/math.h | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/main.o ./src/main.c

./bin/obj/playback.o: ./src/playback.c $(ProgramEntry) | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/playback.o ./src/playback.c

./bin/CreateStringsData.exe: ./src/createStringsData.c ./src/elf.h | ./bin
	gcc $(CompilerArguments) $(CompilerWarnings) -s -o ./bin/CreateStringsData.exe ./src/createStringsData.c

./bin/obj/stringsData.o: ./bin/CreateStringsData.exe ./src/en-us.txt | ./bin/obj/
	./bin/CreateStringsData.exe ./bin/obj/stringsData.o ./src/en-us.txt

./bin/spv/shader.spv: ./src/shader.comp.glsl | ./bin/spv/
	glslangValidator ./src/shader.comp.glsl -V -o ./bin/spv/shader.spv \
	-g0 --target-env vulkan1.1
 #-Os Optimize for size to try

./bin/CreateElfObjectFromFiles.exe: ./src/createElfObjectFromFiles.c ./src/elf.h | ./bin
	gcc $(CompilerArguments) $(CompilerWarnings) -s -o ./bin/CreateElfObjectFromFiles.exe ./src/createElfObjectFromFiles.c

./bin/obj/binData.o: ./bin/CreateElfObjectFromFiles.exe ./bin/spv/shader.spv
	./bin/CreateElfObjectFromFiles.exe ./bin/obj/binData.o ./bin/spv/shader.spv

./bin/obj/win32Resource.o: ./src/win32Resource.rc ./src/win32AppManifest.xml | ./bin/obj/
	windres -o ./bin/obj/win32Resource.o -i ./src/win32Resource.rc -O coff

./bin/CheckLosslessSRGBtoYUV.exe: ./src/checkLosslessSRGBtoYUV.c ./src/math.h ./bin/obj/mathAssembly.o
	gcc $(CompilerArguments) $(CompilerWarnings) -s -o ./bin/CheckLosslessSRGBtoYUV.exe ./src/checkLosslessSRGBtoYUV.c ./bin/obj/mathAssembly.o

CheckLossless: ./bin/CheckLosslessSRGBtoYUV.exe
	./bin/CheckLosslessSRGBtoYUV.exe

LocalLibraryDirectory = -L./lib/
LocalLibraries = -l:vulkan-1.lib -l:cuda.lib -l:nvencodeapi.lib
 # vulkan-1.lib is needed for Vulkan ("Middleman" Compute Pipeline)
 # cuda.lib & nvencodeapi.lib is needed for NVIDIA Encoding

WindowsLibraryDirectory = -LC:/Windows/System32
Libraries = -l:kernel32.dll -l:dxgi.dll -l:d3d11.dll -l:ws2_32.dll
 # kernel32.dll is needed for the basic Window OS API interface
 # dxgi.dll & d3d11.dll is needed for Windows Desktop Duplication
 # ws2_32.dll is needed for Windows networking (sockets)

LinkerLibraries = $(LocalLibraryDirectory) $(LocalLibraries) $(WindowsLibraryDirectory) $(Libraries)

 #gccLibraryDirectory = -LC:/mingw64/lib/gcc/x86_64-w64-mingw32/13.1.0
 #gccLibraries = -lgcc
 #needed for gcc automatic windows function stack correction (when a function uses more than 4096 bytes / a page for the stack)

TempLibraries = $(gccLibraryDirectory) $(gccLibraries)

WindowsLinkingObjects = $(CompatibilityWindowsObjects) $(MathObjects) ./bin/obj/stringsData.o ./bin/obj/win32Resource.o

./bin/LosslessScreenRecord.exe: ./bin/obj/main.o $(WindowsLinkingObjects) ./bin/obj/binData.o
	ld -o ./bin/LosslessScreenRecord.exe -eprogramEntry -s -static --gc-sections \
	./bin/obj/main.o $(WindowsLinkingObjects) ./bin/obj/binData.o \
	$(LinkerLibraries)
 #$(TempLibraries)

./bin/VulkanVideoPlayback.exe: ./bin/obj/playback.o $(WindowsLinkingObjects)
	ld -o ./bin/VulkanVideoPlayback.exe -eprogramEntry -s -static --gc-sections \
	./bin/obj/playback.o $(WindowsLinkingObjects) \
	$(LinkerLibraries)
 #$(TempLibraries)

WindowsExecutables: ./bin/LosslessScreenRecord.exe ./bin/VulkanVideoPlayback.exe

WindowsClean:
	cmd /c rmdir /s /q .\bin


