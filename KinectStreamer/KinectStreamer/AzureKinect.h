#pragma once

#include "Logger.h"
#include <k4a/k4a.hpp>
#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>

#include "Frame.h"

class AzureKinect
{
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
	std::chrono::milliseconds getFrameTimeout;

	std::string kinectDeviceSerial;

	// timestamp, color, depth
	typedef std::function<void(std::chrono::microseconds, std::shared_ptr<Frame>, std::shared_ptr<Frame>)> FrameReadyCallback;

public:

	FrameReadyCallback onFramesReady;

	AzureKinect() : thread_running(false), runningCameras(false), kinectConfiguration(K4A_DEVICE_CONFIG_INIT_DISABLE_ALL),
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

	void Stop()
	{
		thread_running = false;     // stops the loop in case it is running
		kinectDeviceSerial.clear(); // erases serial

		if (sThread && sThread->joinable())
			sThread->join();

		// frees resources
		if (runningCameras)
			kinectDevice.stop_cameras();

		if (kinectDevice.handle() != nullptr)
			kinectDevice.close();
	}

	void Run(k4a_device_configuration_t kinectConfig)
	{
		// saves the configuration
		kinectConfiguration = kinectConfig;

		// starts thread
		sThread.reset(new std::thread(std::bind(&AzureKinect::thread_main, this)));
	}

	bool AdjustExposureBy(int exposure_level)
	{
		int prosedExposure = currentExposure + exposure_level;
		if (prosedExposure > 1)
		{
			prosedExposure = 1;
		}
		else if (prosedExposure < -11)
		{
			prosedExposure = -11;
		}

		try
		{
			kinectDevice.set_color_control(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, K4A_COLOR_CONTROL_MODE_MANUAL, static_cast<int32_t>(exp2f((float)prosedExposure) *
				1000000.0f));
			currentExposure = prosedExposure;
			Logger::Log("AzureKinect") << "Exposure level: " << currentExposure << std::endl;
		}
		catch (const k4a::error& e)
		{
			Logger::Log("AzureKinect") << "Could not adjust exposure level to : " << prosedExposure << std::endl;
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
				while (!OpenDefaultKinect())
				{
					// waits one second
					std::this_thread::sleep_for(std::chrono::seconds(1));
					Logger::Log("AzureKinect") << "Trying again..." << std::endl;
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
				print_camera_parameters = false;
			}

			// time to start reading frames and streaming
			unsigned long long framesCaptured = 0;
			unsigned int triesBeforeRestart = 5;
			unsigned long long totalTries = 0;
			Logger::Log("AzureKinect") << "Started streaming" << std::endl;
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
				std::this_thread::sleep_for(std::chrono::seconds(5));
				if (kinectDevice)
				{
					kinectDevice.stop_cameras();
				}
				kinectDevice.close();
				runningCameras = false;
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


