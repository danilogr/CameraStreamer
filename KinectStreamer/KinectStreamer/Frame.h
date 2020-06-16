#pragma once

#include <cstdint>
#include <memory>
#include <boost/noncopyable.hpp>
#include <boost/pool/singleton_pool.hpp>

struct FrameType
{
	enum class Encoding : unsigned char
	{
		Mono8,
		Mono16,
		ARGB32,
		RGB24,
		RGBA32,
		BGRA32,
		BGR24,
		//YUY2, // class needs to be refactored to support YUV2 (see getPixelLen and getStride)
		//I420,
		Custom

	};

	static unsigned int getPixelLen(Encoding e)
	{
		switch (e) {
		case Encoding::Mono8:
			return sizeof(unsigned char);
			break;
		case Encoding::Mono16:
			return sizeof(uint16_t);
			break;
		case Encoding::RGB24:
		case Encoding::BGR24:
			return 3;
			break;
		case Encoding::RGBA32:
		case Encoding::BGRA32:
			return 4;
			break;

		default:
			return 0;
			break;
		}
	}


};


#define MemoryPoolDefinition(name, width, height, eltype, elcount) \
struct Resolution##name##_FrameDim { constexpr static unsigned int Size() { return (width*height*sizeof(eltype)*elcount);}}; \
typedef boost::singleton_pool<Resolution##name##_FrameDim, Resolution##name##_FrameDim::Size()> MemoryPool##name;

#define MemoryPoolAlloc(name) MemoryPool##name##::malloc()
#define MemoryPoolFree(name, pointer) MemoryPool##name##::free(pointer)

#define CaseAllocMem(name, data) case Resolution##name##_FrameDim::Size(): data = (unsigned char *) MemoryPoolAlloc(name); break;
#define CaseFreeMem(name, data) case Resolution##name##_FrameDim::Size(): MemoryPoolFree(name, data); break;


// k4a specific definitions
MemoryPoolDefinition(K4ADepth, 640, 576, uint16_t, 1);
MemoryPoolDefinition(K4ADepthWithColor, 640, 576, uint8_t, 4);

MemoryPoolDefinition(K4APassiveInfrared, 1024, 1024, uint16_t, 1);
MemoryPoolDefinition(K4APassiveInfraredWithColor, 1024, 1024, uint8_t, 4);

// rs2 specific definitions
MemoryPoolDefinition(RS2_WVGADepth, 848, 480, uint16_t, 1);
MemoryPoolDefinition(RS2_WVGADepthWithColor, 848, 480, uint8_t, 3);

// ultrasound specific resolutions
MemoryPoolDefinition(VGABW, 640, 480, uint8_t, 1);
MemoryPoolDefinition(SVGABW, 800, 600, uint8_t, 1);

// generic resolutions
MemoryPoolDefinition(VGA, 640, 480, uint8_t, 3);
MemoryPoolDefinition(VGADepth, 640, 480, uint16_t, 1)

MemoryPoolDefinition(SVGA, 800, 600, uint8_t, 3);
MemoryPoolDefinition(SVGADepth, 800, 600, uint16_t, 1)

MemoryPoolDefinition(720P, 1280, 720, uint8_t, 3);
MemoryPoolDefinition(720PRGBA, 1280, 720, uint8_t, 4);
MemoryPoolDefinition(720PDepth, 1280, 720, uint16_t, 1);

MemoryPoolDefinition(1080P, 1920, 1080, uint8_t, 3);
MemoryPoolDefinition(1080PRGBA, 1920, 1080, uint8_t, 4);
MemoryPoolDefinition(1080PDepth, 1920, 1080, uint16_t, 1);

MemoryPoolDefinition(1440P, 2560, 1440, uint8_t, 3);
MemoryPoolDefinition(1440PRGBA, 2560, 1440, uint8_t, 4);
MemoryPoolDefinition(1440PDepth, 2560, 1440, uint16_t, 1);

MemoryPoolDefinition(1536P, 2048, 1536, uint8_t, 3);
MemoryPoolDefinition(1536PRGBA, 2048, 1536, uint8_t, 4);
MemoryPoolDefinition(1536PDepth, 2048, 1536, uint16_t, 1);

MemoryPoolDefinition(2160P, 3840, 2160, uint8_t, 3);
MemoryPoolDefinition(2160PRGBA, 3840, 2160, uint8_t, 4);
MemoryPoolDefinition(2160PDepth, 3840, 2160, uint16_t, 1);

MemoryPoolDefinition(3072P, 4096, 3072, uint8_t, 3);
MemoryPoolDefinition(3072PRGBA, 4096, 3072, uint8_t, 4);
MemoryPoolDefinition(3072PDepth, 4096, 3072, uint16_t, 1);


class Frame : boost::noncopyable
{
protected:
	Frame(unsigned long width, unsigned long height, FrameType::Encoding encoding) :
		customDataAlloc(false), width(width), height(height), customSize(0), usingCustomSize(false), encoding(encoding)
	{
		switch (size())
		{
			// k4a specific
			CaseAllocMem(K4ADepth, data);
			CaseAllocMem(K4ADepthWithColor, data);
			CaseAllocMem(K4APassiveInfrared, data);
			CaseAllocMem(K4APassiveInfraredWithColor, data);

			// real-sense 2 specific
			CaseAllocMem(RS2_WVGADepth, data);
			CaseAllocMem(RS2_WVGADepthWithColor, data);

			// application specific
			CaseAllocMem(VGABW, data);
			CaseAllocMem(SVGABW, data);

			// generic resolutions
			CaseAllocMem(VGA, data);
			CaseAllocMem(VGADepth, data);
			CaseAllocMem(SVGA, data);
			CaseAllocMem(SVGADepth, data);
			CaseAllocMem(720P, data);
			CaseAllocMem(720PRGBA, data);
			CaseAllocMem(720PDepth, data);
			CaseAllocMem(1080P, data);
			CaseAllocMem(1080PRGBA, data);
			CaseAllocMem(1080PDepth, data);
			CaseAllocMem(1440P, data);
			CaseAllocMem(1440PRGBA, data);
			CaseAllocMem(1440PDepth, data);
			CaseAllocMem(1536P, data);
			CaseAllocMem(1536PRGBA, data);
			CaseAllocMem(1536PDepth, data);
			CaseAllocMem(2160P, data);
			CaseAllocMem(2160PRGBA, data);
			CaseAllocMem(2160PDepth, data);
			CaseAllocMem(3072P, data);
			CaseAllocMem(3072PRGBA, data);
			CaseAllocMem(3072PDepth, data);

		default:
			data = new unsigned char[size()];
		}
	}

	Frame(unsigned long width, unsigned long height, unsigned long customSize) :
		customDataAlloc(false), width(width), height(height), customSize(customSize), usingCustomSize(true),
		encoding(FrameType::Encoding::Custom)
	{
		data = new unsigned char[size()];
	}


	Frame(unsigned long width, unsigned long height, FrameType::Encoding encoding, void* data) :
		customDataAlloc(true), width(width), height(height), customSize(0), usingCustomSize(false),
		encoding(encoding), data((unsigned char*)data)
	{ }


public:

	// creates a frame with a specific encoding and pre-allocates its memory
	static std::shared_ptr<Frame> Create(unsigned long width, unsigned long height, FrameType::Encoding encoding)
	{
		std::shared_ptr<Frame> f(new Frame(width, height, encoding));
		return f;
	}

	// creates a frame with custom encoding
	static std::shared_ptr<Frame> Create(unsigned long width, unsigned long height, unsigned long size)
	{
		std::shared_ptr<Frame> f(new Frame(width, height, size));
		return f;
	}

	// duplicates a frames
	static std::shared_ptr<Frame> Duplicate(std::shared_ptr<Frame> src)
	{
		if (!src) return src;
		std::shared_ptr<Frame> copy = Frame::Create(src->getWidth(), src->getHeight(), src->getEncoding());
		memcpy(copy->data, src->data, src->size());
		return copy;
	}

	virtual ~Frame()
	{
		if (customDataAlloc) return; // no dellocation required
		if (encoding == FrameType::Encoding::Custom) delete[] data;

		switch (size())
		{
			// k4a specific
			CaseFreeMem(K4ADepth, data);
			CaseFreeMem(K4ADepthWithColor, data);
			CaseFreeMem(K4APassiveInfrared, data);
			CaseFreeMem(K4APassiveInfraredWithColor, data);

			// real-sense 2 specific
			CaseFreeMem(RS2_WVGADepth, data);
			CaseFreeMem(RS2_WVGADepthWithColor, data);

			// application specific
			CaseFreeMem(VGABW, data);
			CaseFreeMem(SVGABW, data);

			// generic resolutions
			CaseFreeMem(VGA, data);
			CaseFreeMem(VGADepth, data);
			CaseFreeMem(SVGA, data);
			CaseFreeMem(SVGADepth, data);
			CaseFreeMem(720P, data);
			CaseFreeMem(720PRGBA, data);
			CaseFreeMem(720PDepth, data);
			CaseFreeMem(1080P, data);
			CaseFreeMem(1080PRGBA, data);
			CaseFreeMem(1080PDepth, data);
			CaseFreeMem(1440P, data);
			CaseFreeMem(1440PRGBA, data);
			CaseFreeMem(1440PDepth, data);
			CaseFreeMem(1536P, data);
			CaseFreeMem(1536PRGBA, data);
			CaseFreeMem(1536PDepth, data);
			CaseFreeMem(2160P, data);
			CaseFreeMem(2160PRGBA, data);
			CaseFreeMem(2160PDepth, data);
			CaseFreeMem(3072P, data);
			CaseFreeMem(3072PRGBA, data);
			CaseFreeMem(3072PDepth, data);

		default:
			delete[] data;
		}
		data = nullptr;
	}

	unsigned long getWidth() const { return width; }
	unsigned long getHeight() const { return height; }
	FrameType::Encoding getEncoding() const { return encoding; }
	unsigned int  getPixelLen(int plane = 0) const { return FrameType::getPixelLen(encoding); }   // this is only valid when not using custom formats
	unsigned long getLineSize(int plane = 0) const { return getPixelLen()* getWidth(); }           // this is only valid when not using custom formats
	unsigned long size() const { return customSize ? customSize : width * height* getPixelLen(); }
	unsigned char* const getData() const { return data; }
private:
	bool customDataAlloc;

protected:
	unsigned long width;
	unsigned long height;
	bool usingCustomSize;
	unsigned long customSize;
	FrameType::Encoding encoding;
public:
	// the last part of the frame is a pointer to the data
	unsigned char* data;
};