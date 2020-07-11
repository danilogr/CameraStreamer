#pragma once

// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_TCPCLIENT_RELAY_CAMERA


// our framework
#include "Logger.h"
#include "Configuration.h"
#include "ApplicationStatus.h"
#include "Frame.h"
#include "Camera.h"

/**
  TCP Relay camera relays incoming streams (yuv, rgb, jpeg, etc...)
  
  Its utility lies on the fact that CameraStreamer has other features
  (such as streaming in a different format, saving to files, etc...)

  Generic configuration settings implemented:
  * type : "replay"
  * requestColor: true -> should forward color if available
  * requestDepth: depth -> should forward depth if available

  Custom configuration elements:
  * host: URI this camera should connect to
  * port: port used to connect
  * headerType: string describing the header this camera should parse
                "CameraStreamer", "JPEGLengthValue",
                
                
  In the future, we could support a custom header format or something like:  "None"

  (todo)
      Configuration settings only used when the header is None:
      * colorFirst: true if we should read color first
      * colorType: string -> RGB, RBG, YUV422, RGBA
      * depthType: string -> raw8, raw16
      * colorWidth x colorHeight: -> how many bytes to read for color
      * depthWidth x depthHeight: -> how many bytes to read for depth
  (todo)  
 */
class TCPRelayCamera : public Camera
{

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