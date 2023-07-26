#version 460
//https://www.khronos.org/opengl/wiki/Compute_Shader for reference

layout(local_size_x = 16, local_size_y = 4, local_size_z = 1) in; //64 multiple size

layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D inputImage;
//layout(set = 0, binding = 0, rgba8) uniform readonly image2D inputImage;

layout(set = 0, binding = 1, std430) buffer lutBufBlock{uint lut[];} lutData;

layout(set = 0, binding = 2, r16ui) uniform writeonly uimage2D outputImage;


void main() {
	ivec2 imgLocation = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y);
	ivec2 imgSize = imageSize(inputImage);
	
	uvec4 uintValue = imageLoad(inputImage, imgLocation);
	//vec4 loadValue = imageLoad(inputImage, imgLocation);
	//uvec4 uintValue = uvec4(loadValue * 255.0);
	int imgHeight2 = imgSize.y << 1;
	ivec2 imgLocation2 = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y + imgSize.y);
	
	//uint rValue = uintValue.x << 16; //Red Value
	//uint gValue = uintValue.y <<  8; //Green Value
	ivec2 imgLocation3 = ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y + imgHeight2);
	
	uint rgbValue = uintValue.x & 0xFFFFFF;
	//uint rgbValue = rValue | gValue | uintValue.z;
	uint yuvValue = lutData.lut[rgbValue];
	
	uint yValue = yuvValue & 0x3FF00000;
	uint uValue = yuvValue & 0xFFC00;
	uint vValue = yuvValue & 0x3FF;
	
	uvec4 storeValue = uvec4(yValue >> 14, 0, 0, 0);
	uvec4 storeValue2 = uvec4(uValue >> 4, 0, 0, 0);
	uvec4 storeValue3 = uvec4(vValue << 6, 0, 0, 0);
	
	imageStore(outputImage, imgLocation, storeValue);
	imageStore(outputImage, imgLocation2, storeValue2);
	imageStore(outputImage, imgLocation3, storeValue3);
}