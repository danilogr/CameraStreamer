// KinectStreamer.cpp
// author: Danilo Gasques
// email: danilod100 at gmail.com / gasques at ucsd.edu
// danilogasques.com

#include <iostream>
#include <chrono>
#include <map>

//
// Local includes, from more generic and widely used to more specific and locally required
//

// 1) key resources shared by all other headers
#include "Logger.h"
#include "ApplicationStatus.h"

// 2) Abstraction of video capture devices and memory management for frames
#include "Frame.h"
#include "Camera.h"

// 3) Applications (threads)
#include "TCPStreamingServer.h"
#include "RemoteControlServer.h"
#include "VideoRecorder.h"

// 4) specific cameras supported
#include "AzureKinect.h"


using namespace std;

int main()
{

//	Logger::Log("Main") << "There are " << k4a_device_get_installed_count() << " kinect devices connected to this computer" << endl;


	// ApplicationStatus is the data structure the application uses to synchronize 
		// the overall application state machine across threads (e.g.: VideoRecorder uses it
		// to let other threads know when it is recording, for instance)
	std::shared_ptr<ApplicationStatus> appStatus = std::make_shared<ApplicationStatus>();
	ApplicationStatus& appStatusPtr = *appStatus;

	// set default values
	appStatus->SetStreamerPort(3614);
	appStatus->SetControlPort(6606);

	// structure that lists supported cameras -> points to their constructors
	map<string, std::shared_ptr<Camera> (*)(std::shared_ptr<ApplicationStatus>)> SupportedCamerasSet = { {"k4a", &AzureKinect::Create} };

	// read configuration file if one is present
	appStatus->LoadConfiguration("config.json");

	// do we have a camera we currently support?
	if (SupportedCamerasSet.find(appStatus->GetCameraName()) == SupportedCamerasSet.cend())
	{
		Logger::Log("Main") << "Device \"" << appStatus->GetCameraName() << "\" is not supported! Exiting..." << endl;
		return 1;
	}


	// main application loop where it waits for a user key to stop everything
	{

		// starts listening but not yet dealing with client connections
		TCPStreamingServer server(appStatus);
		VideoRecorder videoRecorderThread(appStatus, appStatus->GetCameraName());

		// instantiate the correct camera
		std::shared_ptr<Camera> depthCamera = SupportedCamerasSet[appStatus->GetCameraName()](appStatus);

		depthCamera->onFramesReady = [&](std::chrono::microseconds, std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
		{
			server.ForwardToAll(color, depth);

			if (appStatusPtr.isRedirectingFramesToRecorder())
			{
				videoRecorderThread.RecordFrame(color, depth);
			}

		};

		// we could print messages when the camera gets connected / disconnected; not now
		depthCamera->onCameraConnect = []()	{};
		depthCamera->onCameraDisconnect = []() {};
	
		// this is a commented out example of how we can override camera settings using a 
		// SDK specific data structure
		//k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		//config.color_format     = K4A_IMAGE_FORMAT_COLOR_BGRA32; // we need BGRA32 because JPEG won't allow transformation
		//config.camera_fps       = K4A_FRAMES_PER_SECOND_30;		 // at 30 fps
		//config.color_resolution = K4A_COLOR_RESOLUTION_720P;     // 1280x720
		//config.depth_mode       = K4A_DEPTH_MODE_NFOV_UNBINNED;  // 640x576 - fov 75x65 - 0.5m-3.86m
		//config.synchronized_images_only = true;					 // depth and image should be synchronized

		// start device, streaming server, and recording thread
		//depthCamera.SetCameraConfigurationFromCustomDatastructure((void*)& config);
		depthCamera->Run();
		server.Run();
		videoRecorderThread.Run();

		// finally 
		RemoteControlServer remoteControlServer(appStatus,

		// on start kinect request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			// sanity check
			if (!depthCamera) return;

			// we are already running
			if (depthCamera->isStreaming())
			{
				Logger::Log("Remote") << "(startKinect) Kinect is already running!" << std::endl;
				return;
			}

			depthCamera->Run();
			
		},

		// on stop kinect request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			// sanity check
			if (!depthCamera) return;

			if (!depthCamera->isRunning())
			{
				Logger::Log("Remote") << "(stopKinect) Kinect is not running!" << std::endl;
				return;
			}

			depthCamera->Stop();
		},


		// on start recording request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
		
			bool recordingColor = (message.HasMember("color") && message["color"].IsBool()) ? message["color"].GetBool() : true; // records color by default
			bool recordingDepth = (message.HasMember("depth") && message["depth"].IsBool()) ? message["depth"].GetBool() : true; // records depth by default

			std::string recordingColorPath;
			if (recordingColor)
			{
				if (message.HasMember("colorPath"))
					recordingColorPath = std::string(message["colorPath"].GetString(), message["colorPath"].GetStringLength());
				else
				{
					Logger::Log("Remote") << "(startRecording) Color path was not defined!" << std::endl;
					return;
				}
			}

			std::string recordingDepthPath;

			if (recordingDepth)
			{
				if (message.HasMember("depthPath"))
					recordingDepthPath = std::string(message["depthPath"].GetString(), message["depthPath"].GetStringLength());
				else
				{
					Logger::Log("Remote") << "(startRecording) Depth path was not defined!" << std::endl;
					return;
				}
			}
			
			videoRecorderThread.StartRecording(recordingColor, recordingDepth, recordingColorPath, recordingDepthPath);

		},


		// on stop recording request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			videoRecorderThread.StopRecording();
		},


		// on shutdown request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			Logger::Log("Remote") << "Received shutdown notice... " << endl;

			// if recording, we stop recording...

			if (videoRecorderThread.isRecordingInProgress())
				videoRecorderThread.StopRecording();

			// prevents remote control from receiving any new messages
			// by stopping it first
			//remoteControlServer.Stop();

			// stops tcp server
			server.Stop();

			// stops cameras
			if (depthCamera)
				depthCamera->Stop();

			// wait for video recording to end
			videoRecorderThread.Stop();

			// kicks the bucket
			exit(0);

		},

		// change exposure
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document& message)
		{
			// sanity check
			if (!depthCamera) return;

			if (message.HasMember("value") && message["value"].IsNumber())
			{
				depthCamera->AdjustExposureBy(message["value"].GetInt());
			}
			else {
				Logger::Log("Remote") << "(changeExposure) Error! No value received!" << std::endl;
			}

		},


		// change gain
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document& message)
		{
			// sanity check
			if (!depthCamera) return;

			if (message.HasMember("value") && message["value"].IsNumber())
			{
				depthCamera->AdjustGainBy(message["value"].GetInt());
				
			}
			else {
				Logger::Log("Remote") << "(changeGain) Error! No value received!" << std::endl;
			}

		});

		// runs the rmeote server with the above callbacks (yeah, I should remove
		// them from the constructor...)
		remoteControlServer.Run();


		// let's everyone know that they can exit by pressing 'q'
		Logger::Log("Main") << endl << "To close this application, press 'q'" << endl;

		// waits for user command to exit
		bool exit = false;
		while (!exit)
		{
			switch (getchar())
			{
				case '+':
					depthCamera->AdjustExposureBy(1);
					break;
				case '-':
					depthCamera->AdjustExposureBy(-1);
					break;
				case 'q':
					exit = true;
					break;
			}
		}
		Logger::Log("Main") << "User pressed 'q'. Exiting... " << endl;

		// if recording, we stop recording...
		if (videoRecorderThread.isRecordingInProgress())
			videoRecorderThread.StopRecording();

		// prevents remote control from receiving any new messages
		// by stopping it first
		remoteControlServer.Stop();

		// stops tcp server
		server.Stop();

		// stops cameras
		depthCamera->Stop();

		// wait for video recording to end
		videoRecorderThread.Stop();

		// done
	}


}

