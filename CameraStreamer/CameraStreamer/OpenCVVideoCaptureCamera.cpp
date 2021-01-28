#include "OpenCVVideoCaptureCamera.h"


// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef CS_ENABLE_CAMERA_CV_VIDEOCAPTURE

// name used in logs
const char* CVVideoCaptureCamera::CVVideoCaptureCameraStr = "cv2::VideoCapture";


bool CVVideoCaptureCamera::LoadConfigurationSettings()
{
	// future work - match camera serial against available devices
	if (!configuration->UseFirstCameraAvailable())
	{
		Logger::Log(CVVideoCaptureCameraStr) << "Warning: Ignoring camera serial number defined as \"" << configuration->GetCameraSN() << "\" because this feature is not supported (yet) !" << std::endl;
	}

	// reads the URL or camera index
	cameraIndex = configuration->GetCameraCustomInt("index", -1, false);
	url = configuration->GetCameraCustomString("url", "", false);

	// user didn't set anything!
	if (cameraIndex <= -1 && url.empty())
	{
		// this means that we are connecting to the first webcam available
		Logger::Log(CVVideoCaptureCameraStr) << "Warning: neither camera.index nor camera.url are set. Using camera.index = 0 (first webcam available)" << std::endl;
		
		// sets webcam to zero
		cameraIndex = 0;
	}

	// one or the other are set
	if (cameraIndex >= 0)
	{
		usingWebcam = true;
	}


	if (usingWebcam)
		Logger::Log(CVVideoCaptureCameraStr) << "Opening webcam at index: "<< cameraIndex << std::endl;
	else
		Logger::Log(CVVideoCaptureCameraStr) << "Opening URI: " << url << std::endl;
	// else, url is set, so usingWebcam is not set
	return true;
}

void CVVideoCaptureCamera::CameraLoop()
{
	Logger::Log(CVVideoCaptureCameraStr) << "Started OpenCV's VideoCapture polling thread: " << std::this_thread::get_id << std::endl;

	// if the thread is stopped but we did execute the connected callback,
	// then we will execute the disconnected callback to maintain consistency
	bool didWeCallConnectedCallback = false;
	unsigned long long totalTries = 0;
	int cameraFrameWidth = 0, cameraFrameHeight = 0;
	double cameraFrameRate = 0;
	
	
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
				Logger::Log(CVVideoCaptureCameraStr) << "Trying again in 5 seconds..." << std::endl;
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

					// get device
					if (usingWebcam)
						device = std::make_shared<cv::VideoCapture>(cameraIndex);
					else
						device = std::make_shared<cv::VideoCapture>(url);

					// sanity check
					if (!device)
						throw std::runtime_error("device is a nullptr");

					if (!device->isOpened())
						throw std::runtime_error("Could not open camera / URI!");


					// this is not a webcam or network stream - it is a file / set of files
					frameCount = device->get(cv::CAP_PROP_FRAME_COUNT);
					if (frameCount > 0)
					{
						usingFile = true;
						usingWebcam = false;
					}

					// get current camera settings in opencv
					cameraFrameWidth = device->get(cv::CAP_PROP_FRAME_WIDTH);
					cameraFrameHeight = device->get(cv::CAP_PROP_FRAME_HEIGHT);
					cameraFrameRate = device->get(cv::CAP_PROP_FPS);


					// get camera settinsg
					int cameraWidth = configuration->GetCameraColorWidth(),
						camareHeight = configuration->GetCameraColorHeight(),
						cameraFPS = configuration->GetCameraColorFPS();


					if (!usingFile)
					{

						// let's set width and height
						device->set(cv::CAP_PROP_FRAME_WIDTH, cameraWidth);
						device->set(cv::CAP_PROP_FRAME_HEIGHT, camareHeight);

					}


					//
					// to be continued here:
					// https://docs.opencv.org/4.4.0/d8/dfe/classcv_1_1VideoCapture.html#ae38c2a053d39d6b20c9c649e08ff0146
					// https://docs.opencv.org/4.4.0/d8/dfe/classcv_1_1VideoCapture.html#a9ac7f4b1cdfe624663478568486e6712
					// https://docs.opencv.org/4.4.0/d6/ddf/samples_2cpp_2laplace_8cpp-example.html#a6
					// https://docs.opencv.org/4.4.0/d4/d15/group__videoio__flags__base.html#ggaeb8dd9c89c10a5c63c139bf7c4f5704daf01bc92359d2abc9e6eeb5cbe36d9af2

					//
					// what is next here?
					// two workflows: control frame rate when reading from files and capturing as many frames as possible when reading from the webcam
					//

					//device->set(cv::CAP_PROP_FRAME_COUNT, cameraFPS);



					// camera has opened, let's set the settings that we need
					colorCameraEnabled = configuration->IsColorCameraEnabled();
					colorCameraParameters.resolutionWidth = cameraWidth;
					colorCameraParameters.resolutionHeight = camareHeight;
					colorCameraParameters.frameRate = cameraFPS;
					

					

					// update camera serial number based on selected camera
					cameraSerialNumber = device->get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
					
				}
				catch (const cv::Exception& e) // opencv exception
				{
					Logger::Log(CVVideoCaptureCameraStr) << "ERROR! Could not start the camera: " << e.what() << "\n at " << e.file << ":" << e.line << std::endl;
					device = nullptr;
					colorCameraEnabled = false;
					depthCameraEnabled = false;
					Logger::Log(CVVideoCaptureCameraStr) << "Trying again in 5 seconds..." << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(5));
				}
				catch (const std::runtime_error& e) // our own exception
				{
					Logger::Log(CVVideoCaptureCameraStr) << "ERROR! Could not start the camera: " << e.what() << std::endl;
					Logger::Log(CVVideoCaptureCameraStr) << "Trying again in 1 second..." << std::endl;
					device = nullptr;
					colorCameraEnabled = false;
					depthCameraEnabled = false;
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}

			}

			Logger::Log(RealSenseConstStr) << "Opened RealSense device id: " << cameraSerialNumber << std::endl;

		}
	}


}


#endif