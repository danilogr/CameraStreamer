#pragma once

// are we compilling support to this camera into our application?
#include "CompilerConfiguration.h"
#ifdef ENABLE_MKV_PLAYER


// std
#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>

// our framework
#include "Logger.h"
#include "ApplicationStatus.h"
#include "Frame.h"
#include "Camera.h"

// kinect sdk
#include <k4a/k4a.hpp>
#include <k4arecord/playback.hpp>

// json for the fast point cloud
#include <opencv2/opencv.hpp>

/**
  AzureKinect is a Camera device that interfaces with
  the k4a API. 

  Current support: color and depth frames

  Configuration settings implemented:
  * type: "k4a-file"
  * requestColor: true / false -> changes whether or not color is served (defaults to true)
  * requestDepth: true / false -> changes whether or not depth is server (defaults to true)
  *
  * files usually come with a configuration of their own, but you
  * colorWidth x colorHeight: 1280x720, 1920x1080, 2560x1440, 2048x1536, 4096x3072 
  * depthWidth x depthHeight: 302x288, 512x512, 640x576, 1024x1024 
  
  Configuration settings currently not supported:
  * serialNumber: We currently grab the first k4a device available
*/
class AzureKinectFileReader : public Camera
{

	// internal implementation of the handle
	k4a::playback mkvPlayer;

	k4a_device_configuration_t kinectConfiguration;
	k4a::calibration kinectCameraCalibration;
	k4a::transformation kinectCameraTransformation;

	k4a_calibration_intrinsic_parameters_t* intrinsics_color;
	k4a_calibration_intrinsic_parameters_t* intrinsics_depth;
	std::string kinectDeviceSerial;


	static const char* AzureKinectFileReaderConstStr;

public:

	/**
	  This method creates a shared pointer to this camera implementation
 	  */
	static std::shared_ptr<Camera> Create(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration)
	{
		return std::make_shared<AzureKinectFileReader>(appStatus, configuration);
	}

	AzureKinectFileReader(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : Camera(appStatus, configuration), kinectConfiguration(K4A_DEVICE_CONFIG_INIT_DISABLE_ALL),
		intrinsics_color(nullptr), intrinsics_depth(nullptr)
	{
	}

	~AzureKinectFileReader()
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
			//mkvPlayer.stop_cameras();
			depthCameraEnabled = false;
			colorCameraEnabled = false;
		}

		mkvPlayer.close();
	}


	virtual bool AdjustGainBy(int gain_level)
	{
		
		Logger::Log(AzureKinectFileReaderConstStr) << "Cannot adjust gain of a recording" << std::endl;
		return false;
		
		
	}

	virtual bool AdjustExposureBy(int exposure_level)
	{
		Logger::Log(AzureKinectFileReaderConstStr) << "Cannot adjust exposure of a recording" << std::endl;
	}


protected:

	// opens the default kinect camera
	bool OpenK4AMKVFile();
	
	void saveTransformationTable(int img_width, int img_height);

	// camera loop responsible for receiving frames, transforming them, and invoking callbacks
	virtual void CameraLoop();

	// parses app status to configure device
	virtual bool LoadConfigurationSettings();
	

};


#endif