#pragma once

#include <memory>

#include "NetworkBuffer.h"
#include "Frame.h"



class FrameNetworkBuffer : NetworkBuffer
{

protected:

	std::shared_ptr<Frame> frame;

public:

	FrameNetworkBuffer(std::shared_ptr<Frame> frame) : frame(frame) {}

	// whether or not the buffer is still valid
	virtual bool Allocated()
	{
		return frame != nullptr;
	}

	// the size of the buffer (the amount of bytes read / to write)
	virtual size_t Size()
	{
		return frame->size();
	}

	// pointer to the implementation specific buffer
	virtual const unsigned char* Data() const
	{
		return frame->getData();
	}

};