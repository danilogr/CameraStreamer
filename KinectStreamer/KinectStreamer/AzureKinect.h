#pragma once

#include "Logger.h"
#include <k4a/k4a.hpp>
#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>

#include <opencv2/opencv.hpp>

#include "Frame.h"
#include "ApplicationStatus.h"

class AzureKinect
{
	std::shared_ptr<ApplicationStatus> appStatus;

	std::shared_ptr<std::thread> sThread;
	k4a::device kinectDevice;
	bool thread_running;
	bool runningCameras;

	k4a_device_configuration_t kinectConfiguration;
	k4a::calibration kinectCameraCalibration;
	k4a::transformation kinectCameraTransformation;

	k4a_calibration_intrinsic_parameters_t* intrinsics_color;
	k4a_calibration_intrinsic_parameters_t* intrinsics_depth;

	int currentExposure = 0;
	int currentGain = 0;
	std::chrono::milliseconds getFrameTimeout;

	std::string kinectDeviceSerial;

	// timestamp, color, depth
	typedef std::function<void(std::chrono::microseconds, std::shared_ptr<Frame>, std::shared_ptr<Frame>)> FrameReadyCallback;

public:

	FrameReadyCallback onFramesReady;

	AzureKinect(std::shared_ptr<ApplicationStatus> appStatus) : appStatus(appStatus), thread_running(false), runningCameras(false), kinectConfiguration(K4A_DEVICE_CONFIG_INIT_DISABLE_ALL),
		intrinsics_color(nullptr), intrinsics_depth(nullptr), getFrameTimeout(1000)
	{
	}

	~AzureKinect()
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
			kinectDeviceSerial.clear(); // erases serial

			if (sThread && sThread->joinable())
				sThread->join();

			// makes sure that sthread doesn't point to anything
			sThread = nullptr;
		}

		// frees resources
		if (runningCameras)
		{
			kinectDevice.stop_cameras();
			runningCameras = false;
		}

		if (kinectDevice.handle() != nullptr)
			kinectDevice.close();
	}

	void Run(k4a_device_configuration_t kinectConfig)
	{
		if (!thread_running && !sThread)
		{ 
			// saves the configuration
			kinectConfiguration = kinectConfig;

			// starts thread
			sThread.reset(new std::thread(std::bind(&AzureKinect::thread_main, this)));
		}
	}

	bool AdjustGainBy(int gain_level)
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
		}
		catch (const k4a::error & e)
		{
			Logger::Log("AzureKinect") << "Could not adjust gain level to : " << proposedGain << std::endl;
		}
	}

	bool AdjustExposureBy(int exposure_level)
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
		}
		catch (const k4a::error& e)
		{
			Logger::Log("AzureKinect") << "Could not adjust exposure level to : " << proposedExposure << std::endl;
		}
		
	}

	const std::string& getSerial()
	{
		return kinectDeviceSerial;
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
			kinectDeviceSerial = kinectDevice.get_serialnum();
		}
		catch (const k4a::error & e)
		{
			Logger::Log("AzureKinect") << "Could not open default device..." << std::endl;
			kinectDeviceSerial = "Unknown";
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

	void thread_main()
	{
		Logger::Log("AzureKinect") << "Started Azure Kinect polling thread: " << std::this_thread::get_id << std::endl;
		thread_running = true;
		bool print_camera_parameters = true;

		while (thread_running)
		{

			// stay in a loop until we can open the device and the cameras
			while (thread_running && !runningCameras)
			{

				// try opening the device
				while (!OpenDefaultKinect() && thread_running)
				{
					// waits one second
					std::this_thread::sleep_for(std::chrono::seconds(1));
					Logger::Log("AzureKinect") << "Trying again..." << std::endl;
				}

				if (!thread_running)
				{
					return;
				}

				Logger::Log("AzureKinect") << "Opened kinect device id: " << kinectDeviceSerial << std::endl;

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
			}

			if (print_camera_parameters)
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

			// time to start reading frames and streaming
			unsigned long long framesCaptured = 0;
			unsigned int triesBeforeRestart = 5;
			unsigned long long totalTries = 0;
			Logger::Log("AzureKinect") << "Started streaming" << std::endl;
			appStatus->isCameraRunning = true;
			auto start = std::chrono::high_resolution_clock::now();

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
							break;
						}
					}
				}
			}
			catch (const k4a::error& e)
			{
				Logger::Log("AzureKinect") << "Fatal error getting frames... Restarting device in 5 seconds." << std::endl;
				
				if (kinectDevice)
				{
					kinectDevice.stop_cameras();
					kinectDevice.close();
				}

				kinectDevice.stop_cameras();
				runningCameras = false;
				appStatus->isCameraRunning = false;

				// waits 5 seconds before trying again
				std::this_thread::sleep_for(std::chrono::seconds(5));
			} 

			// are we running cameras?
			if (runningCameras)
			{
				kinectDevice.stop_cameras();
				runningCameras = false;
				appStatus->isCameraRunning = false;
			}

			auto durationInSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - start).count();
			Logger::Log("AzureKinect") << "Captured " << framesCaptured << " frames in " << durationInSeconds << " seconds (" << ((double)framesCaptured / (double)durationInSeconds) << " fps) - Timed out: " << totalTries << " times" << std::endl;

			// waits one second before restarting...
			if (thread_running)
			{
				Logger::Log("AzureKinect") << "Restarting device..." << std::endl;
			}

		}
	}



};


