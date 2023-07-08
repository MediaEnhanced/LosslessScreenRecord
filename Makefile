default_target: WindowsExecutables

./bin/:
	cmd /c mkdir .\bin

./bin/obj/:
	cmd /c mkdir .\bin\obj

./bin/lib/:
	cmd /c mkdir .\bin\lib

./bin/spv/:
	cmd /c mkdir .\bin\spv

CompilerArguments = -std=c11 -O2 -m64 -mconsole -mabi=ms
  # -ffreestanding
CompilerWarnings = -Wall
 # -Wextra -Wwrite-strings -pedantic -g3 More Warnings in the future
CompatibilityHeaders = ./src/compatibility.h ./src/helperFunctions.h

./bin/obj/compatibility.o: ./src/compatibilityWin32.c $(CompatibilityHeaders) | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/compatibility.o ./src/compatibilityWin32.c

./bin/obj/helperFunctions.o: ./src/helperFunctions.asm | ./bin/obj/
	FASM ./src/helperFunctions.asm ./bin/obj/helperFunctions.o

./bin/obj/log2.o: ./src/log2.c | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/log2.o ./src/log2.c
	
./bin/obj/exp2.o: ./src/exp2.c | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/exp2.o ./src/exp2.c

./bin/lib/math.a: ./bin/obj/log2.o ./bin/obj/exp2.o | ./bin/lib/
	ar cr ./bin/lib/math.a ./bin/obj/log2.o ./bin/obj/exp2.o

./bin/obj/main.o: ./src/main.c $(CompatibilityHeaders) | ./bin/obj/
	gcc $(CompilerArguments) $(CompilerWarnings) -c -o ./bin/obj/main.o ./src/main.c

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

./bin/FixExecutableCudaDLL.exe: ./src/fixExecutableCudaDll.c | ./bin
	gcc $(CompilerArguments) $(CompilerWarnings) -s -o ./bin/FixExecutableCudaDLL.exe ./src/fixExecutableCudaDll.c

./bin/CheckLosslessSRGBtoYUV.exe: ./src/checkLosslessSRGBtoYUV.c ./src/helperFunctions.h ./bin/obj/helperFunctions.o
	gcc $(CompilerArguments) $(CompilerWarnings) -s -o ./bin/CheckLosslessSRGBtoYUV.exe ./src/checkLosslessSRGBtoYUV.c ./bin/obj/helperFunctions.o

CheckLossless: ./bin/CheckLosslessSRGBtoYUV.exe
	./bin/CheckLosslessSRGBtoYUV.exe

LocalLibraryDirectory = -L./bin/lib/
LocalLibraries = -l:math.a

WindowsLibraryDirectory = -LC:/Windows/System32
Libraries = -l:kernel32.dll -l:dxgi.dll -l:d3d11.dll -l:vulkan-1.dll -l:nvCuda.dll -l:nvEncodeAPI64.dll -l:ws2_32.dll
 # -l:gdi32.dll
 # ws2_32 is used for windows networking (sockets)
 # -lgdi32 -ld3d11 is used for windows desktop duplication

LinkerLibraries = $(LocalLibraryDirectory) $(LocalLibraries) $(WindowsLibraryDirectory) $(Libraries)

Mingw64LibraryDirectories = -LC:/mingw64/x86_64-w64-mingw32/lib -LC:/mingw64/lib
Mingw64Libraries = -ldxguid -lmingwex
 # -ldxguid is needed for the definitions of the GUID (used for windows desktop duplication under DirectX 11
 # -lmingwex needed for log2.c fma function "calls"

gccLibraryDirectory = -LC:/mingw64/lib/gcc/x86_64-w64-mingw32/13.1.0
gccLibraries = -lgcc
 #needed for gcc automatic windows function stack correction (when a function uses more than 4096 bytes / a page for the stack)

TempLibraries = $(Mingw64LibraryDirectories) $(Mingw64Libraries) $(gccLibraryDirectory) $(gccLibraries)

./bin/LosslessScreenRecord.exe: ./bin/obj/compatibility.o ./bin/obj/helperFunctions.o ./bin/lib/math.a ./bin/obj/main.o ./bin/obj/stringsData.o ./bin/obj/binData.o ./bin/obj/win32Resource.o ./bin/FixExecutableCudaDLL.exe
	ld -o ./bin/LosslessScreenRecord.exe -estartP -s -static --gc-sections \
	./bin/obj/compatibility.o ./bin/obj/helperFunctions.o ./bin/lib/math.a ./bin/obj/main.o ./bin/obj/stringsData.o ./bin/obj/binData.o ./bin/obj/win32Resource.o \
	$(LinkerLibraries) $(TempLibraries)
	./bin/FixExecutableCudaDLL.exe ./bin/LosslessScreenRecord.exe
 #Correct the improper nvCuda.dll naming in the final executable

WindowsExecutables: ./bin/LosslessScreenRecord.exe

WindowsClean:
	cmd /c rmdir /s /q .\bin


