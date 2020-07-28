#pragma once
#include "ProtocolPacketReader.h"

#include <memory>

class RAWYUVProtocolReader : public ProtocolPacketReader
{
private:
	static const char* RAWYUVProtocolName;

protected:

public:

	static std::shared_ptr<ProtocolPacketReader> Create()
	{
		return std::shared_ptr<ProtocolPacketReader>(new RAWYUVProtocolReader());
	}

	virtual bool HasFixedHeaderSize() const { return true; }
	virtual size_t FixedHeaderSize() const { return sizeof(uint32_t) * 3; }

	virtual bool ParseHeader(const unsigned char* header, size_t headerLength)
	{
		// sanity check
		if (headerLength < FixedHeaderSize()) return false;

		// no timestamp info is available for this protocol
		lastFrameTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

		// how much should be read next
		networkFrameSize = ((const uint32_t*)header)[0];		 // first integer is the entire frame size
		networkFrameSize -= sizeof(uint32_t) * 2;				 // frameSize included the number of bytes taken for colorWidth and colorHeight

		// resolution of the next frame available
		colorFrameWidth = ((const uint32_t*)header)[1];			 // second integer is the width
		colorFrameHeight = ((const uint32_t*)header)[2];		 // third integer is the height

		return true;
	}

	
	// yuv protocol doesn't  support depth at all. it can always return false
	virtual bool supportsDepth() const { return false; }

	// yuv protocol always supports color, so it can always return true
	virtual bool supportsColor() const { return true;  }

	virtual bool ParseFrame(const unsigned char* data, size_t dataLength);

	virtual const std::string ProtocolName() const { return RAWYUVProtocolName; }
};

