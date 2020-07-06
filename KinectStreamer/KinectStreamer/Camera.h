#pragma once


#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>

#include "Frame.h"
#include "Logger.h"

#include "Configuration.h"
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
	float metricScale;   // Scale to transform measurements into meters (only makes sense for depth cameras)

	CameraIntrinsics() : cx(0), cy(0), fx(0), fy(0),
		k1(0), k2(0), k3(0), k4(0), k5(0), k6(0), p2(0), p1(0),
		metricRadius(0), metricScale(0)
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


struct CameraStatistics
{
	// number of frames captured on the long run
	unsigned long long framesCapturedTotal;
	unsigned long long framesFailedTotal;
	unsigned int sessions;
	std::chrono::steady_clock::time_point startTimeTotal;
	std::chrono::steady_clock::time_point endTimeTotal;

	// number of frames captured in this session
	unsigned long long framesCaptured;
	unsigned long long framesFailed;
	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point endTime;


	CameraStatistics() : framesCapturedTotal(0), framesFailedTotal(0), sessions(0), framesCaptured(0), framesFailed(0), initialized(false), inSession(false) {}

	void StartCounting()
	{
		// did we forget to stop counting?
		if (inSession)
		{
			StopCounting(); // not super accurate, but whatevs
		}

		startTime = std::chrono::high_resolution_clock::now();
		framesCaptured = 0;
		framesFailed = 0;

		// this is the first time we are running the camera loop
		// thus, we will set both the startTimeTotal and the startTime
		if (!initialized)
		{
			initialized = true;
			startTimeTotal = startTime;
		}

		++sessions;
		inSession = true;
	}

	void StopCounting()
	{
		if (inSession)
		{
			endTime = std::chrono::high_resolution_clock::now();
		
			// add information to the long run
			inSession = false;
			endTimeTotal = endTime;
			framesCapturedTotal += framesCaptured;
			framesFailedTotal += framesFailed;
		}
	}

	long long durationInSeconds()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
	}

	long long totalDurationInSeconds()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(endTimeTotal - startTimeTotal).count();
	}

private:
	bool initialized, inSession;
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

public:

protected:
	// camera properties that help us identify the type of camera
	// when using the abstract Camera interface
	std::string cameraSerialNumber;
	std::string cameraType;

	// configurable camera settings
	int currentExposure = 0;
	int currentGain = 0;

	// timeouts?
	std::chrono::milliseconds getFrameTimeout;
	unsigned int getFrameTimeoutMSInt;

	// which camera is running?
	bool depthCameraEnabled, colorCameraEnabled;

	// all threads use a shared data structure to report their status
	std::shared_ptr<ApplicationStatus> appStatus;

	// configuration (from either a configuration file or a setting set by the user)
	std::shared_ptr<Configuration> configuration;

	// cameras have a thread that handles requesting frames
	// Todo: we should create an abstract thread class for future uses
	std::shared_ptr<std::thread> sThread;
	bool thread_running;

	// Virtual Methods to start and stop the camera
	virtual void CameraLoop() = 0;

	void thread_main()
	{
		// executes the internal camera loop
		thread_running = true;

		// loops through the camera loop implementation
		while (thread_running)
		{
			try
			{
				CameraLoop();
			}
			catch (...)
			{
				Logger::Log("Camera") << "Unhandled exception in " << cameraType << " (" << cameraSerialNumber << "). Restarting camera thread in 5 seconds..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}
		}

		// This should not happen because this method
		// is running within the thread!
		//sThread = nullptr;
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

	// constructor explicitly defining a configuration file (as well as appStatus)
	Camera(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : currentExposure(0), currentGain(0), appStatus(appStatus), configuration(configuration), thread_running(false), depthCameraEnabled(false), colorCameraEnabled(false), getFrameTimeout(1000), getFrameTimeoutMSInt(1000)
	{
		// camera frame timeout
		getFrameTimeoutMSInt = configuration->GetCameraFrameTimeoutMs();
		getFrameTimeout = configuration->GetCameraFrameTimoutMsChrono();

		// are we looking for a specific camera? let users know and avoid confusion
		if (!configuration->UseFirstCameraAvailable())
		{
			Logger::Log("Camera") << "Attention: This application is looking for a " << configuration->GetCameraType() << " camera with SN " << configuration->GetCameraSN() << std::endl;
		}
	}

	~Camera()
	{
		Stop();
	}

	bool IsThreadRunning()
	{
		return (sThread && sThread->joinable());
	}

	// Returns true if kinect is opened and streaming video
	bool IsAnyCameraEnabled()
	{
		return colorCameraEnabled || depthCameraEnabled;
	}

	// returns true if the depth camera is opened and streaming video
	bool IsDepthCameraEnabled()
	{
		return depthCameraEnabled;
	}

	// returns true if the color camera is opened and streaming video
	bool IsColorCameraEnabled()
	{
		return colorCameraEnabled;
	}


	// responsible for stopping the thread
	virtual void Stop()
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


	// Returns a json file with a valid OpenCV camera intrinsic matrix
	virtual const std::string& OpenCVCameraMatrix(const CameraParameters& param);

	// Prints camera parameters
	virtual void PrintCameraIntrinsics()
	{
		if (depthCameraEnabled)
		{
			Logger::Log("Camera") << "[Depth] resolution width: " << depthCameraParameters.resolutionWidth << std::endl;
			Logger::Log("Camera") << "[Depth] resolution height: " << depthCameraParameters.resolutionHeight << std::endl;
			Logger::Log("Camera") << "[Depth] metric radius: " << depthCameraParameters.metricRadius << std::endl;
			Logger::Log("Camera") << "[Depth] principal point x: " << depthCameraParameters.intrinsics.cx << std::endl;
			Logger::Log("Camera") << "[Depth] principal point y: " << depthCameraParameters.intrinsics.cy << std::endl;
			Logger::Log("Camera") << "[Depth] focal length x: " << depthCameraParameters.intrinsics.fx << std::endl;
			Logger::Log("Camera") << "[Depth] focal length y: " << depthCameraParameters.intrinsics.fy << std::endl;
			Logger::Log("Camera") << "[Depth] radial distortion coefficients:" << std::endl;
			Logger::Log("Camera") << "[Depth] k1: " << depthCameraParameters.intrinsics.k1 << std::endl;
			Logger::Log("Camera") << "[Depth] k2: " << depthCameraParameters.intrinsics.k2 << std::endl;
			Logger::Log("Camera") << "[Depth] k3: " << depthCameraParameters.intrinsics.k3 << std::endl;
			Logger::Log("Camera") << "[Depth] k4: " << depthCameraParameters.intrinsics.k4 << std::endl;
			Logger::Log("Camera") << "[Depth] k5: " << depthCameraParameters.intrinsics.k5 << std::endl;
			Logger::Log("Camera") << "[Depth] k6: " << depthCameraParameters.intrinsics.k6 << std::endl;
			Logger::Log("Camera") << "[Depth] tangential distortion coefficient x: " << depthCameraParameters.intrinsics.p1 << std::endl;
			Logger::Log("Camera") << "[Depth] tangential distortion coefficient y: " << depthCameraParameters.intrinsics.p2 << std::endl;
			Logger::Log("Camera") << "[Depth] metric radius (intrinsics): " << depthCameraParameters.intrinsics.metricRadius << std::endl;
			Logger::Log("Camera") << "[Depth] metric radius (to meters): " << depthCameraParameters.intrinsics.metricScale << std::endl << std::endl;

		}

		if (colorCameraEnabled)
		{
			Logger::Log("Camera") << "[Color] resolution width: " << colorCameraParameters.resolutionWidth << std::endl;
			Logger::Log("Camera") << "[Color] resolution height: " << colorCameraParameters.resolutionHeight << std::endl;
			Logger::Log("Camera") << "[Color] metric radius: " << colorCameraParameters.metricRadius << std::endl;
			Logger::Log("Camera") << "[Color] principal point x: " << colorCameraParameters.intrinsics.cx << std::endl;
			Logger::Log("Camera") << "[Color] principal point y: " << colorCameraParameters.intrinsics.cy << std::endl;
			Logger::Log("Camera") << "[Color] focal length x: " << colorCameraParameters.intrinsics.fx << std::endl;
			Logger::Log("Camera") << "[Color] focal length y: " << colorCameraParameters.intrinsics.fy << std::endl;
			Logger::Log("Camera") << "[Color] radial distortion coefficients:" << std::endl;
			Logger::Log("Camera") << "[Color] k1: " << colorCameraParameters.intrinsics.k1 << std::endl;
			Logger::Log("Camera") << "[Color] k2: " << colorCameraParameters.intrinsics.k2 << std::endl;
			Logger::Log("Camera") << "[Color] k3: " << colorCameraParameters.intrinsics.k3 << std::endl;
			Logger::Log("Camera") << "[Color] k4: " << colorCameraParameters.intrinsics.k4 << std::endl;
			Logger::Log("Camera") << "[Color] k5: " << colorCameraParameters.intrinsics.k5 << std::endl;
			Logger::Log("Camera") << "[Color] k6: " << colorCameraParameters.intrinsics.k6 << std::endl;
			Logger::Log("Camera") << "[Color] tangential distortion coefficient x: " << colorCameraParameters.intrinsics.p1 << std::endl;
			Logger::Log("Camera") << "[Color] tangential distortion coefficient y: " << colorCameraParameters.intrinsics.p2 << std::endl;
			Logger::Log("Camera") << "[Color] metric radius (intrinsics): " << colorCameraParameters.intrinsics.metricRadius << std::endl << std::endl;
		}

		
	}

	// Camera paremeters
	CameraParameters depthCameraParameters, colorCameraParameters;

	// Camera statistics
	CameraStatistics statistics;

	const std::string& getSerial() const
	{
		return cameraSerialNumber;
	}

	const std::string& getCameraType() const
	{
		return cameraType;
	}

};

