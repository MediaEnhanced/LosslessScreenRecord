# Lossless Screen Record
&emsp;&emsp;This Lossless Screen Record program is designed to capture and save an exact copy of the monitor image of a computer over a duration of 60 seconds. It currently targets modern (64-bit) Windows 10/11 computers with a recent discrete Nvidia graphics card by utilizing [Windows Desktop Duplication](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api) alongside [Nvidia NVENC](https://developer.nvidia.com/video-codec-sdk) which allows access to the graphics card special video encoding hardware. The screen is captured 60 times a second (60 fps) and is encoded using [HEVC (h.265)](https://en.wikipedia.org/wiki/High_Efficiency_Video_Coding) which is saved to a basic bitstream file that can be processed and converted later. When recording a sRGB monitor output, the typical case, the data saved is 100% lossless due to the [BT.709](https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion) conversion from the 8 bits per channel (bpc) sRGB color space to the 10 bpc YCbCr color space with no chroma subsampling applied (4:4:4).

&emsp;&emsp;This program will only run with the primary desktop monitor being connected to a Nvidia GPU that supports [H.265 Lossless / 10-bit](https://developer.nvidia.com/video-encode-and-decode-gpu-support-matrix-new), and the 60 second 60 fps recordings can easily be 10GB+ in size depending on monitor resolution and screen content. For example, a capture of text editing will have a much smaller resulting file size than a capture of a video game. The recorded file can be converted by [FFMPEG](https://ffmpeg.org/) into a lossy format of choice so that it can be watched easily by a program like [VLC](https://www.videolan.org/vlc/). The code has a very flexible MIT license and can serve as an example of proper sRGB lossless video encoding and how to connect the Direct X interface used for the Windows desktop duplication to Vulkan used for color space conversion to Nvidia CUDA / NVENC used for encoding. The lossless recordings generated from this project are intended for used in future open projects where a comparision analysis of different encodings of the same source content requires a master baseline.
  
&nbsp;

## Program Features
* 60 second at 60 fps sRGB Lossless Screen Capture
* Real-time HEVC Lossless Encoding
* Small Portable Executable
* Simple to Compile and Modify
* Nice Example of Direct X, Vulkan, and CUDA Interoperability
  
## Program Requirements
* 1 GB of free RAM
* 500 MB of free GPU VRAM
* 20GB+ of free SSD storage 
* 64-bit updated Windows 10 or 11
* 6th Generation or Newer Nvidia Graphics Card with latest drivers
* Monitor to Record Connected to Nvidia GPU
* AVX2 / FMA SIMD Instruction Capable CPU

&nbsp;

## How To Use It
Just double click the executable program downloaded from the releases page.
Read the text in the console window and press Enter when ready to record.
The screen will be recorded for 60 seconds.
Press Enter to quit after reading the ending statistics
A "bitstream.h265" file will be generated in the same folder as the executable
An ffmpeg command can be run to convert this bitstream to a lossy but viewable video such as:
```ffmpeg -i bitstream.h265 -c:v libx264 -preset veryfast -crf 22 -pix_fmt yuv420p -color_range 2 lossyVersion.mp4```  

## How to Provide Feedback
I Want To Hear From You!
I'm actively developing this software and want to know how it can be modified and improved to meet other use cases besides my future intended projects
The best way to get in touch is by filling out a Feature Request or a Bug Report on this GitHub repository. The templates are setup so that you can provide most of the information I need to be able to respond appropriately. 
Alternatively you can always email me for direct support:
Email me: Jared.Loewenthal@proton.me

&nbsp;

## How to Compile It
The following software needs to be downloaded and "installed" (at least added to the PATH):
* MinGW-w64 for gcc and ld that can create Windows Executables
* FASM for assembling the helper assembly functions
* Vulkan SDK for compiling the GLSL Shader code with glslangValidator

Run mingw32-make.exe (from MinGW-w64) in the directory with the Makefile to generate a few executables into the bin sub-directory including the one found on the releases page

