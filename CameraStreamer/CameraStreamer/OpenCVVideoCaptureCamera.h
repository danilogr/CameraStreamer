#pragma once

// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef CS_ENABLE_CAMERA_CV_VIDEOCAPTURE

// std
#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>
#include <set>

// our framework
#include "Logger.h"
#include "Configuration.h"
#include "ApplicationStatus.h"
#include "Frame.h"
#include "Camera.h"

// opencv video capture
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

/**
  OpenCV VideoCapture API supports reading video files, image sequences, webcams, network cameras, etc...
  
  The downside to OpenCV VideoCapture is that the camera index might change so it might not always give you the 
  camera you were expecting to receive. 

  Configuration settings implemented:
  * type : "opencv"
  * requestColor: true
  * colorWidth x colorHeight: - only available if using webcam (video files and image sequence have pre-defined resolutions)
  * fps: - only available if using webcam or image sequence (video files have a capture frame rate set)
  * 
  * 
  OpenCV VideoCapture camera specifics (use url or index):
  * url: URI this camera should connect to
  *		e.g,:	name of video file (eg. video.avi)
  *				image sequence (eg. img_%02d.jpg, which will read samples like img_00.jpg, img_01.jpg, img_02.jpg, ...)
  *				URL of video stream (eg. protocol://host:port/script_name?script_params|auth)
  * 
  * if url is not provided then you can use index to select a webcam
  *			
  * index: 	id of the video capturing device to open. To open default camera using default backend just pass 0. 
  *         (defaults to 0)
  *
  *
  * 
  * Future work:
  * 
  * serialNumber: if set, and on windows only, looks for a camera with a specific name and opens the first one
  * 
  * reader: OpenCV string with the reader library that should be used - defaults to CAP_ANY
  *         (CAP_FFMPEG, CAP_IMAGES, CAP_DSHOW, CAP_MSMF, CAP_V4L)
  * 
  Configuration settings that cannot be set or changed (unsupported):
  * requestDepth: false (depth streams are not supported by OpenCV VideoCapture)
  * depthWidth x depthHeight:

  author: Danilo Gasques (gasques@ucsd.edu)
 */
class CVVideoCaptureCamera : public Camera
{

	std::shared_ptr<cv::VideoCapture> device;
	static const char* CVVideoCaptureCameraStr;
	bool usingWebcam, usingFile;
	std::string url;
	int cameraIndex, frameCount;

protected:

	// method that finds a suitable camera given what is set in the app status
	bool LoadConfigurationSettings();

	// camera loop responsible for receiving frames, transforming them, and invoking callbacks
	virtual void CameraLoop();


public:

	/**
	  This method creates a shared pointer to this camera implementation
	*/
	static std::shared_ptr<Camera> Create(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration)
	{
		return std::make_shared<CVVideoCaptureCamera>(appStatus, configuration);
	}

	/**
	  Returns a set of all devices connected to the computer
	  (the order in which devices appear give their unique id)
	*/
	static std::vector<std::tuple<std::string, std::string>> ListDevices();

	CVVideoCaptureCamera(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : Camera(appStatus, configuration), usingWebcam(false), cameraIndex(-1), usingFile(false), frameCount(-1)
	{
	}

	~CVVideoCaptureCamera()
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

#endif // CS_ENABLE_CAMERA_RS2



