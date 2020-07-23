#pragma once

#include <memory>
#include <vector>

#include "NetworkBuffer.h"

namespace comms
{

	template <class T>
	class VectorNetworkBuffer : NetworkBufferPtr
	{

	protected:

		std::shared_ptr<std::vector<T>> buffer;

	public:

		VectorNetworkBuffer(std::shared_ptr<std::vector<T> buffer) : buffer(buffer) {}

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
		virtual unsigned char* Data() const
		{
			return &((*buffer)[0]);
		}

	};

}