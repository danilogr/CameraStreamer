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

// our framework
#include "Logger.h"
#include "ApplicationStatus.h"
#include "Frame.h"
#include "Camera.h"

// real sense sdk
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

	std::shared_ptr<rs2::decimation_filter> dec_filter; // Decimation - reduces depth frame density
	std::shared_ptr<rs2::spatial_filter> spat_filter;   // Spatial    - edge-preserving spatial smoothing
	std::shared_ptr<rs2::temporal_filter> temp_filter;  // Temporal   - reduces temporal noise
	std::shared_ptr<rs2::disparity_transform> depth_to_disparity;
	std::shared_ptr<rs2::disparity_transform> disparity_to_depth;

};

#endif // ENABLE_RS2


