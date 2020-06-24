#pragma once

// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_RS2

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

// real sense sdk
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING 1 // see https://github.com/IntelRealSense/librealsense/issues/6283
#include <librealsense2/rs.hpp>

/**
  RealSense implements the generic RealSense camera API
  
  We only tested it with D435, but it should theoretically support all RealSense cameras

  Configuration settings implemented:
  * type : "rs2"
  * requestColor: true
  * requestDepth: true
  * colorWidth x colorHeight: 
  * depthWidth x depthHeight:
  * serialNumber: if set, looks for a camera with a specific serial number
 */
class RealSense : public Camera
{
	rs2::pipeline realsensePipeline;

	std::shared_ptr<rs2::device> device;
	std::shared_ptr<rs2::playback> playback;

	std::shared_ptr<rs2::decimation_filter> dec_filter;				// Decimation - reduces depth frame density
	std::shared_ptr<rs2::spatial_filter> spat_filter;				// Spatial    - edge-preserving spatial smoothing
	std::shared_ptr<rs2::temporal_filter> temp_filter;				// Temporal   - reduces temporal noise
	std::shared_ptr<rs2::disparity_transform> depth_to_disparity;
	std::shared_ptr<rs2::disparity_transform> disparity_to_depth;

	rs2::config rs2Configuration;

	static const char* RealSenseConstStr;

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
		return std::make_shared<RealSense>(appStatus, configuration);
	}

	/**
	  Returns a set of all devices connected to the computer
	*/
	static const std::set<std::string>& ListDevices()
	{
		rs2::context ctx{};
		rs2::device_list deviceList = ctx.query_devices();
		
		std::set<std::string> devices;
		for (auto dev = deviceList.begin(); dev != deviceList.end(); ++dev)
		{
			devices.insert((*dev).get_info(RS2_CAMERA_INFO_SERIAL_NUMBER));
		}

		return devices;
	}



	RealSense(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : Camera(appStatus, configuration), realsensePipeline{}
	{
	}

	~RealSense()
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
			realsensePipeline.stop();
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

#endif // ENABLE_RS2


