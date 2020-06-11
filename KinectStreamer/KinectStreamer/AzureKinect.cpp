#include "AzureKinect.h"


bool AzureKinect::OpenDefaultKinect()
{
	if (kinectDevice)
	{
		Logger::Log("AzureKinect") << "Device is already open!" << std::endl;
		return true;
	}

	try
	{
		kinectDevice = k4a::device::open(K4A_DEVICE_DEFAULT);
	}
	catch (const k4a::error& e)
	{
		Logger::Log("AzureKinect") << "Could not open default device..." << std::endl;
		return false;
	}

	// let's update the serial number
	try
	{
		cameraSerialNumber = kinectDevice.get_serialnum();
	}
	catch (const k4a::error& e)
	{
		Logger::Log("AzureKinect") << "Could not open default device..." << std::endl;
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

	// save xy table
	std::string table_path = R"(xy_table.json)";
	cv::FileStorage fs(table_path, cv::FileStorage::WRITE);
	fs << "table" << xy_image_matrix;
	fs.release();

	k4a_image_release(xy_image);
}

void AzureKinect::CameraLoop()
{
	Logger::Log("AzureKinect") << "Started Azure Kinect polling thread: " << std::this_thread::get_id << std::endl;
	bool print_camera_parameters = true;
	bool didWeEverInitializeTheCamera = false;
	bool didWeCallConnectedCallback = false; // if the thread is stopped but we did execute the connected callback, then we will execute the disconnected callback to maintain consistency
	unsigned long long framesCaptured = 0;
	unsigned long long totalTries = 0;

	while (thread_running)
	{
		// start again ...
		didWeEverInitializeTheCamera = false;
		didWeCallConnectedCallback = false;
		framesCaptured = 0;
		totalTries = 0;

		//
		// Step #1) OPEN CAMERA
		//
		// stay in a loop until we can open the device and the cameras
		//
		while (thread_running && !runningCameras)
		{

			// try opening the device
			while (!OpenDefaultKinect() && thread_running)
			{
				// waits one second
				std::this_thread::sleep_for(std::chrono::seconds(1));
				Logger::Log("AzureKinect") << "Trying again..." << std::endl;
			}

			//  if we stop the application while waiting...
			if (!thread_running)
			{
				return;
			}

			Logger::Log("AzureKinect") << "Opened kinect device id: " << cameraSerialNumber << std::endl;

			// opening cameras
			try
			{
				kinectDevice.start_cameras(&kinectConfiguration);
				runningCameras = true;
			}
			catch (const k4a::error& error)
			{
				Logger::Log("AzureKinect") << "Error opening cameras!" << std::endl;
			}

			if (runningCameras)
			{
				try
				{
					kinectCameraCalibration = kinectDevice.get_calibration(kinectConfiguration.depth_mode, kinectConfiguration.color_resolution);
					kinectCameraTransformation = k4a::transformation(kinectCameraCalibration);
				}
				catch (const k4a::error& error)
				{
					Logger::Log("AzureKinect") << "Error obtaining camera parameter!" << std::endl;
					kinectDevice.stop_cameras();
					runningCameras = false;
				}
			}

			if (!runningCameras)
			{
				// we have to close the device and try again
				kinectDevice.close();
				Logger::Log("AzureKinect") << "Trying again in 1 second..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			else {
				didWeEverInitializeTheCamera = true;
			}
		}

		if (runningCameras && print_camera_parameters)
		{
			// printing out camera specifics
			Logger::Log("AzureKinect") << "[Depth] resolution width: " << kinectCameraCalibration.depth_camera_calibration.resolution_width << std::endl;
			Logger::Log("AzureKinect") << "[Depth] resolution height: " << kinectCameraCalibration.depth_camera_calibration.resolution_height << std::endl;
			Logger::Log("AzureKinect") << "[Depth] principal point x: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.cx << std::endl;
			Logger::Log("AzureKinect") << "[Depth] principal point y: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.cy << std::endl;
			Logger::Log("AzureKinect") << "[Depth] focal length x: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.fx << std::endl;
			Logger::Log("AzureKinect") << "[Depth] focal length y: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.fy << std::endl;
			Logger::Log("AzureKinect") << "[Depth] radial distortion coefficients:" << std::endl;
			Logger::Log("AzureKinect") << "[Depth] k1: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k1 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] k2: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k2 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] k3: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k3 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] k4: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k4 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] k5: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k5 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] k6: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.k6 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] center of distortion in Z=1 plane, x: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.codx << std::endl;
			Logger::Log("AzureKinect") << "[Depth] center of distortion in Z=1 plane, y: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.cody << std::endl;
			Logger::Log("AzureKinect") << "[Depth] tangential distortion coefficient x: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.p1 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] tangential distortion coefficient y: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.p2 << std::endl;
			Logger::Log("AzureKinect") << "[Depth] metric radius: " << kinectCameraCalibration.depth_camera_calibration.intrinsics.parameters.param.metric_radius << std::endl << std::endl;

			Logger::Log("AzureKinect") << "[Color] resolution width: " << kinectCameraCalibration.color_camera_calibration.resolution_width << std::endl;
			Logger::Log("AzureKinect") << "[Color] resolution height: " << kinectCameraCalibration.color_camera_calibration.resolution_height << std::endl;
			Logger::Log("AzureKinect") << "[Color] principal point x: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.cx << std::endl;
			Logger::Log("AzureKinect") << "[Color] principal point y: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.cy << std::endl;
			Logger::Log("AzureKinect") << "[Color] focal length x: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.fx << std::endl;
			Logger::Log("AzureKinect") << "[Color] focal length y: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.fy << std::endl;
			Logger::Log("AzureKinect") << "[Color] radial distortion coefficients:" << std::endl;
			Logger::Log("AzureKinect") << "[Color] k1: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k1 << std::endl;
			Logger::Log("AzureKinect") << "[Color] k2: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k2 << std::endl;
			Logger::Log("AzureKinect") << "[Color] k3: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k3 << std::endl;
			Logger::Log("AzureKinect") << "[Color] k4: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k4 << std::endl;
			Logger::Log("AzureKinect") << "[Color] k5: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k5 << std::endl;
			Logger::Log("AzureKinect") << "[Color] k6: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.k6 << std::endl;
			Logger::Log("AzureKinect") << "[Color] center of distortion in Z=1 plane, x: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.codx << std::endl;
			Logger::Log("AzureKinect") << "[Color] center of distortion in Z=1 plane, y: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.cody << std::endl;
			Logger::Log("AzureKinect") << "[Color] tangential distortion coefficient x: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.p1 << std::endl;
			Logger::Log("AzureKinect") << "[Color] tangential distortion coefficient y: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.p2 << std::endl;
			Logger::Log("AzureKinect") << "[Color] metric radius: " << kinectCameraCalibration.color_camera_calibration.intrinsics.parameters.param.metric_radius << std::endl << std::endl;

			// saves table
			saveTransformationTable(kinectCameraCalibration.color_camera_calibration.resolution_width, kinectCameraCalibration.color_camera_calibration.resolution_height);

			print_camera_parameters = false;
		}


		//
		// Step #2) START, LOOP FOR FRAMES, STOP
		//
		auto start = std::chrono::high_resolution_clock::now();
		if (didWeEverInitializeTheCamera)
		{
			// time to start reading frames and streaming
			unsigned int triesBeforeRestart = 5;
			totalTries = 0;

			// updates app with capture and stream status
			appStatus->UpdateCaptureStatus(true, true,
				kinectCameraCalibration.color_camera_calibration.resolution_width, kinectCameraCalibration.color_camera_calibration.resolution_height,
				kinectCameraCalibration.color_camera_calibration.resolution_width, kinectCameraCalibration.color_camera_calibration.resolution_height, // might change depending on config
				kinectCameraCalibration.depth_camera_calibration.resolution_width, kinectCameraCalibration.depth_camera_calibration.resolution_height,
				kinectCameraCalibration.color_camera_calibration.resolution_width, kinectCameraCalibration.color_camera_calibration.resolution_height);

			// starts
			Logger::Log("AzureKinect") << "Started streaming" << std::endl;

			// invokes the kinect started callback if the thread is running
			if (thread_running && onCameraConnect)
			{
				didWeCallConnectedCallback = true; // we will need this later in case the thread is stopped
				onCameraConnect();
			}

			try
			{

				while (thread_running)
				{
					k4a::capture currentCapture; // this object garantess a release at the end of the 

					if (kinectDevice.get_capture(&currentCapture, getFrameTimeout))
					{
						// get color frame
						k4a::image colorFrame = currentCapture.get_color_image();
						//colorFrame.get_i

						// get depth frame
						k4a::image depthFrame = currentCapture.get_depth_image();
						k4a::image largeDepthFrame = kinectCameraTransformation.depth_image_to_color_camera(depthFrame);

						// transform color to depth
						//k4a::image colorInDepthFrame = kinectCameraTransformation.color_image_to_depth_camera(depthFrame, colorFrame);


						// copies images to Frame
						std::shared_ptr<Frame> sharedColorFrame = Frame::Create(colorFrame.get_width_pixels(), colorFrame.get_height_pixels(), FrameType::Encoding::BGRA32);
						memcpy(sharedColorFrame->data, colorFrame.get_buffer(), sharedColorFrame->size());

						std::shared_ptr<Frame> sharedDepthFrame = Frame::Create(largeDepthFrame.get_width_pixels(), largeDepthFrame.get_height_pixels(), FrameType::Encoding::Mono16);
						memcpy(sharedDepthFrame->data, largeDepthFrame.get_buffer(), sharedDepthFrame->size());

						// invoke callback
						if (onFramesReady)
							onFramesReady(colorFrame.get_device_timestamp(), sharedColorFrame, sharedDepthFrame);

						// counts frames and restarts chances to get a new frame
						framesCaptured++;
						triesBeforeRestart = 5;
					}
					else {
						Logger::Log("AzureKinect") << "Timed out while getting a frame..." << std::endl;
						--triesBeforeRestart;
						++totalTries;

						if (triesBeforeRestart == 0)
						{
							Logger::Log("AzureKinect") << "Tried to get a frame 5 times but failed! Restarting system in 1 second..." << std::endl;
							std::this_thread::sleep_for(std::chrono::seconds(1));
							if (kinectDevice)
							{
								kinectDevice.stop_cameras();
								kinectDevice.close();
							}
							runningCameras = false;
							break; // breaks out of the loop
						}
					}
				}
			}
			catch (const k4a::error& e)
			{
				Logger::Log("AzureKinect") << "Fatal error getting frames... Restarting device in 5 seconds! (" << e.what() << ")" << std::endl;

				if (kinectDevice)
				{
					kinectDevice.stop_cameras();
					kinectDevice.close();
				}

				kinectDevice.stop_cameras();
				runningCameras = false;
				appStatus->UpdateCaptureStatus(false, false);

				// waits 5 seconds before trying again
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}
			catch (const std::bad_alloc& e)
			{
				Logger::Log("AzureKinect") << "Fatal error! Running out of memory! Restarting device in 5 seconds! (" << e.what() << ")" << std::endl;

				if (kinectDevice)
				{
					kinectDevice.stop_cameras();
					kinectDevice.close();
				}

				kinectDevice.stop_cameras();
				runningCameras = false;
				appStatus->UpdateCaptureStatus(false, false);

				// waits 5 seconds before trying again
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}

		}

		//
		// Step #3) Shutdown
		//
		if (didWeEverInitializeTheCamera)
		{
			// are we running cameras? stop them!
			appStatus->UpdateCaptureStatus(false, false);
			if (runningCameras)
			{
				kinectDevice.stop_cameras();
				runningCameras = false;
			}

			// report usage
			auto durationInSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count();
			Logger::Log("AzureKinect") << "Captured " << framesCaptured << " frames in " << durationInSeconds << " seconds (" << ((double)framesCaptured / (double)durationInSeconds) << " fps) - Timed out: " << totalTries << " times" << std::endl;

			// calls the camera disconnect callback if we called onCameraConnect()
			if (didWeCallConnectedCallback && onCameraDisconnect)
				onCameraDisconnect();
		}

		// waits one second before restarting...
		if (thread_running)
		{
			Logger::Log("AzureKinect") << "Restarting device..." << std::endl;
		}

	}
}
