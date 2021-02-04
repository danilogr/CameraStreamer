#include "OpenCVVideoCaptureCamera.h"


// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef CS_ENABLE_CAMERA_CV_VIDEOCAPTURE

// name used in logs
const char* CVVideoCaptureCamera::CVVideoCaptureCameraStr = "VideoCapture";


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
		forcedshow = configuration->GetCameraCustomBool("forceDSHOW", true, false);
	}
	else {
		forcedshow = configuration->GetCameraCustomBool("forceDSHOW", false, false);
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
	int cvDeviceFrameWidth = 0, cvDeviceFrameHeight = 0;
	double cvDeviceFrameRate = 0;
	int currentFrame = 0;
	cv::Mat videoFrame;
	
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
						device = std::make_shared<cv::VideoCapture>(cameraIndex, forcedshow ? cv::CAP_DSHOW : cv::CAP_ANY);
					else
						device = std::make_shared<cv::VideoCapture>(url, forcedshow ? cv::CAP_DSHOW : cv::CAP_ANY);

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
					cvDeviceFrameWidth = device->get(cv::CAP_PROP_FRAME_WIDTH);
					cvDeviceFrameHeight = device->get(cv::CAP_PROP_FRAME_HEIGHT);
					cvDeviceFrameRate = device->get(cv::CAP_PROP_FPS);


					// get camera settinsg
					int cameraWidth = configuration->GetCameraColorWidth(),
						cameraHeight = configuration->GetCameraColorHeight(),
						cameraFPS = configuration->GetCameraColorFPS();


					//
					// breaks down into two configurations
					// camera or file
					//

					// if we are not using a file, it could be a network camera or a local camera
					if (!usingFile)
					{

						// let's set width and height
						if (cameraWidth != cvDeviceFrameWidth)
							device->set(cv::CAP_PROP_FRAME_WIDTH, cameraWidth);

						if (cameraHeight != cvDeviceFrameHeight)
							device->set(cv::CAP_PROP_FRAME_HEIGHT, cameraHeight);

						if (cameraFPS != cvDeviceFrameRate)
							device->set(cv::CAP_PROP_FPS, cameraFPS);

						// did we manage to change anything ?
						cvDeviceFrameWidth = device->get(cv::CAP_PROP_FRAME_WIDTH);
						cvDeviceFrameHeight = device->get(cv::CAP_PROP_FRAME_HEIGHT);
						cvDeviceFrameRate = device->get(cv::CAP_PROP_FPS);

						if (cameraWidth != cvDeviceFrameWidth || cameraHeight != cvDeviceFrameHeight || cameraFPS != cvDeviceFrameRate)
						{
							Logger::Log(CVVideoCaptureCameraStr) << "Requested " << cameraWidth << "x" << cameraHeight << " at " << cameraFPS
								<< " but got " << cvDeviceFrameWidth << "x" << cvDeviceFrameHeight << " at " << cvDeviceFrameRate << std::endl;
						}

					}


					// camera has opened, let's set the settings that we need
					colorCameraEnabled = configuration->IsColorCameraEnabled();
					colorCameraParameters.resolutionWidth = cvDeviceFrameWidth;
					colorCameraParameters.resolutionHeight = cvDeviceFrameHeight;
					colorCameraParameters.frameRate = cvDeviceFrameRate;

					// todo: come up with something less arbitrary ? :P
					if (usingWebcam)
					{
						std::stringstream ss;
						ss << "opencv::webcam::index=" << cameraIndex;
						cameraSerialNumber = ss.str();
					}
					else if (usingFile)
					{
						std::stringstream ss;
						ss << "opencv::file::uri=" << url;
						cameraSerialNumber = ss.str();
					}
					else {

						std::stringstream ss;
						ss << "opencv::any::uri=" << url;
						cameraSerialNumber = ss.str();
					}



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

			Logger::Log(CVVideoCaptureCameraStr) << "Opened cv::VideoCapture device: " << cameraSerialNumber << std::endl;
		}
			//
			// Step #2) START, LOOP FOR FRAMES, STOP
			//

			// start keeping track of incoming frames / failed frames
			statistics.StartCounting();
			auto timeSinceLastFrame = std::chrono::high_resolution_clock::now();
			//std::chrono::milliseconds timeSinceLastFrame = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
			long long periodms = floor(1000.0 / cvDeviceFrameRate);

			// loop to capture frames
			if (thread_running && IsAnyCameraEnabled())
			{
				// time to start reading frames and streaming
				unsigned int triesBeforeRestart = 5;
				totalTries = 0;

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
				Logger::Log(CVVideoCaptureCameraStr) << "Started capturing" << std::endl;

				// invokes camera connect callback
				didWeCallConnectedCallback = true; // we will need this later in case the thread is stopped
				if (onCameraConnect)
					onCameraConnect();


				// capture loop
				while (thread_running)
				{
					try
					{
						if (!usingFile)
						{
							// webcam loop
							while (thread_running)
							{
								std::shared_ptr<Frame> sharedColorFrame;

								// capture color
								device->read(videoFrame);

								// todo: get camera timestamp (so that we can syncronize cameras through their hardware)
								// timestamp = colorFrame.get_device_timestamp();
								std::chrono::microseconds timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

								// is it empty?
								if (videoFrame.empty())
									throw("empty video frame");

								// copies image to our very own frame
								sharedColorFrame = Frame::Create(videoFrame.size().width, videoFrame.size().height, FrameType::Encoding::BGR24);
								if (!sharedColorFrame)
									throw std::bad_alloc();

								assert(colorFrame.get_data_size() == sharedColorFrame->size()); // sanity check for debugging
								memcpy(sharedColorFrame->data, videoFrame.ptr(), sharedColorFrame->size()); // no suppport for stride! (yet)

								// invoke callback
								if (onFramesReady)
									onFramesReady(timestamp, sharedColorFrame, nullptr, nullptr);

								// update info
								++statistics.framesCaptured;
								triesBeforeRestart = 5;
							}
						}
						else {
							// file loop
							while (thread_running)
							{
								// rewind
								if (currentFrame >= frameCount)
								{
									device->set(cv::CAP_PROP_POS_FRAMES, 0);
									currentFrame = 0;
								}

								std::shared_ptr<Frame> sharedColorFrame;

								// capture color
								device->read(videoFrame);

								// todo: get camera timestamp (so that we can syncronize cameras through their hardware)
								// timestamp = colorFrame.get_device_timestamp();
								std::chrono::microseconds timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

								// is it empty?
								if (videoFrame.empty())
									throw("empty video frame");

								// copies image to our very own frame
								sharedColorFrame = Frame::Create(videoFrame.size().width, videoFrame.size().height, FrameType::Encoding::BGR24);
								if (!sharedColorFrame)
									throw std::bad_alloc();

								assert(colorFrame.get_data_size() == sharedColorFrame->size()); // sanity check for debugging
								memcpy(sharedColorFrame->data, videoFrame.ptr(), sharedColorFrame->size()); // no suppport for stride! (yet)

								// sleep a little bit (to control frame rate)
								long long timeleft = periodms - std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - timeSinceLastFrame).count();
								if (timeleft > 0)
									std::this_thread::sleep_for(std::chrono::milliseconds(timeleft));
								timeSinceLastFrame = std::chrono::high_resolution_clock::now();

								// invoke callback
								if (onFramesReady)
									onFramesReady(timestamp, sharedColorFrame, nullptr, nullptr);

								// update info
								++statistics.framesCaptured;
								triesBeforeRestart = 5;
							}
						}

					}
					catch (const cv::Exception& e) // opencv exception
					{
						++statistics.framesFailed;
						Logger::Log(CVVideoCaptureCameraStr) << "ERROR! Tried to get frame but failed: " << e.what() << "\n at " << e.file << ":" << e.line << std::endl;
						--triesBeforeRestart;

						if (triesBeforeRestart == 0)
						{
							Logger::Log(CVVideoCaptureCameraStr) << "Tried to get a frame 5 times but failed! Restarting capture in 5 seconds..." << std::endl;
							device->release();
							colorCameraEnabled = false;
							std::this_thread::sleep_for(std::chrono::seconds(5));
							break; // breaks out of the loop
						}
						else {
							Logger::Log(CVVideoCaptureCameraStr) << "Trying again in 1 second!" << std::endl;
							std::this_thread::sleep_for(std::chrono::seconds(1));
						}
					}
					catch (const std::exception& e) // our own exception
					{
						++statistics.framesFailed;
						Logger::Log(CVVideoCaptureCameraStr) << "ERROR! Tried to get frame but failed: " << e.what() << std::endl;
						--triesBeforeRestart;

						if (triesBeforeRestart == 0)
						{
							Logger::Log(CVVideoCaptureCameraStr) << "Tried to get a frame 5 times but failed! Restarting capture in 5 seconds..." << std::endl;
							device->release();
							colorCameraEnabled = false;
							std::this_thread::sleep_for(std::chrono::seconds(5));
							break; // breaks out of the loop
						}
						else {
							Logger::Log(CVVideoCaptureCameraStr) << "Trying again in 1 second!" << std::endl;
							std::this_thread::sleep_for(std::chrono::seconds(1));
						}
					}
					catch (const std::bad_alloc& e)
					{
						++statistics.framesFailed;
						Logger::Log(CVVideoCaptureCameraStr) << "FATAL ERROR! No memory left! Restarting device in 10 seconds! (" << e.what() << ")" << std::endl;
						colorCameraEnabled = false;
						appStatus->UpdateCaptureStatus(false, false);
						statistics.StopCounting();

						// waits 10 seconds before trying again
						std::this_thread::sleep_for(std::chrono::seconds(10));

						// breaks loop
						break;
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
					if (device->isOpened())
						device->release();
					device = nullptr;
					colorCameraEnabled = false;
				}

				// calls the camera disconnect callback if we called onCameraConnect() - consistency
				if (didWeCallConnectedCallback && onCameraDisconnect)
					onCameraDisconnect();

				// waits one second before restarting...
				if (thread_running)
				{
					Logger::Log(CVVideoCaptureCameraStr) << "Restarting device..." << std::endl;
				}


			}

		
	}


}


#endif