#pragma once

#include <memory>
#include <vector>

#include "NetworkBuffer.h"

class FrameNetworkBuffer : NetworkBuffer
{

protected:

	std::shared_ptr<std::vector<unsigned char>> buffer;

public:

	FrameNetworkBuffer(std::shared_ptr<std::vector<unsigned char>> buffer) : buffer(buffer) {}

	// whether or not the buffer is still valid
	virtual bool Allocated()
	{
		return buffer != nullptr;
	}

	// the size of the buffer (the amount of bytes read / to write)
	virtual size_t Size()
	{
		return buffer->size();
	}

	// pointer to the implementation specific buffer
	virtual const unsigned char* Data() const
	{
		return &((*buffer)[0]);
	}

};