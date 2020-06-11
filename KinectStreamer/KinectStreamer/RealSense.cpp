#include "RealSense.h"

// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_RS2

bool RealSense::SetCameraConfigurationFromAppStatus()
{

	// blank slate
	rs2Configuration.disable_all_streams();

	// get current list of devices
	const std::set<std::string>& devicesConnected = RealSense::ListDevices();

	// do we have a specific serial number we are looking for?
	if (!appStatus->UseFirstCameraAvailable())
	{
		if (devicesConnected.find(appStatus->GetCameraSN()) == devicesConnected.cend())
		{
			Logger::Log("RealSense2") << "ERROR! Selected device \"" << appStatus->GetCameraSN() << "\" not available!" << std::endl;
			return false; // we cannot test configuration if the device is not available :(
		}

		// device is available! yay
		rs2Configuration.enable_device(appStatus->GetCameraSN());
	}
	else {
		if (devicesConnected.size() == 0)
		{
			Logger::Log("RealSense2") << "ERROR! No devices available...." << std::endl;
			return false;
		}

		// get the first device available
		cameraSerialNumber = *devicesConnected.cbegin();
		rs2Configuration.enable_device(cameraSerialNumber);
		
	}

	// first figure out which cameras have been loaded
	if (true || appStatus->IsColorCameraEnabled())
	{
		rs2Configuration.enable_stream(RS2_STREAM_COLOR, appStatus->GetCameraColorWidth(), appStatus->GetCameraColorHeight(), RS2_FORMAT_BGR8, 30);
	}
	else {
		rs2Configuration.disable_stream(RS2_STREAM_COLOR);
	}

	// then figure out depth
	if (true || appStatus->IsDepthCameraEnabled())
	{
		const int requestedWidth = appStatus->GetCameraDepthWidth();
		const int requestedHeight = appStatus->GetCameraDepthHeight();
		rs2Configuration.enable_stream(RS2_STREAM_DEPTH, appStatus->GetCameraDepthWidth(), appStatus->GetCameraDepthHeight(), RS2_FORMAT_Z16, 30);
	}
	else {
		// camera is off
		rs2Configuration.disable_stream(RS2_STREAM_DEPTH);
	}

	// now, let's make sure that this configuration is valid!
	if (!rs2Configuration.can_resolve(realsensePipeline))
	{
		Logger::Log("RealSense2") << "ERROR! Could not initialize a device with the provided settings!" << std::endl;
		cameraSerialNumber.empty();
		return false;
	}
}

void RealSense::CameraLoop()
{
	Logger::Log("RealSense2") << "Started Real Sense polling thread: " << std::this_thread::get_id << std::endl;

	bool didWeEverInitializeTheCamera = false;
	// if the thread is stopped but we did execute the connected callback,
	// then we will execute the disconnected callback to maintain consistency
	bool didWeCallConnectedCallback = false; 
	unsigned long long totalTries = 0;

	while (thread_running)
	{
		// start again ...
		didWeEverInitializeTheCamera = false;
		didWeCallConnectedCallback = false;
		totalTries = 0;

		//
		// Step #1) OPEN CAMERA
		//
		// stay in a loop until we can open the device and the cameras
		// (or until someone requests thread to exit)
		//
		while (thread_running && !runningCameras)
		{
			// start with camera configuration
			while (!SetCameraConfigurationFromAppStatus() && thread_running)
			{
				Logger::Log("RealSense2") << "Trying again in 5 seconds..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}

			//  if we stop the application while waiting...
			if (!thread_running)
			{
				break;
			}


			// tries to start the streaming pipeline
			while (!runningCameras && thread_running)
			{
				try
				{
					realsensePipeline.start(rs2Configuration);
					runningCameras = true;
				}
				catch (const rs2::camera_disconnected_error& e)
				{
					Logger::Log("RealSense2") << "ERROR! Camera is not connected! Please, connect the camera! Trying again in 5 seconds..." << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(5));
				}
				catch (const std::runtime_error& e)
				{
					Logger::Log("RealSense2") << "ERROR! Could not start the camera: " << e.what()  << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(5));
				}
			}

			//  if we stop the application while waiting...
			if (!thread_running)
			{
				break;
			}


			Logger::Log("RealSense2") << "Opened RealSense device id: " << cameraSerialNumber << " connected through " << realsensePipeline.get_active_profile().get_device().get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR) << std::endl;


		}
	}

}

#endif