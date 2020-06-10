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

	bool OpenDefaultKinect()
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
		catch (const k4a::error & e)
		{
			Logger::Log("AzureKinect") << "Could not open default device..." << std::endl;
			cameraSerialNumber = "Unknown";
		}

		return true;
	}

	
	void saveTransformationTable(int img_width, int img_height)
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
					table_data[idx].xy.x = nanf("");
					table_data[idx].xy.y = nanf("");
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

	virtual void CameraLoop()
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
				catch (const k4a::error & error)
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
					catch (const k4a::error & error)
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
				catch (const k4a::error & e)
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



};


