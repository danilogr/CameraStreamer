#pragma once

// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_TCPCLIENT_RELAY_CAMERA

// stl
#include <string>

// our framework
#include "Logger.h"
#include "Configuration.h"
#include "ApplicationStatus.h"
#include "Frame.h"
#include "Camera.h"

// boost requirements for this camera
#include <boost/asio.hpp>


/**
  TCP Relay camera relays incoming streams (yuv, rgb, jpeg, etc...)
  
  Its utility lies on the fact that CameraStreamer has other features
  (such as streaming in a different format, saving to files, etc...)

  Generic configuration settings implemented:
  * type : "replay"
  * requestColor: true -> should forward color if available
  * requestDepth: depth -> should forward depth if available

  TCPRelayCamera-specific configuration elements (All required):
  * host: URI this camera should connect to
  * port: port used to connect
  * headerType: string describing the header this camera should parse
                "CameraStreamer", "LengthWidthHeight", "JPEGLengthValue", "Length", "None"

  * colorFormat: string describing what this camera should expect in the content
                 (content type is ignored and set to JPEG when using JPEGLengthValue)
			e.g.: "yuv422", "rgb", "bgr", "rgba", "bgra" , "jpeg", "none" (when no color is expected)

  * depthFormat: string describing what this camera should expect in the content
				 (content type is ignored and set to None when using JPEGLengthValue)
		    e.g.: "raw16", "raw8", "none"


  In order to save video files, CameraStreamer **cannot** act as a blind relay server.
  Thus, for combinations such as headerType = "Length" and colorFormat="rgb", 
  we need to know the dimensions of the frame ahead of time.
             
  In the future, we could support that by asking for a few additional configuration details:

  (todo)
      Configuration settings only used when the header is None:
      * colorFirst: true if we should read color first
      * colorWidth x colorHeight: -> how many bytes to read for color
      * depthWidth x depthHeight: -> how many bytes to read for depth
  (todo)  


  Alternatively, we could provide a grammar for network protocol description. Something much simpler
  than ProtocolBuf with pre-defined keywords. E.g.:

  packet = "[TotalLength:4][ColorWidth:4][ColorHeight:4][ColorFrame:ColorWidthxColorHeightx3]"

  for now, we cover basic use cases required in our research

 */
class TCPRelayCamera : public Camera
{
	// hostAddr as found in the configuration file
	std::string hostAddr;

	// hostPort as found in the configuration file
	int hostPort;

	// where we should connect to
	boost::asio::ip::tcp::endpoint hostEndpoint;

protected:

	// method that finds a suitable camera given what is set in the app status
	bool LoadConfigurationSettings();

	// camera loop responsible for receiving frames, transforming them, and invoking callbacks
	virtual void CameraLoop();

	// used in all camera logs
	static const char* TCPRelayCameraConstStr;

public:

	/**
	  This method creates a shared pointer to this camera implementation
	*/
	static std::shared_ptr<Camera> Create(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration)
	{
		return std::make_shared<TCPRelayCamera>(appStatus, configuration);
	}

	TCPRelayCamera(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : Camera(appStatus, configuration)
	{

	}

	~TCPRelayCamera()
	{
		Stop();
	}

	virtual void Stop()
	{
		// stop thread first
		Camera::Stop();

		// frees resources
		if (IsAnyCameraEnabled())
		{
			depthCameraEnabled = false;
			colorCameraEnabled = false;
		}
	}


	virtual bool AdjustGainBy(int gain_level)
	{
		return false;

	}

	virtual bool AdjustExposureBy(int exposure_level)
	{
		return false;
	}
};

#endif