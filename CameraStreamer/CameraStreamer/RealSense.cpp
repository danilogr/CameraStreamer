#include "RealSense.h"

// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef CS_ENABLE_CAMERA_RS2

// includes that only get compiled if the camera is enabled
#include <sstream>

// name used in logs
const char* RealSense::RealSenseConstStr = "RealSense2";

bool RealSense::LoadConfigurationSettings()
{
	// makes sure to invoke base class implementation of settings
	if (Camera::LoadConfigurationSettings())
	{

		// blank slate
		rs2Configuration.disable_all_streams();

		// get current list of devices
		std::set<std::string> devicesConnected = RealSense::ListDevices();

		// do we have a specific serial number we are looking for?
		if (!configuration->UseFirstCameraAvailable())
		{
			if (devicesConnected.find(configuration->GetCameraSN()) == devicesConnected.cend())
			{
				Logger::Log(RealSenseConstStr) << "ERROR! Selected device \"" << configuration->GetCameraSN() << "\" not available!" << std::endl;

				std::stringstream deviceList;
				int lastDevice = devicesConnected.size();
				for (const std::string& sn : devicesConnected)
				{
					lastDevice--;
					deviceList << sn << (lastDevice ? ',' : '.');
				}

				Logger::Log(RealSenseConstStr) << "Devices connected: " << deviceList.str() << std::endl;


				return false; // we cannot test configuration if the device is not available :(
			}

			// device is available! yay
			rs2Configuration.enable_device(configuration->GetCameraSN());
		}
		else {
			if (devicesConnected.size() == 0)
			{
				Logger::Log(RealSenseConstStr) << "ERROR! No devices available...." << std::endl;
				return false;
			}

			// get the first device available
			cameraSerialNumber = *devicesConnected.cbegin();
			rs2Configuration.enable_device(cameraSerialNumber);

		}

		// first figure out which cameras have been loaded
		if (configuration->IsColorCameraEnabled())
		{
			rs2Configuration.enable_stream(RS2_STREAM_COLOR, configuration->GetCameraColorWidth(), configuration->GetCameraColorHeight(), RS2_FORMAT_BGR8, 30);
		}
		else {
			rs2Configuration.disable_stream(RS2_STREAM_COLOR);
		}

		// then figure out depth
		if (configuration->IsDepthCameraEnabled())
		{
			rs2Configuration.enable_stream(RS2_STREAM_DEPTH, configuration->GetCameraDepthWidth(), configuration->GetCameraDepthHeight(), RS2_FORMAT_Z16, 30);
		}
		else {
			// camera is off
			rs2Configuration.disable_stream(RS2_STREAM_DEPTH);
		}

		// now, let's make sure that this configuration is valid!
		if (!rs2Configuration.can_resolve(realsensePipeline))
		{
			Logger::Log(RealSenseConstStr) << "ERROR! Could not initialize a device with the provided settings!" << std::endl;
			cameraSerialNumber = std::string();
			return false;
		}


		if (configuration->IsDepthCameraEnabled())
		{
			// add filters (todo: make them configurable)
			dec_filter = std::make_shared<rs2::decimation_filter>(1.0f); // Decimation - reduces depth frame density
			spat_filter = std::make_shared<rs2::spatial_filter>();       // Spatial    - edge-preserving spatial smoothing
			temp_filter = std::make_shared<rs2::temporal_filter>();      // Temporal   - reduces temporal noise

			depth_to_disparity = std::make_shared<rs2::disparity_transform>(true);
			disparity_to_depth = std::make_shared<rs2::disparity_transform>(false);
		}

		return true;
	}
	return false;
}

void RealSense::CameraLoop()
{
	Logger::Log(RealSenseConstStr) << "Started Real Sense polling thread: " << std::this_thread::get_id << std::endl;

	// if the thread is stopped but we did execute the connected callback,
	// then we will execute the disconnected callback to maintain consistency
	bool didWeCallConnectedCallback = false; 
	unsigned long long totalTries = 0;

	// align to color filter
	rs2::align align_to_color(RS2_STREAM_COLOR);


	while (thread_running)
	{
		// start again ...
		didWeCallConnectedCallback = false;
		totalTries = 0;
		device = nullptr;

		//
		// Step #1) OPEN CAMERA
		//
		// stay in a loop until we can open the device and the cameras
		// (or until someone requests thread to exit)
		//
		while (thread_running && !IsAnyCameraEnabled())
		{

			// start with camera configuration
			while (!LoadConfigurationSettings() && thread_running)
			{
				Logger::Log(RealSenseConstStr) << "Trying again in 5 seconds..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}

			//  if we stop the application while waiting...
			if (!thread_running)
			{
				break;
			}

			// repeats until a camera is running or the application  stops
			while (!IsAnyCameraEnabled() && thread_running)
			{
				try
				{
					realsensePipeline.start(rs2Configuration);
					
					// get device
					device = std::make_shared<rs2::device>(realsensePipeline.get_active_profile().get_device());

					// sanity check
					if (!device)
						throw std::runtime_error("device is a nullptr");

					// get camera intrinsics
					auto streams = realsensePipeline.get_active_profile().get_streams();

					colorCameraEnabled = configuration->IsColorCameraEnabled();
					depthCameraEnabled = configuration->IsDepthCameraEnabled();
					bool foundColorCamera = false, foundDepthCamera = false;

					// read all incoming streams
					for (const auto& stream : streams)
					{
						if (stream.stream_type() == RS2_STREAM_DEPTH || stream.stream_type() == RS2_STREAM_COLOR)
						{
							if (stream.stream_type() == RS2_STREAM_DEPTH)
							{
								foundDepthCamera = true;
								// Get depth scale which is used to convert the measurements into millimeters
								depthCameraParameters.intrinsics.metricScale = realsensePipeline.get_active_profile().get_device().first<rs2::depth_sensor>().get_depth_scale();
							}

							if (stream.stream_type() == RS2_STREAM_COLOR)
								foundColorCamera = true;

							if ((colorCameraEnabled && foundColorCamera) || (depthCameraEnabled && foundDepthCamera))
							{
								auto intrinsics = stream.as<rs2::video_stream_profile>().get_intrinsics();
								CameraParameters &cameraParameters = (stream.stream_type() == RS2_STREAM_DEPTH) ? depthCameraParameters : colorCameraParameters;

								cameraParameters.intrinsics.fx = intrinsics.fx;
								cameraParameters.intrinsics.fy = intrinsics.fy;
								cameraParameters.intrinsics.cx = intrinsics.ppx;  
								cameraParameters.intrinsics.cy = intrinsics.ppy;

								cameraParameters.intrinsics.k1 = intrinsics.coeffs[0];
								cameraParameters.intrinsics.k2 = intrinsics.coeffs[1];
								cameraParameters.intrinsics.k3 = intrinsics.coeffs[2];
								cameraParameters.intrinsics.k4 = intrinsics.coeffs[3];
								cameraParameters.intrinsics.k5 = intrinsics.coeffs[4];
								//cameraParameters.intrinsics.k6 = intrinsics.coeffs[5];

								cameraParameters.resolutionHeight = intrinsics.height;
								cameraParameters.resolutionWidth  = intrinsics.width;
							}
						}

					}

					if (colorCameraEnabled && !foundColorCamera)
						throw std::runtime_error("Could not enable color camera! Check configuration! Can your device/connection support the requested settings?");

					if (depthCameraEnabled && !foundDepthCamera)
						throw std::runtime_error("Could not enable depth camera! Check configuration! Can your device/connection support the requested settings?");


					// update camera serial number based on selected camera
					cameraSerialNumber = device->get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
				}
				catch (const rs2::camera_disconnected_error& e)
				{
					Logger::Log(RealSenseConstStr) << "ERROR! Camera is not connected! Please, connect the camera! Trying again in 5 seconds..." << std::endl;
					device = nullptr;
					colorCameraEnabled = false;
					depthCameraEnabled = false;
					Logger::Log(RealSenseConstStr) << "Trying again in 5 seconds..." << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(5));
				}
				catch (const std::runtime_error& e)
				{
					Logger::Log(RealSenseConstStr) << "ERROR! Could not start the camera: " << e.what()  << std::endl;
					Logger::Log(RealSenseConstStr) << "Trying again in 1 second..." << std::endl;
					device = nullptr;
					colorCameraEnabled = false;
					depthCameraEnabled = false;
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
				
			}

			Logger::Log(RealSenseConstStr) << "Opened RealSense device id: " << cameraSerialNumber << std::endl;
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
			totalTries = 0;

			// updates app with capture and stream status
			appStatus->UpdateCaptureStatus(colorCameraEnabled, depthCameraEnabled, cameraSerialNumber,
				OpenCVCameraMatrix(colorCameraEnabled? colorCameraParameters : depthCameraParameters),

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
			Logger::Log(RealSenseConstStr) << "Started capturing" << std::endl;

			// invokes camera connect callback
			didWeCallConnectedCallback = true; // we will need this later in case the thread is stopped
			if (onCameraConnect)
				onCameraConnect();

			// capture loop
			while (thread_running)
			{
				try
				{
					while (thread_running)
					{
						rs2::frameset capture = realsensePipeline.wait_for_frames(getFrameTimeoutMSInt);

						std::shared_ptr<Frame> sharedColorFrame, sharedDepthFrame;

						// todo: get camera timestamp (so that we can syncronize cameras through their hardware)
						// timestamp = colorFrame.get_device_timestamp();
						std::chrono::microseconds timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

						// align both frames to color
						if (colorCameraEnabled && depthCameraEnabled)
						{
							capture = align_to_color.process(capture);
						}


						// capture color
						if (colorCameraEnabled)
						{
							// get color frame
							rs2::video_frame colorFrame = capture.get_color_frame();

							// copies image to our very own frame
							sharedColorFrame = Frame::Create(colorFrame.get_width(), colorFrame.get_height(), FrameType::Encoding::BGR24);
							if (!sharedColorFrame)
								throw std::bad_alloc();

							assert(colorFrame.get_data_size() == sharedColorFrame->size()); // sanity check for debugging
							memcpy(sharedColorFrame->data, colorFrame.get_data(), sharedColorFrame->size());
						}

						// get depth frame
						if (depthCameraEnabled)
						{
							rs2::depth_frame depthFrame = capture.get_depth_frame();
							rs2::depth_frame filteredDepthFrame = depthFrame;

							// filters
							filteredDepthFrame = dec_filter->process(filteredDepthFrame);
							filteredDepthFrame = depth_to_disparity->process(filteredDepthFrame);
							filteredDepthFrame = spat_filter->process(filteredDepthFrame);
							filteredDepthFrame = temp_filter->process(filteredDepthFrame);
							filteredDepthFrame = disparity_to_depth->process(filteredDepthFrame);

							sharedDepthFrame = Frame::Create(depthFrame.get_width(), depthFrame.get_height(), FrameType::Encoding::Mono16);
							if (!sharedDepthFrame)
								throw std::bad_alloc();

							assert(depthFrame.get_data_size() == sharedDepthFrame->size()); // sanity check for debugging
							memcpy(sharedDepthFrame->data, depthFrame.get_data(), sharedDepthFrame->size());
						}

						// invoke callback
						if (onFramesReady)
							onFramesReady(timestamp, sharedColorFrame, sharedDepthFrame, sharedDepthFrame);

						// update info
						++statistics.framesCaptured;
						triesBeforeRestart = 5;
					}

				}
				catch (const rs2::camera_disconnected_error& e)
				{
					++statistics.framesFailed;
					Logger::Log(RealSenseConstStr) << "Error! Camera disconnected!... Trying again in 1 second! (" << e.what() << ")" << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
				catch (const rs2::recoverable_error& e)
				{
					++statistics.framesFailed;
					--triesBeforeRestart;

					if (triesBeforeRestart == 0)
					{
						Logger::Log(RealSenseConstStr) << "Tried to get a frame 5 times but failed ("<< e.what() << ")! Restarting system in 1 second..." << std::endl;
						std::this_thread::sleep_for(std::chrono::seconds(1));
						
						if (IsAnyCameraEnabled())
						{
							realsensePipeline.stop();
						}

						depthCameraEnabled = false;
						colorCameraEnabled = false;

						break; // breaks out of the loop
					}
					else {
						Logger::Log(RealSenseConstStr) << "Error! " << e.what() << "!! Trying again in 1 second!" << std::endl;
						std::this_thread::sleep_for(std::chrono::seconds(1));
					}

				}
				catch (const std::runtime_error& e)
				{
					++statistics.framesFailed;
					Logger::Log(RealSenseConstStr) << "Fatal Error! " << e.what() << "!! Restarting system in 5 seconds..." << std::endl;

					if (IsAnyCameraEnabled())
					{
						realsensePipeline.stop();
					}

					depthCameraEnabled = false;
					colorCameraEnabled = false;

					break; // breaks out of the loop
				}
				catch (const std::bad_alloc& e)
				{
					++statistics.framesFailed;
					Logger::Log(RealSenseConstStr) << "FATAL ERROR! No memory left! Restarting device in 10 seconds! (" << e.what() << ")" << std::endl;

					if (IsAnyCameraEnabled())
					{
						realsensePipeline.stop();
					}

					depthCameraEnabled = false;
					colorCameraEnabled = false;
					appStatus->UpdateCaptureStatus(false, false);
					statistics.StopCounting();

					// waits 5 seconds before trying again
					std::this_thread::sleep_for(std::chrono::seconds(10));

					// breaks loop
					break;
				}
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
			realsensePipeline.stop();
			depthCameraEnabled = false;
			colorCameraEnabled = false;
		}

		// calls the camera disconnect callback if we called onCameraConnect() - consistency
		if (didWeCallConnectedCallback && onCameraDisconnect)
			onCameraDisconnect();

		// waits one second before restarting...
		if (thread_running)
		{
			Logger::Log(RealSenseConstStr) << "Restarting device..." << std::endl;
		}


	}

}

#endif