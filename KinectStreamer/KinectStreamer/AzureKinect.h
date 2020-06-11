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
	static std::shared_ptr<Camera> Create(std::shared_ptr<ApplicationStatus> appStatus)
	{
		return std::make_shared<AzureKinect>(appStatus);
	}

	AzureKinect(std::shared_ptr<ApplicationStatus> appStatus) : Camera(appStatus), kinectConfiguration(K4A_DEVICE_CONFIG_INIT_DISABLE_ALL),
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

	virtual void SetCameraConfigurationFromCustomDatastructure(void* cameraSpecificConfiguration)
	{
		kinectConfiguration = (*(k4a_device_configuration_t*)cameraSpecificConfiguration);
	}

	virtual void SetCameraConfigurationFromAppStatus(bool resetConfiguration)
	{

		bool canRun30fps = true;

		// blank slate
		if (resetConfiguration)
			kinectConfiguration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;

		// first figure out which cameras have been loaded
		if (true || appStatus->IsColorCameraEnabled())
		{
			kinectConfiguration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32; // for now, we force BGRA32

			const int requestedWidth = appStatus->GetCameraColorWidth();
			const int requestedHeight = appStatus->GetCameraColorHeight();
			int newWidth = requestedWidth;
			
			switch (requestedHeight)
			{
				case 720:
					kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_720P;
					newWidth = 1280;
				break;

				case 1080:
					kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_1080P;
					newWidth = 1920;
					break;

				case 1440:
					kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_1440P;
					newWidth = 2560;
					break;

				case 1536:
					kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_1536P;
					newWidth = 2048;
					break;

				case 2160:
					kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_2160P;
					newWidth = 3840;
					break;
				case 3072:
					kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_3072P;
					newWidth = 4096;
					canRun30fps = false;
					break;

				default:
					Logger::Log("AzureKinect") << "Color camera Initialization Error! The requested resolution is not supported: " << requestedWidth << 'x' << requestedHeight << std::endl;
					kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_720P;
					appStatus->SetCameraColorHeight(720);
					appStatus->SetCameraColorWidth(1280);
					newWidth = 1280;
					break;
			}

			// adjusting requested resolution to match an accepted resolution
			if (requestedWidth != newWidth)
			{
				// add warning?
				appStatus->SetCameraColorWidth(newWidth);
			}

		}
		else {
			kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_OFF;
		}

		// then figure out depth
		if (true || appStatus->IsDepthCameraEnabled())
		{
			// we will decided on the mode (wide vs narrow) based on the resolution

			//kinectConfiguration.depth_mode
			const int requestedWidth = appStatus->GetCameraDepthWidth();
			const int requestedHeight = appStatus->GetCameraDepthHeight();
			int newWidth = requestedWidth;

			switch (requestedHeight)
			{
				case 288:
					kinectConfiguration.depth_mode = K4A_DEPTH_MODE_NFOV_2X2BINNED;
					newWidth = 320;
					break;
				case 512:
					kinectConfiguration.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
					newWidth = 512;
					break;
				case 576:
					kinectConfiguration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
					newWidth = 640;
					break;
				case 1024:
					kinectConfiguration.depth_mode = K4A_DEPTH_MODE_WFOV_UNBINNED;
					newWidth = 1024;
					canRun30fps = false;
					break;
				default:
					Logger::Log("AzureKinect") << "Depth camera Initialization Error! The requested resolution is not supported: " << requestedWidth << 'x' << requestedHeight << std::endl;
					kinectConfiguration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
					appStatus->SetCameraDepthHeight(576);
					appStatus->SetCameraDepthWidth(640);
					newWidth = 640;
					break;
			}

			// adjusting requested resolution to match an accepted resolution
			if (requestedWidth != newWidth)
			{
				// add warning?
				appStatus->SetCameraDepthWidth(newWidth);
			}

			
		} else {
			// camera is off
			kinectConfiguration.depth_mode = K4A_DEPTH_MODE_OFF;
		}

		// if both cameras are enabled, we will make sure that frames are synchronized
		if (appStatus->IsColorCameraEnabled() && appStatus->IsDepthCameraEnabled())
		{
			kinectConfiguration.synchronized_images_only = true;
		}

		// are we able to run at 30 fps?
		if (canRun30fps)
		{
			kinectConfiguration.camera_fps = K4A_FRAMES_PER_SECOND_30;
		}
		else {
			// the second fastest speed it can run is 15fps
			kinectConfiguration.camera_fps = K4A_FRAMES_PER_SECOND_15;
			Logger::Log("AzureKinect") << "WARNING! The selected combination of depth and color resolution can run at a max of 15fps!" << std::endl;
		}

		// what about the device?
		if (!appStatus->UseFirstCameraAvailable())
		{
			Logger::Log("AzureKinect") << "WARNING! We currently do not support selecting a camera based on serial number (Sorry!)" << std::endl;
		}
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


private:

	// opens the default kinect camera
	bool OpenDefaultKinect();
	
	void saveTransformationTable(int img_width, int img_height);

	// camera loop responsible for receiving frames, transforming them, and invoking callbacks
	virtual void CameraLoop();

};


