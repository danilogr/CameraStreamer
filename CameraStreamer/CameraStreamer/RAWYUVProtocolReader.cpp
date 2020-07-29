#include "RAWYUVProtocolReader.h"
#include <libyuv.h>



const char* RAWYUVProtocolReader::RAWYUVProtocolName = "RAWYUV420";

bool RAWYUVProtocolReader::ParseFrame(const unsigned char* data, size_t dataLengthcalc)
{
	using namespace libyuv;
	const unsigned int colorWidthHalf = colorFrameWidth >> 1;
	const unsigned int colorHeightHalf = colorFrameHeight >> 1;
	const uint64_t pixelsCount = (uint64_t) colorFrameWidth * (uint64_t) colorFrameHeight;
	const uint64_t pixelsCountARGB = pixelsCount << 2;
	const uint64_t pixelsCount4 = pixelsCount >> 2;

	const unsigned char* frameY = data;
	const unsigned char* frameU = data + pixelsCount;
	const unsigned char* frameV = frameU + pixelsCount4;

	// makes sure that I4202BGRA will not crash
	if ((pixelsCount + pixelsCount4 + pixelsCount4) != dataLengthcalc)
		return false;

	// creates BGRA frame
	lastColorFrame = Frame::Create(colorFrameWidth, colorFrameHeight, FrameType::Encoding::ARGB32);

	// makes sure that we have enough memory to write a frame
	if (lastColorFrame && lastColorFrame->size() == pixelsCountARGB)
	{
		I420ToARGB((const uint8_t*)frameY, colorFrameWidth,
			(const uint8_t*)frameU, colorWidthHalf,
			(const uint8_t*)frameV, colorWidthHalf,
			lastColorFrame->getData(), colorFrameWidth << 2,
			colorFrameWidth, colorFrameHeight);

		return true;
	}

	return false;
}