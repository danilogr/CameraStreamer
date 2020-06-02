#pragma once


#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>

#include "Frame.h"
#include "Logger.h"
#include "ApplicationStatus.h"


// Camera Intrinsics -    These are based on _k4a_calibration_camera_t
struct CameraIntrinsics
{
	// Todo, export an opencv readable float[]

	float cx;            // Principal point in image, x */
	float cy;            // Principal point in image, y */
	float fx;            // Focal length x */
	float fy;            // Focal length y */
	float k1;            // k1 radial distortion coefficient */
	float k2;            // k2 radial distortion coefficient */
	float k3;            // k3 radial distortion coefficient */
	float k4;            // k4 radial distortion coefficient */
	float k5;            // k5 radial distortion coefficient */
	float k6;            // k6 radial distortion coefficient */
	float p2;            // Tangential distortion coefficient 2 */
	float p1;            // Tangential distortion coefficient 1 */
	float metricRadius;  // Metric radius */

	CameraIntrinsics() : cx(0), cy(0), fx(0), fy(0),
		k1(0), k2(0), k3(0), k4(0), k5(0), k6(0), p2(0), p1(0),
		metricRadius(0)
	{

	}

};

//   CameraExtrinsics - These are based on _k4a_calibration_camera_t
struct CameraExtrinsics
{
	float rotation[9];    /** 3x3 Rotation matrix stored in row major order */
	float translation[3]; /** Translation vector, x,y,z (in millimeters) */

	CameraExtrinsics() : rotation{0,0,0,0,0,0,0,0,0}, translation{0,0,0}
	{

	}
};

/**
   Structure for generic camera parameters

   Applications can then request extrinsic camera info.
 */
struct CameraParameters
{

	CameraIntrinsics intrinsics;
	CameraExtrinsics extrinsics;

	int resolutionWidth;
	int resolutionHeight;
	float metricRadius;
};

/**
  The Camera class is an abstraction to all cameras supported by this application.
  In an ideal world, cameras would be loaded dynamically so that our application doesn't
  require lots of DLLs. For now, we compile and link to all the cameras that we might need.

  Todo: Support Building with / without specific cameras
  Todo: Support loading camera modules dynamically from DLLs
  */
class Camera
{

protected:
	// camera properties that help us identify the type of camera
	// when using the abstract Camera interface
	std::string cameraSerialNumber;
	std::string cameraName;

	// configurable camera settings
	int currentExposure = 0;
	int currentGain = 0;

	// timeouts?
	std::chrono::milliseconds getFrameTimeout;

	// is the camera running?
	bool runningCameras;

	// most of the configuration comes from the AppStatus class
	std::shared_ptr<ApplicationStatus> appStatus;

	// cameras have a thread that handles requesting frames
	// Todo: we should create an abstract thread class for future uses
	std::shared_ptr<std::thread> sThread;
	bool thread_running;


	// Virtual Methods to start and stop the camera
	virtual void StopCameras() = 0;
	virtual void FreeDevice() = 0;
	virtual void CameraLoop() = 0;

	void thread_main()
	{
		// executes the internal camera loop
		thread_running = true;

		try
		{
			CameraLoop();
		}
		catch (...)
		{
			Logger::Log("Camera") << "Unhandled exception in " << cameraName << " (" << cameraSerialNumber << "). Exiting..." << std::endl;
			
			// erases thread info
			thread_running = true;
			sThread = nullptr;

			// were devices running?
			if (runningCameras)
			{
				StopCameras();
				runningCameras = false;
			}


		}
	}


	// timestamp, color, depth
	typedef std::function<void(std::chrono::microseconds, std::shared_ptr<Frame>, std::shared_ptr<Frame>)> FrameReadyCallback;
	typedef std::function<void()> CameraConnectedCallback;
	typedef std::function<void()> CameraDisconnectedCallback;


public:

	// callback invoked when a frame is ready
	FrameReadyCallback onFramesReady;

	// callback invoked when the camera is running
	CameraConnectedCallback onCameraConnect;

	// callback invoked when the camera disconnects
	CameraDisconnectedCallback onCameraDisconnect;

	// constructor 
	Camera(std::shared_ptr<ApplicationStatus> appStatus) : appStatus(appStatus), thread_running(false), runningCameras(false), getFrameTimeout(1000)
	{

	}

	~Camera()
	{
		Stop();
	}

	bool isRunning()
	{
		return (sThread && sThread->joinable());
	}

	// Returns true if kinect is opened and streaming video
	bool isStreaming()
	{
		return runningCameras;
	}

	void Stop()
	{

		// if we are running
		if (thread_running)
		{
			thread_running = false;     // stops the loop in case it is running
			cameraSerialNumber.clear(); // erases serial

			if (sThread && sThread->joinable())
				sThread->join();

			// makes sure that sthread doesn't point to anything
			sThread = nullptr;
		}

		// frees resources
		if (runningCameras)
		{
			StopCameras();
			runningCameras = false;
		}

		FreeDevice();
	}


	
	virtual void SetCameraSpecificConfiguration(void* cameraSpecificConfiguration)
	{

	}

	void Run()
	{
		if (!thread_running && !sThread)
		{
			// starts thread
			sThread.reset(new std::thread(std::bind(&Camera::thread_main, this)));
		}
	}

	//
	// Virtual Methods that classes should override
	//

	// Adjust the camera gain
	virtual bool AdjustGainBy(int gain_level) = 0;
	
	// Adjust the camera exposure
	virtual bool AdjustExposureBy(int exposure_level) = 0;

	// Camera paremeters
	CameraParameters calibration;

	const std::string& getSerial()
	{
		return cameraSerialNumber;
	}

	const std::string& getCameraType()
	{
		return cameraName;
	}

};

