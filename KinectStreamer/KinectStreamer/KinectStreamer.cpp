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
#include "Logger.h" // singleton (for now)
#include "Configuration.h"
#include "ApplicationStatus.h"

// 2) Abstraction of video capture devices and memory management for frames
#include "Frame.h"
#include "Camera.h"

// 3) Applications (threads)
#include "TCPStreamingServer.h"
#include "RemoteControlServer.h"
#include "VideoRecorder.h"

// 4) specific cameras supported
#include "CompilerConfiguration.h"
#include "AzureKinect.h"
#include "RealSense.h"


using namespace std;

int main(int argc, char* argv[])
{

//	Logger::Log("Main") << "There are " << k4a_device_get_installed_count() << " kinect devices connected to this computer" << endl;


	// ApplicationStatus is the data structure the application uses to synchronize 
	// the overall application state machine across threads (e.g.: VideoRecorder uses it
	// to let other threads know when it is recording, for instance)
	std::shared_ptr<ApplicationStatus> appStatus = std::make_shared<ApplicationStatus>();

	// Configuration is a data structure that holds the default settings
	// for all threads
	std::shared_ptr<Configuration> configuration = std::make_shared<Configuration>();


	ApplicationStatus& appStatusPtr = *appStatus;

	// set default values
	appStatus->SetStreamerPort(3614);
	appStatus->SetControlPort(6606);

	// structure that lists supported cameras -> points to their constructors
	typedef  std::map<string, std::shared_ptr<Camera>(*)(std::shared_ptr<ApplicationStatus>, std::shared_ptr<Configuration>)> CameraNameToConstructorMap;
	CameraNameToConstructorMap SupportedCamerasSet = {
		
		// Azure Kinect support
		#ifdef ENABLE_K4A
		{"k4a", &AzureKinect::Create},
		#endif

		// RealSense2 support
		#ifdef ENABLE_RS2
		{"rs2", &RealSense::Create},
		#endif
	};
	

	std::string configFilePath("config.json");
	// do we have a parameter?
	if (argc > 1)
	{
		// uses the first argument as the configuration filename
		configFilePath = std::string(argv[1]);
	}


	// read configuration file if one is present
	configuration->LoadConfiguration(configFilePath);

	// do we have a camera we currently support?
	if (SupportedCamerasSet.find(configuration->GetCameraName()) == SupportedCamerasSet.cend())
	{
		Logger::Log("Main") << "Device \"" << configuration->GetCameraName() << "\" is not supported! Exiting..." << endl;
		return 1;
	}

	// initializes appStatus based on some default values from the configuration
	appStatus->UpdateAppStatusFromConfig(*configuration);

	// main application loop where it waits for a user key to stop everything
	{

		// starts listening but not yet dealing with client connections
		TCPStreamingServer server(appStatus, configuration);
		VideoRecorder videoRecorderThread(appStatus, appStatus->GetCameraName());

		// instantiate the correct camera
		std::shared_ptr<Camera> depthCamera = SupportedCamerasSet[appStatus->GetCameraName()](appStatus, configuration);

		// set up callbacks
		depthCamera->onFramesReady = [&](std::chrono::microseconds, std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
		{
			// streams to client
			server.ForwardToAll(color, depth);

			// saves to file 
			if (appStatusPtr.isRedirectingFramesToRecorder())
			{
				videoRecorderThread.RecordFrame(color, depth);
			}

		};

		
		// prints device intrinsics the first time
		bool printedIntrinsicsOnce = false;
		depthCamera->onCameraConnect = [&]()
		{
			if (!printedIntrinsicsOnce && depthCamera)
			{
				depthCamera->PrintCameraIntrinsics();
				printedIntrinsicsOnce = true;
			}
		};

		depthCamera->onCameraDisconnect = [&]()
		{
			if (depthCamera)
			{ 
				Logger::Log("Camera") << "Captured " << depthCamera->statistics.framesCaptured << " frames in " << depthCamera->statistics.durationInSeconds() << " seconds (" << ((double)depthCamera->statistics.framesCaptured / (double)depthCamera->statistics.durationInSeconds()) << " fps) - Fails: " << depthCamera->statistics.framesFailed << " times" << std::endl;
			}
		};

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
			if (depthCamera->IsAnyCameraEnabled())
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

			if (!depthCamera->IsThreadRunning())
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

