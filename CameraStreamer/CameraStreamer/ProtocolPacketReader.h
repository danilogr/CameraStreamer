#pragma once

#include "Frame.h"

#include <memory>

/**
	ProtocolPacketReader classes are responsible for creating a network packet
	given a specific protocol implementation
*/

class ProtocolPacketReader : std::enable_shared_from_this<ProtocolPacketReader>
{
protected:
	bool initialized;
	bool colorFrameAvailable;
	unsigned int colorFrameWidth, colorFrameHeight;

	bool depthFrameAvailable;
	unsigned int depthFrameWidth, depthFrameHeight;

	size_t networkFrameSize;

	ProtocolPacketReader() : initialized(false), colorFrameAvailable(false),
		colorFrameWidth(0), colorFrameHeight(0), depthFrameAvailable(0),
		depthFrameWidth(0), depthFrameHeight(0), networkFrameSize(0) {}

public:

	inline size_t getNetworkFrameSize() { return networkFrameSize; }
	inline bool isColorFrameAvailable() { return colorFrameAvailable; }
	inline bool isDepthFrameAvailable() { return depthFrameAvailable; }
	inline unsigned int getDepthFrameHeight() { return depthFrameHeight; }
	inline unsigned int getDepthFrameWidth() { return depthFrameWidth; }
	inline unsigned int getColorFrameWidth() { return colorFrameWidth; }
	inline unsigned int getColorFrameHeight() { return colorFrameHeight; }

	/// <summary>
	/// Does this protocol use a fixed header size in bytes?
	/// </summary>
	/// <returns>true if network header has a fixed size</returns>
	virtual bool HasFixedHeaderSize() const = 0;

	/// <summary>
	/// Network packet header size in bytes
	/// </summary>
	/// <returns>header length in bytes</returns>
	virtual size_t FixedHeaderSize() const = 0;

	/// <summary>
	/// Parses header given a pointer to it. 
	/// </summary>
	/// <param name="header">header buffer pointer</param>
	/// <param name="headerLength">header buffer length in bytes</param>
	/// <param name="frameSize">(out) size in bytes of the frame encoded by this header</param>
	/// <returns>True if the header was parsed successfully</returns>
	virtual bool ParseHeader(const unsigned char* header, size_t headerLength, size_t& frameSize) = 0;
	
	/// <summary>
	/// Does this protocol support depth frames?
	/// </summary>
	/// <returns>true if this protocol supports depth frames</returns>
	virtual bool supportsDepth() const = 0;

	/// <summary>
	/// Does this protocol support color frames?
	/// </summary>
	/// <returns>true if this protocol supports color frames</returns>
	virtual bool supportsColor() const = 0;

	/// <summary>
	/// Parses a buffer and returns a frame.
	/// </summary>
	/// <param name="data">pointer to a buffer</param>
	/// <param name="dataLength">bytes to read from data</param>
	/// <param name="timestamp">Timestamp for this frame (or current time if not availble)</param>
	/// <returns>A valid Frame if everything went well. nullptr otherwise</returns>
	virtual std::shared_ptr<Frame> ParseFrame(const unsigned char* data, size_t dataLength, std::chrono::microseconds &timestamp) = 0;


	/// <summary>
	/// Returns a human readable string with the protocol name
	/// </summary>
	/// <returns></returns>
	virtual const std::string ProtocolName() const = 0;

};

