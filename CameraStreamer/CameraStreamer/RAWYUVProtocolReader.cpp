#include "RAWYUVProtocolReader.h"
#include <libyuv.h>



const char* RAWYUVProtocolReader::RAWYUVProtocolName = "RAWYUV420";

bool RAWYUVProtocolReader::ParseFrame(const unsigned char* data, size_t dataLengthcalc
)
{
	using namespace libyuv;
	const unsigned int colorWidthHalf = colorFrameWidth >> 1;
	const unsigned int colorHeightHalf = colorFrameHeight >> 1;

	const unsigned char* frameY = data;
	const unsigned char* frameU = data + ((uint64_t) colorFrameWidth * colorFrameHeight);
	const unsigned char* frameV = frameU + ((uint64_t)colorWidthHalf * colorHeightHalf);

	// creates BGRA frame
	lastColorFrame = Frame::Create(colorFrameWidth, colorFrameHeight, FrameType::Encoding::BGRA32);

	I420ToBGRA((const uint8_t*)frameY, colorFrameWidth,
		(const uint8_t*)frameU, colorWidthHalf,
		(const uint8_t*)frameV, colorWidthHalf,
		lastColorFrame->getData(), colorFrameWidth, colorFrameWidth, colorFrameHeight);

	return true;
}