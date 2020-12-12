#pragma once

// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_REPLAY_CAMERA

// our framework
#include "Logger.h"
#include "Configuration.h"
#include "ApplicationStatus.h"
#include "Frame.h"
#include "Camera.h"

/**
  Replay camera camera replays generic video files, K4A MKV files or our old school custom recordings)
  
  In the future, it will replay K4A MKV files


  Custom configuration elements:
  * replayType: "opencv-video-file","k4a-mkv", "k4a-mkv-separate-files", "legacy-recorder"
  * loop: whether or not to loop (defaults to True) (ignored when using  "opencv-webcam")

  * for each replay camera type supported:
  ** -> "opencv-video-file"
        "path" or "colorPath" -> only color
  ** -> k4a-mkv
        "path" -> mkv file
  ** -> k4a-mkv-separate-files
	    "colorPath" -> mkv with color stream
		"depthPath" -> mkv with depth stream
  ** -> legacy-recorder
		"colorPath" -> mp4 with color stream
		"depthPath" -> .bin file with depth stream


  Generic configuration settings implemented:
  * type : "replay"
  * requestColor: true
  * requestDepth: true/false

  Configuration settings ignored:
  (For now, files only have a single stream of each type; thus, resolution
   and serial number are not supported)
  * colorWidth x colorHeight: -> support
  * depthWidth x depthHeight:
  * serialNumber: if available in the file, looks for a camera with a specific serial number
 */
class ReplayCamera : public Camera
{

protected:

	// method that finds a suitable camera given what is set in the app status
	virtual bool LoadConfigurationSettings();

	// camera loop responsible for receiving frames, transforming them, and invoking callbacks
	virtual void CameraLoop();

	// used in all camera logs
	static const char* ReplayCameraConstStr;

public:

	/**
	  This method creates a shared pointer to this camera implementation
	*/
	static std::shared_ptr<Camera> Create(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration)
	{
		return std::make_shared<ReplayCamera>(appStatus, configuration);
	}

	ReplayCamera(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : Camera(appStatus, configuration)
	{
	}

	~ReplayCamera()
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

#endif // ENABLE_REPLAY_CAMERA