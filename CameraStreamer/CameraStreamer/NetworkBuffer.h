#pragma once

namespace comms
{
	/*
	 * NetworkBufferPtr is a buffer wrapper we use to make sure
	 * that memory is only freed when it is not necessary anymore.
	 *
	 * This class is only used to wrap other implementation specific
	 * memory allocation types such as Frame.
	 *
	 * Classes inheriting from NetworkBufferPtr should limit their memory footprint to a single pointer
	 *
	 * Our interface uses it directly as each NetworkBufferPtr + shared_ptr (in the 
	 * implementation specific case) only uses 16 bytes - 8 bytes for vtable and 8 bytes for the shared_ptr
	 *
	 * author: gasques@ucsd.edu
	 */
	class NetworkBufferPtr
	{
	public:
		// whether or not the buffer is still valid
		virtual bool Allocated() { return false; }

		// the size of the buffer (the amount of bytes read / to write)
		virtual size_t Size() { return 0;  }

		// pointer to the implementation specific buffer
		virtual unsigned char* Data() { return nullptr;  }

	};

}