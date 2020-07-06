#include "AzureKinect.h"

#ifdef ENABLE_K4A
#include <sstream>


// name used in logs
const char* AzureKinect::AzureKinectConstStr = "AzureKinect";


bool AzureKinect::OpenDefaultKinect()
{
	if (kinectDevice)
	{
		Logger::Log(AzureKinectConstStr) << "Device is already open!" << std::endl;
		return true;
	}

	try
	{
		kinectDevice = k4a::device::open(K4A_DEVICE_DEFAULT);
	}
	catch (const k4a::error& e)
	{
		Logger::Log(AzureKinectConstStr) << "Could not open default device..." << "(" << e.what() << ")" << std::endl;
		return false;
	}

	// let's update the serial number
	try
	{
		cameraSerialNumber = kinectDevice.get_serialnum();
	}
	catch (const k4a::error& e)
	{
		Logger::Log(AzureKinectConstStr) << "Could not open default device..." << "(" << e.what() << ")" << std::endl;
		cameraSerialNumber = "Unknown";
	}

	return true;
}

void AzureKinect::saveTransformationTable(int img_width, int img_height)
{

	k4a_image_t xy_image;


	k4a_image_create(K4A_IMAGE_FORMAT_CUSTOM,
		img_width, img_height,
		img_width * (int)sizeof(k4a_float2_t),
		&xy_image);

	k4a_float2_t* table_data = (k4a_float2_t*)(void*)k4a_image_get_buffer(xy_image);

	k4a_float2_t p;
	k4a_float3_t ray;
	bool valid;
	int y, idx;
	for (y = 0, idx = 0; y < img_height; ++y)
	{
		p.xy.y = (float)y;
		for (int x = 0; x < img_width; ++x, ++idx)
		{
			p.xy.x = (float)x;
			valid = kinectCameraCalibration.convert_2d_to_3d(p, 1.f, K4A_CALIBRATION_TYPE_COLOR, K4A_CALIBRATION_TYPE_COLOR, &ray);
			if (valid)
			{
				table_data[idx].xy.x = ray.xyz.x;
				table_data[idx].xy.y = ray.xyz.y;
			}
			else
			{
				table_data[idx].xy.x = 0.0; // nanf("");
				table_data[idx].xy.y = 0.0; // nanf("");
			}
		}
	}


	cv::Mat xy_image_matrix;
	xy_image_matrix.create(img_height, img_width, CV_32FC2);
	memcpy(xy_image_matrix.data, table_data, idx * sizeof(float) * 2);

	// create file name
	std::stringstream ss;

	// save xy table
	ss << "kinect_fastpointcloud_";
	ss << img_width;
	ss << 'x';
	ss << img_height;
	ss << ".json";

	cv::FileStorage fs(ss.str(), cv::FileStorage::WRITE);
	fs << "table" << xy_image_matrix;
	fs.release();

	k4a_image_release(xy_image);
}

void AzureKinect::CameraLoop()
{
	Logger::Log(AzureKinectConstStr) << "Started Azure Kinect polling thread: " << std::this_thread::get_id << std::endl;
	bool print_camera_parameters = true;
	bool didWeEverInitializeTheCamera = false;
	bool didWeCallConnectedCallback = false; // if the thread is stopped but we did execute the connected callback, then we will execute the disconnected callback to maintain consistency

	while (thread_running)
	{
		// start again ...
		didWeEverInitializeTheCamera = false;
		didWeCallConnectedCallback = false;

		//
		// Step #1) OPEN CAMERA
		//
		// stay in a loop until we can open the device and the cameras
		//
		while (thread_running && !IsAnyCameraEnabled())
		{

			// try reading configuration
			while (!LoadConfigurationSettings() && thread_running)
			{
				Logger::Log(AzureKinectConstStr) << "Trying again in 5 seconds..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}

			// try opening the device
			while (!OpenDefaultKinect() && thread_running)
			{
				// waits one second
				std::this_thread::sleep_for(std::chrono::seconds(1));
				Logger::Log(AzureKinectConstStr) << "Trying again..." << std::endl;
			}

			//  if we stop the application while waiting...
			if (!thread_running)
			{
				break;
			}

			Logger::Log(AzureKinectConstStr) << "Opened Azure Kinect device SSN: " << cameraSerialNumber << std::endl;

			// starting cameras based on configuration
			try
			{
				kinectDevice.start_cameras(&kinectConfiguration);
				colorCameraEnabled = configuration->IsColorCameraEnabled();
				depthCameraEnabled = configuration->IsDepthCameraEnabled();
			}
			catch (const k4a::error& error)
			{
				Logger::Log(AzureKinectConstStr) << "Error opening cameras!" << "(" << error.what() << ")" << std::endl; 
			}

			// loading camera configuration to memory
			if (IsAnyCameraEnabled())
			{
				try
				{
					kinectCameraCalibration    = kinectDevice.get_calibration(kinectConfiguration.depth_mode, kinectConfiguration.color_resolution);
					kinectCameraTransformation = k4a::transformation(kinectCameraCalibration);

					if (depthCameraEnabled)
					{
						depthCameraParameters.resolutionWidth = kinectCameraCalibration.depth_camera_calibration.resolution_width;
						depthCameraParameters.resolutionHeight = kinectCameraCalibration.depth_camera_calibration.resolution_height;
						depthCameraParameters.metricRadius = kinectCameraCalibration.depth_camera_calibration.metric_radius;
						depthCameraParameters.intrinsics.cx = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.cx;
						depthCameraParameters.intrinsics.cy = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.cy;
						depthCameraParameters.intrinsics.fx = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.fx;
						depthCameraParameters.intrinsics.fy = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.fy;
						depthCameraParameters.intrinsics.k1 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k1;
						depthCameraParameters.intrinsics.k2 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k2;
						depthCameraParameters.intrinsics.k3 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k3;
						depthCameraParameters.intrinsics.k4 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k4;
						depthCameraParameters.intrinsics.k5 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k5;
						depthCameraParameters.intrinsics.k6 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k6;
						depthCameraParameters.intrinsics.p1 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.p1;
						depthCameraParameters.intrinsics.p2 = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.p2;
						depthCameraParameters.intrinsics.metricRadius = kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.metric_radius;
					}

					if (colorCameraEnabled)
					{
						colorCameraParameters.resolutionWidth = kinectCameraCalibration.color_camera_calibration.resolution_width;
						colorCameraParameters.resolutionHeight = kinectCameraCalibration.color_camera_calibration.resolution_height;
						colorCameraParameters.metricRadius = kinectCameraCalibration.color_camera_calibration.metric_radius;
						colorCameraParameters.intrinsics.cx = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.cx;
						colorCameraParameters.intrinsics.cy = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.cy;
						colorCameraParameters.intrinsics.fx = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.fx;
						colorCameraParameters.intrinsics.fy = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.fy;
						colorCameraParameters.intrinsics.k1 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k1;
						colorCameraParameters.intrinsics.k2 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k2;
						colorCameraParameters.intrinsics.k3 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k3;
						colorCameraParameters.intrinsics.k4 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k4;
						colorCameraParameters.intrinsics.k5 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k5;
						colorCameraParameters.intrinsics.k6 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k6;
						colorCameraParameters.intrinsics.p1 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.p1;
						colorCameraParameters.intrinsics.p2 = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.p2;
						colorCameraParameters.intrinsics.metricRadius = kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.metric_radius;
					}
				}
				catch (const k4a::error& error)
				{
					Logger::Log(AzureKinectConstStr) << "Error obtaining camera parameter!" << "(" << error.what() << ")" << std::endl;
					kinectDevice.stop_cameras();
					colorCameraEnabled = false;
					depthCameraEnabled = false;
				}
			}

			// error openning all cameras?
			if (!IsAnyCameraEnabled())
			{
				// we have to close the device and try again
				kinectDevice.close();
				colorCameraEnabled = false;
				depthCameraEnabled = false;
				Logger::Log(AzureKinectConstStr) << "Trying again in 1 second..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			else {
				// reminder that we have to wrap things in the end, even if
				// no frames were captured
				didWeEverInitializeTheCamera = true;
			}
		}

		// save camera transformation table the first time this thread goes through this loop
		if (thread_running && colorCameraEnabled && depthCameraEnabled && print_camera_parameters)
		{
			saveTransformationTable(kinectCameraCalibration.color_camera_calibration.resolution_width, kinectCameraCalibration.color_camera_calibration.resolution_height);
			print_camera_parameters = false;
		}


		//
		// Step #2) START, LOOP FOR FRAMES, STOP
		//

		// start keeping track of incoming frames / failed frames
		statistics.StartCounting();

		// loop to capture frames
		if (thread_running && IsAnyCameraEnabled())
		{
			// time to start reading frames and streaming
			unsigned int triesBeforeRestart = 5;

			// updates app with capture and stream status
			appStatus->UpdateCaptureStatus(colorCameraEnabled, depthCameraEnabled, cameraSerialNumber,
				OpenCVCameraMatrix(colorCameraEnabled ? colorCameraParameters : depthCameraParameters),

				// color camera
				colorCameraEnabled ? colorCameraParameters.resolutionWidth : 0,
				colorCameraEnabled ? colorCameraParameters.resolutionHeight : 0,
				
				// depth camera
				depthCameraEnabled ? depthCameraParameters.resolutionWidth : 0,
				depthCameraEnabled ? depthCameraParameters.resolutionHeight : 0,

				// streaming (color resolution when  color is available, depth resolution otherwise)
				colorCameraEnabled ? colorCameraParameters.resolutionWidth : depthCameraParameters.resolutionWidth,
				colorCameraEnabled ? colorCameraParameters.resolutionHeight : depthCameraParameters.resolutionHeight);

			// starts
			Logger::Log(AzureKinectConstStr) << "Started capturing" << std::endl;

			// invokes camera connect callback
			if (thread_running && onCameraConnect)
			{
				didWeCallConnectedCallback = true; // we will need this later in case the thread is stopped
				onCameraConnect();
			}


			// capture loop
			try
			{

				while (thread_running)
				{
					k4a::capture currentCapture; // this object garantes a release at the end of the scope

					if (kinectDevice.get_capture(&currentCapture, getFrameTimeout))
					{
						std::shared_ptr<Frame> sharedColorFrame, sharedDepthFrame;
						std::chrono::microseconds timestamp;

						// capture color
						if (colorCameraEnabled)
						{
							// get color frame
							k4a::image colorFrame = currentCapture.get_color_image();
							timestamp = colorFrame.get_device_timestamp();

							// transform color to depth
							//k4a::image colorInDepthFrame = kinectCameraTransformation.color_image_to_depth_camera(depthFrame, colorFrame);

							// copies images to Frame
							sharedColorFrame = Frame::Create(colorFrame.get_width_pixels(), colorFrame.get_height_pixels(), FrameType::Encoding::BGRA32);
							memcpy(sharedColorFrame->data, colorFrame.get_buffer(), sharedColorFrame->size());
						}

						// get depth frame
						if  (depthCameraEnabled)
						{
							k4a::image depthFrame = currentCapture.get_depth_image();

							if (colorCameraEnabled)
							{
								k4a::image largeDepthFrame = kinectCameraTransformation.depth_image_to_color_camera(depthFrame);
								sharedDepthFrame = Frame::Create(largeDepthFrame.get_width_pixels(), largeDepthFrame.get_height_pixels(), FrameType::Encoding::Mono16);
								memcpy(sharedDepthFrame->data, largeDepthFrame.get_buffer(), sharedDepthFrame->size());
							}
							else {
								timestamp = depthFrame.get_device_timestamp();
								sharedDepthFrame = Frame::Create(depthFrame.get_width_pixels(), depthFrame.get_height_pixels(), FrameType::Encoding::Mono16);
								memcpy(sharedDepthFrame->data, depthFrame.get_buffer(), sharedDepthFrame->size());
							}
						}

						// invoke callback
						if (onFramesReady)
							onFramesReady(timestamp, sharedColorFrame, sharedDepthFrame);

						// update info
						++statistics.framesCaptured;
						triesBeforeRestart = 5;
					}
					else {
						Logger::Log(AzureKinectConstStr) << "Timed out while getting a frame..." << std::endl;
						--triesBeforeRestart;
						++statistics.framesFailed;

						if (triesBeforeRestart == 0)
						{
							Logger::Log(AzureKinectConstStr) << "Tried to get a frame 5 times but failed! Restarting system in 1 second..." << std::endl;
							std::this_thread::sleep_for(std::chrono::seconds(1));
							if (kinectDevice)
							{
								kinectDevice.stop_cameras();
								kinectDevice.close();
							}
							depthCameraEnabled = false;
							colorCameraEnabled = false;
							break; // breaks out of the loop
						}
					}
				}
			}
			catch (const k4a::error& e)
			{
				Logger::Log(AzureKinectConstStr) << "Fatal error getting frames... Restarting device in 5 seconds! (" << e.what() << ")" << std::endl;
				++statistics.framesFailed;
				if (kinectDevice)
				{
					kinectDevice.stop_cameras();
					kinectDevice.close();
				}

				depthCameraEnabled = false;
				colorCameraEnabled = false;
				appStatus->UpdateCaptureStatus(false, false);
				statistics.StopCounting();

				// waits 5 seconds before trying again
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}
			catch (const std::bad_alloc& e)
			{
				Logger::Log(AzureKinectConstStr) << "Fatal error! Running out of memory! Restarting device in 5 seconds! (" << e.what() << ")" << std::endl;
				++statistics.framesFailed;
				if (kinectDevice)
				{
					kinectDevice.stop_cameras();
					kinectDevice.close();
				}

				depthCameraEnabled = false;
				colorCameraEnabled = false;
				appStatus->UpdateCaptureStatus(false, false);
				statistics.StopCounting();

				// waits 5 seconds before trying again
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}

		}

		//
		// Step #3) Shutdown
		//

		// stop statistics
		statistics.StopCounting();

		// let other threads know that we are not capturing anymore
		appStatus->UpdateCaptureStatus(false, false);

		// stop cameras that might be running
		if (IsAnyCameraEnabled())
		{
			kinectDevice.stop_cameras();
			depthCameraEnabled = false;
			colorCameraEnabled = false;
		}

		// calls the camera disconnect callback if we called onCameraConnect() - consistency
		if (didWeCallConnectedCallback && onCameraDisconnect)
			onCameraDisconnect();
		
		// waits one second before restarting...
		if (thread_running)
		{
			Logger::Log(AzureKinectConstStr) << "Restarting device..." << std::endl;
		}

	}
}


bool AzureKinect::LoadConfigurationSettings()
{

	bool canRun30fps = true;

	// blank slate
	kinectConfiguration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;

	// first figure out which cameras have been loaded
	if (configuration->IsColorCameraEnabled())
	{
		kinectConfiguration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32; // for now, we force BGRA32

		const int requestedWidth = configuration->GetCameraColorWidth();
		const int requestedHeight = configuration->GetCameraColorHeight();

		appStatus->SetCameraColorWidth(requestedWidth);
		appStatus->SetCameraColorHeight(requestedHeight);
		
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
			Logger::Log(AzureKinectConstStr) << "Color camera Initialization Error! The requested resolution is not supported: " << requestedWidth << 'x' << requestedHeight << std::endl;
			kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_720P;
			appStatus->SetCameraColorHeight(720);
			appStatus->SetCameraColorWidth(1280);
			newWidth = 1280;
			break;
		}

		// adjusting requested resolution to match an accepted resolution
		if (requestedWidth != newWidth)
			appStatus->SetCameraColorWidth(newWidth);
		

	}
	else {
		kinectConfiguration.color_resolution = K4A_COLOR_RESOLUTION_OFF;
	}

	// then figure out depth
	if (configuration->IsDepthCameraEnabled())
	{

		//kinectConfiguration.depth_mode
		const int requestedWidth = configuration->GetCameraDepthWidth();
		const int requestedHeight = configuration->GetCameraDepthHeight();
		int newWidth = requestedWidth;

		appStatus->SetCameraDepthWidth(requestedWidth);
		appStatus->SetCameraDepthHeight(requestedHeight);

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
			Logger::Log(AzureKinectConstStr) << "Depth camera Initialization Error! The requested resolution is not supported: " << requestedWidth << 'x' << requestedHeight << std::endl;
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


	}
	else {
		// camera is off
		kinectConfiguration.depth_mode = K4A_DEPTH_MODE_OFF;
	}

	// if both cameras are enabled, we will make sure that frames are synchronized
	if (configuration->IsColorCameraEnabled() && configuration->IsDepthCameraEnabled())
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
		Logger::Log(AzureKinectConstStr) << "WARNING! The selected combination of depth and color resolution can run at a max of 15fps!" << std::endl;
	}

	// what about the device?
	if (!configuration->UseFirstCameraAvailable())
	{
		Logger::Log(AzureKinectConstStr) << "WARNING! We currently do not support selecting a camera based on serial number (Sorry!)" << std::endl;
	}

	return true;
}

#endif