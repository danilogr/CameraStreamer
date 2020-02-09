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

struct NFOVUnbinnedFrameDim { constexpr static unsigned int Size() { return (640 * 576 * sizeof(uint16_t) * 1); } };
struct RGBX2DepthFrameDim { constexpr static unsigned int Size() { return (640 * 576 * sizeof(uint8_t) * 3); } };

struct RGBXFrameDim720PFrameDim { constexpr static unsigned int Size() { return (1280 * 720 * sizeof(uint8_t) * 8); } };
//struct SVGAColorFrameDim { constexpr static unsigned int Size() { return (800 * 600 * sizeof(uint8_t) * 3); } };
//struct SVGABWFrameDim { constexpr static unsigned int Size() { return (800 * 600 * sizeof(uint8_t) * 1); } };
//struct VGAColorFrameDim { constexpr static unsigned int Size() { return (640 * 480 * sizeof(uint8_t) * 3); } };
struct PassiveIRFrameDim { constexpr static unsigned int Size() { return (1024 * 1024 * sizeof(uint16_t) * 1); } };

typedef boost::singleton_pool<NFOVUnbinnedFrameDim, NFOVUnbinnedFrameDim::Size()> NFOVUnbinnedFrameMemoryPool;
//typedef boost::singleton_pool<SVGABWFrameDim, SVGABWFrameDim::Size()> SVGABWMemoryPool;
//typedef boost::singleton_pool<VGAColorFrameDim, VGAColorFrameDim::Size()> VGAMemoryPool;
typedef boost::singleton_pool<PassiveIRFrameDim, PassiveIRFrameDim::Size()> PassiveIRFrameMemoryPool;
typedef boost::singleton_pool<RGBXFrameDim720PFrameDim, RGBXFrameDim720PFrameDim::Size()> RGBXFrameDim720PMemoryPool;
typedef boost::singleton_pool<RGBX2DepthFrameDim, RGBX2DepthFrameDim::Size()> RGBX2DepthMemoryPool;

class Frame : boost::noncopyable
{
protected:
	Frame(unsigned long width, unsigned long height, FrameType::Encoding encoding) :
		customDataAlloc(false), width(width), height(height), customSize(0), usingCustomSize(false), encoding(encoding)
	{
		switch (size())
		{
		case NFOVUnbinnedFrameDim::Size():
			data = (unsigned char*)NFOVUnbinnedFrameMemoryPool::malloc();
			break;

		case PassiveIRFrameDim::Size():
			data = (unsigned char*)PassiveIRFrameMemoryPool::malloc();
			break;

		case RGBXFrameDim720PFrameDim::Size():
			data = (unsigned char*)RGBXFrameDim720PMemoryPool::malloc();
			break;

		case RGBX2DepthFrameDim::Size():
			data = (unsigned char*)RGBX2DepthMemoryPool::malloc();
			break;

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

		switch (size())
		{
		case NFOVUnbinnedFrameDim::Size():
			NFOVUnbinnedFrameMemoryPool::free(data);
			break;

		case PassiveIRFrameDim::Size():
			PassiveIRFrameMemoryPool::free(data);
			break;

	 	case RGBXFrameDim720PFrameDim::Size():
			RGBXFrameDim720PMemoryPool::free(data);
			break;

		case RGBX2DepthFrameDim::Size():
			RGBX2DepthMemoryPool::free(data);
			break;

		default:
			delete data;
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