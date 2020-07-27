#include "RAWYUVProtocolReader.h"
#include <libyuv.h>



const char* RAWYUVProtocolReader::RAWYUVProtocolName = "RAWYUV420";

std::shared_ptr<Frame> RAWYUVProtocolReader::ParseFrame(const unsigned char* data, size_t dataLength, std::chrono::microseconds& timestamp)
{
	//timestamp = std::chrono::system_clock::now();

	using namespace libyuv;
	const unsigned int colorWidthHalf = colorFrameWidth >> 1;
	const unsigned int colorHeightHalf = colorFrameHeight >> 1;

	const unsigned char* frameY = data;
	const unsigned char* frameU = data + ((uint64_t) colorFrameWidth * colorFrameHeight);
	const unsigned char* frameV = frameU + ((uint64_t)colorWidthHalf * colorHeightHalf);

	// creates BGRA frame
	std::shared_ptr<Frame> rgbFrame = Frame::Create(colorFrameWidth, colorFrameHeight, FrameType::Encoding::RGBA32);

	I420ToRGBA((const uint8_t*)frameY, colorFrameWidth,
		(const uint8_t*)frameU, colorWidthHalf,
		(const uint8_t*)frameV, colorWidthHalf,
		rgbFrame->getData(), colorFrameWidth, colorFrameWidth, colorFrameHeight);

	return rgbFrame;
}