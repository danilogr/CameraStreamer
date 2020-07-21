#pragma once

/*
 * NetworkBuffer is a buffer wrapper we use to make sure
 * that memory is only freed when it is not necessary anymore.
 *
 * This class is only used to wrap other implementation specific
 * memory allocation types such as Frame.
 *
 * author: gasques@ucsd.edu
 */
class NetworkBuffer
{
public:
	// whether or not the buffer is still valid
	virtual bool Allocated() = 0;

	// the size of the buffer (the amount of bytes read / to write)
	virtual size_t Size() = 0;

	// pointer to the implementation specific buffer
	virtual const unsigned char* Data() const = 0;

};