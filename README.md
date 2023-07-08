# Lossless Screen Record
&emsp;&emsp;This Lossless Screen Record program is designed to capture and save an exact copy of the monitor image data over a duration of 60 seconds. It currently targets modern (64-bit) Windows 10/11 computers with a recent discrete Nvidia graphics card and utilizes [Windows Desktop Duplication](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api) alongside [Nvidia NVENC](https://developer.nvidia.com/video-codec-sdk) to capture the screen. The display is captured at 60 times a second (60 fps) and is encoded with [HEVC (H.265)](https://en.wikipedia.org/wiki/High_Efficiency_Video_Coding) to a minimal bitstream file that can be processed, analyzed, and converted later. When recording a sRGB monitor source, which is the typical case, the data saved is 100% lossless due to the [BT.709](https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.709_conversion) conversion from the 8 bits per channel (bpc) sRGB color space to the 10 bpc YCbCr color space. No [chroma subsampling](https://en.wikipedia.org/wiki/Chroma_subsampling) is applied (4:4:4) during this conversion and the created bitstream can be used to generate the original sRGB images.

&emsp;&emsp;This program will only run with the primary desktop monitor being connected to a Nvidia GPU that supports the [H.265 Lossless / 10-bit](https://developer.nvidia.com/video-encode-and-decode-gpu-support-matrix-new) modes. The 60 second, 60 fps recordings can easily be 6 GB+ in size depending on the monitor resolution and screen content. For example, a capture of basic text editing will have a much smaller resulting file size than a capture of a video game since most of the image information is the same from one frame to the next. The recorded bitstream file can be converted by [FFMPEG](https://ffmpeg.org/) into a lossy format of choice so that it can be watched easily in a program like [VLC](https://www.videolan.org/vlc/). A very flexible MIT license was used for this project and the code can serve as an example of how to connect the Direct X interface used for the Windows Desktop Duplication to Vulkan used for color space conversion to Nvidia CUDA / NVENC used for encoding. The lossless recordings generated from this project are intended to be used in future open projects where a comparision of different lossy encodings of the same source content requires an original master baseline.
  
&nbsp;

## Program Features
* Simple 60 second sRGB Lossless Screen Capture at 60 fps
* Real-time (low-latency) HEVC Encoding
* Small Portable Executable
* Simple to Compile and Modify
* Nice Example of Direct X, Vulkan, and CUDA / NVENC Interoperability
  
## Program Requirements
* 1 GB of free RAM
* 500 MB of free GPU VRAM
* 20GB+ of free SSD storage 
* 64-bit CPU capable of AVX2 / FMA SIMD instructions
* Updated Windows 10 / 11
* 6th Generation or Newer Nvidia Discrete Graphics Card with latest drivers
* Primary Monitor Connected to Nvidia GPU

## How To Use It
1. Download the latest executable from the [releases](https://github.com/MediaEnhanced/LosslessScreenRecord/releases)
2. Run the executable and a console window should appear with basic information
3. Press Enter to start the record when prompted
4. After the 60 second period a "bitstream.h265" file will be created in the same folder as the executable
5. *Optionally use ffmpeg to convert the bitstream into a lossy viewable video by using a command like:

 ```ffmpeg -i bitstream.h265 -c:v libx264 -preset veryfast -crf 22 -pix_fmt yuv420p -color_range 2 lossyVersion.mp4```

&nbsp;

## How to Provide Feedback
### [I Want To Hear From You!](https://github.com/MediaEnhanced/LosslessScreenRecord/issues)
&emsp;&emsp;I am actively developing this software tool and want to know how it can be modified and improved to meet other use cases and requirements besides my intended future projects! This can be as simple as letting me know that I should provide more documentation information and better explain certain details of the project or as challenging as asking for me to add support for linux and other graphics card brands. Either way please raise a [GitHub issue](https://github.com/MediaEnhanced/LosslessScreenRecord/issues/new/choose) and fill out the relevant fourm. I look forward to ideas I have not even considered!  

&emsp;&emsp;You should submit a Bug Report if the software does not work properly as stated yet the running computer meets all the requirements. This program tool does not do too much on its own but I want to make sure what it does is functional for all users. If there is something I have overlooked in the design or something else I should be made aware of please create a Discussion post. While I want to keep as much information about this project as public as possible, I can be reached more directly and privately at my email: Jared.Loewenthal@proton.me

&nbsp;

## How to Compile It
The following software needs to be downloaded and "installed" (at least added to the PATH):
* MinGW-w64 for gcc and ld that can create Windows Executables
* FASM for assembling the helper assembly functions
* Vulkan SDK for compiling the GLSL Shader code with glslangValidator

Run mingw32-make.exe (from MinGW-w64) in the directory with the Makefile to generate a few executables into the bin sub-directory including the one found on the releases page

