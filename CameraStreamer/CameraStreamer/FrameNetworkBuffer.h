#pragma once

#include <memory>

/*
 Future work: find a way of accepting differnt types of networkbuffers :)

#include "NetworkBuffer.h"
#include "Frame.h"

namespace comms
{
	// CameraStreamer extension of NetworkBufferPtr to wrap a frame
	class FrameNetworkBuffer : public NetworkBufferPtr
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
		virtual unsigned char* Data() const
		{
			return frame->getData();
		}

	};

}*/