#pragma once
#include "ProtocolPacketReader.h"

#include <memory>

class RAWYUVProtocolReader : public ProtocolPacketReader
{
private:
	static const char* RAWYUVProtocolName;

protected:

public:

	std::shared_ptr<ProtocolPacketReader> Create()
	{
		return new RAWYUVProtocolReader();
	}

	virtual bool HasFixedHeaderSize() const { return true; }
	virtual size_t FixedHeaderSize() const { return sizeof(uint32_t) * 3; }

	virtual bool ParseHeader(const unsigned char* header, size_t headerLength, size_t& frameSize)
	{
		// sanity check
		if (headerLength < FixedHeaderSize()) return false;

		frameSize = ((const uint32_t*)header)[0];		 // first integer is the entire frame size
		colorFrameWidth = ((const uint32_t*)header)[1];  // second integer is the width
		colorFrameHeight = ((const uint32_t*)header)[2]; // third integer is the height

		frameSize -= sizeof(uint32_t) * 2;				 // frameSize included the number of bytes taken for colorWidth and colorHeight
		return true;
	}

	
	// yuv protocol doesn't  support depth at all. it can always return false
	virtual bool supportsDepth() const { return false; }

	// yuv protocol always supports color, so it can always return true
	virtual bool supportsColor() const { return true;  }

	virtual std::shared_ptr<Frame> ParseFrame(const unsigned char* data, size_t dataLength, std::chrono::microseconds& timestamp);

	virtual const std::string ProtocolName() const { return RAWYUVProtocolName; }
};

