#pragma once

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

// json for the fast point cloud
#include <opencv2/opencv.hpp>

/**
  AzureKinect is a Camera device that interfaces with
  the k4a API. 

  Current support: color and depth frames

  Configuration settings implemented:
  * type: "k4a"
  * requestColor: true
  * requestDepth: true
  * colorWidth x colorHeight: 1280x720, 1920x1080, 2560x1440, 2048x1536, 4096x3072 (15fps)
  * depthWidth x depthHeight: 302x288, 512x512, 640x576, 1024x1024 (15fps)
  
  Configuration settings currently not supported:
  * serialNumber: We currently grab the first k4a device available
*/
class AzureKinect : public Camera
{

	// internal implementation of the handle
	k4a::device kinectDevice;

	k4a_device_configuration_t kinectConfiguration;
	k4a::calibration kinectCameraCalibration;
	k4a::transformation kinectCameraTransformation;

	k4a_calibration_intrinsic_parameters_t* intrinsics_color;
	k4a_calibration_intrinsic_parameters_t* intrinsics_depth;
	std::string kinectDeviceSerial;


public:

	/**
	  This method creates a shared pointer to this camera implementation
 	  */
	static std::shared_ptr<Camera> Create(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration)
	{
		return std::make_shared<AzureKinect>(appStatus, configuration);
	}

	AzureKinect(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : Camera(appStatus, configuration), kinectConfiguration(K4A_DEVICE_CONFIG_INIT_DISABLE_ALL),
		intrinsics_color(nullptr), intrinsics_depth(nullptr)
	{
	}

	~AzureKinect()
	{
		Stop();
	}

	virtual void Stop()
	{
		// stop thread first
		Camera::Stop();

		// frees resources
		if (runningCameras)
		{
			kinectDevice.stop_cameras();
			runningCameras = false;
		}

		kinectDevice.close();
	}


	virtual bool AdjustGainBy(int gain_level)
	{
		int proposedGain = currentGain + gain_level;
		if (proposedGain < 0)
		{
			proposedGain = 0;
		}
		else if (proposedGain > 10)
		{
			proposedGain = 10;
		}

		try
		{
			kinectDevice.set_color_control(K4A_COLOR_CONTROL_GAIN, K4A_COLOR_CONTROL_MODE_MANUAL, static_cast<int32_t>((float)proposedGain * 25.5f));
			currentGain = proposedGain;
			Logger::Log("AzureKinect") << "Gain level: " << currentGain << std::endl;
			return true;
		}
		catch (const k4a::error & e)
		{
			Logger::Log("AzureKinect") << "Could not adjust gain level to : " << proposedGain << std::endl;
			return false;
		}
		
	}

	virtual bool AdjustExposureBy(int exposure_level)
	{
		int proposedExposure = currentExposure + exposure_level;
		if (proposedExposure > 1)
		{
			proposedExposure = 1;
		}
		else if (proposedExposure < -11)
		{
			proposedExposure = -11;
		}

		try
		{
			kinectDevice.set_color_control(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, K4A_COLOR_CONTROL_MODE_MANUAL, static_cast<int32_t>(exp2f((float)proposedExposure) *
				1000000.0f));
			currentExposure = proposedExposure;
			Logger::Log("AzureKinect") << "Exposure level: " << currentExposure << std::endl;
			return true;
		}
		catch (const k4a::error& e)
		{
			Logger::Log("AzureKinect") << "Could not adjust exposure level to : " << proposedExposure << std::endl;
			return false;
		}
		
	}


protected:

	// opens the default kinect camera
	bool OpenDefaultKinect();
	
	void saveTransformationTable(int img_width, int img_height);

	// camera loop responsible for receiving frames, transforming them, and invoking callbacks
	virtual void CameraLoop();

	// parses app status to configure device
	bool LoadConfigurationSettings();
	

};


