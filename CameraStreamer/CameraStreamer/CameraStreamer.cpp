// CameraStreamer.cpp
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
#include "TCPRelayCamera.h"
#include "OpenCVVideoCaptureCamera.h"

// 5) version specific 
#include "Version.h"

using namespace std;

int main(int argc, char* argv[])
{

	// Hello world!
 	Logger::Log("Main") << "CameraStreamer v." << VERSION_MAJOR << '.' << VERSION_MINOR << '.' << VERSION_PATCH << endl;
	Logger::Log("Main") << "To close this application, press 'q'" << endl << endl;


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
		#ifdef CS_ENABLE_CAMERA_K4A
		{"k4a", &AzureKinect::Create},
		#endif

		// RealSense2 support
		#ifdef CS_ENABLE_CAMERA_RS2
		{"rs2", &RealSense::Create},
		#endif

		// TCP Relay support
		#ifdef  CS_ENABLE_CAMERA_TCPCLIENT_RELAY
		{"tcp-relay", &TCPRelayCamera::Create},
		#endif //  CS_ENABLE_CAMERA_TCPCLIENT_RELAY

		// OpenCV
		#ifdef CS_ENABLE_CAMERA_CV_VIDEOCAPTURE
		{"opencv", &CVVideoCaptureCamera::Create},
		#endif // CS_ENABLE_CAMERA_CV_VIDEOCAPTURE

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
	if (SupportedCamerasSet.find(configuration->GetCameraType()) == SupportedCamerasSet.cend())
	{
		Logger::Log("Main") << "Device \"" << configuration->GetCameraType() << "\" is not supported! Exiting..." << endl;
		return 1;
	}

	// initializes appStatus based on some default values from the configuration
	appStatus->UpdateAppStatusFromConfig(*configuration);

	// main application loop where it waits for a user key to stop everything
	try 
	{

		// starts listening but not yet dealing with client connections
		TCPStreamingServer server(appStatus, configuration);
		VideoRecorder videoRecorderThread(appStatus, appStatus->GetCameraType());

		// instantiate the correct camera
		std::shared_ptr<Camera> camera = SupportedCamerasSet[appStatus->GetCameraType()](appStatus, configuration);

		// set up callbacks
		camera->onFramesReady = [&](std::chrono::microseconds, std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth, std::shared_ptr<Frame> originalDepth)
		{
			// streams to client
			server.ForwardToAll(color, depth);

			// saves to file 
			if (appStatusPtr.isRedirectingFramesToRecorder())
			{
				videoRecorderThread.RecordFrame(color, originalDepth);
			}

		};

		
		// prints device intrinsics the first time
		bool printedIntrinsicsOnce = false;
		camera->onCameraConnect = [&]()
		{
			if (!printedIntrinsicsOnce && camera)
			{
				camera->PrintCameraIntrinsics();
				printedIntrinsicsOnce = true;				
			}

			// also, make sure that the streaming software can handle the content comming from the camera
			// (this only works to disable streaming in case it was expected)
			if (appStatus && appStatus->GetStreamingColorEnabled())
			{
				appStatus->SetStreamingColorEnabled(camera->IsColorCameraEnabled());
				appStatus->SetStreamingWidth(camera->colorCameraParameters.resolutionWidth);
				appStatus->SetStreamingHeight(camera->colorCameraParameters.resolutionHeight);
			}

			if (appStatus && appStatus->GetStreamingDepthEnabled())
			{
				appStatus->SetStreamingDepthEnabled(camera->IsDepthCameraEnabled());

				if (!appStatus->GetStreamingColorEnabled())
				{
					appStatus->SetStreamingWidth(camera->depthCameraParameters.resolutionWidth);
					appStatus->SetStreamingHeight(camera->depthCameraParameters.resolutionHeight);
				}
			}

			// are we supposed to be recording? resume recording
			if (appStatus && appStatus->HasPendingRequestToRecord())
			{
				videoRecorderThread.StartRecording(appStatus->HasPendingRequestToRecordColor(), appStatus->HasPendingRequestToRecordDepth(),
					appStatus->GetRequestToRecordColorPath(), appStatus->GetRequestToRecordDepthPath());
			}

		};

		camera->onCameraDisconnect = [&]()
		{
			if (camera)
			{ 
				Logger::Log("Camera") << "Captured " << camera->statistics.framesCaptured << " frames in " << camera->statistics.durationInSeconds() << " seconds (" << ((double)camera->statistics.framesCaptured / (double)camera->statistics.durationInSeconds()) << " fps) - Fails: " << camera->statistics.framesFailed << " times" << std::endl;
			}

			if (appStatus && appStatus->isRedirectingFramesToRecorder())
			{
				videoRecorderThread.StopRecording();
			}
		};

		camera->Run();
		server.Run();
		videoRecorderThread.Run();

		// finally 
		RemoteControlServer remoteControlServer(appStatus,

		// on start kinect request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			// sanity check
			if (!camera) return;

			// we are already running
			if (camera->IsAnyCameraEnabled())
			{
				Logger::Log("Remote") << "(startCamera) Camera is already running!" << std::endl;
				return;
			}

			camera->Run();
			
		},

		// on stop kinect request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			// sanity check
			if (!camera) return;

			if (!camera->IsThreadRunning())
			{
				Logger::Log("Remote") << "(stopCamera) Camera is not running!" << std::endl;
				return;
			}

			camera->Stop();
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
			
			appStatusPtr.UpdateIntentToRecord(recordingColor, recordingDepth, recordingColorPath, recordingDepthPath);
			videoRecorderThread.StartRecording(recordingColor, recordingDepth, recordingColorPath, recordingDepthPath);

		},


		// on stop recording request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			appStatusPtr.UpdateIntentToRecord(false, false);
			videoRecorderThread.StopRecording();
		},


		// on shutdown request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			Logger::Log("Remote") << "Received shutdown notice... " << endl;

			// if recording, we stop recording...

			appStatusPtr.UpdateIntentToRecord(false, false);

			if (videoRecorderThread.isRecordingInProgress())
				videoRecorderThread.StopRecording();

			// prevents remote control from receiving any new messages
			// by stopping it first
			//remoteControlServer.Stop();

			// stops tcp server
			server.Stop();

			// stops cameras
			if (camera)
				camera->Stop();

			// wait for video recording to end
			videoRecorderThread.Stop();

			// kicks the bucket
			exit(0);

		},

		// change exposure
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document& message)
		{
			// sanity check
			if (!camera) return;

			if (message.HasMember("value") && message["value"].IsNumber())
			{
				camera->AdjustExposureBy(message["value"].GetInt());
			}
			else {
				Logger::Log("Remote") << "(changeExposure) Error! No value received!" << std::endl;
			}

		},


		// change gain
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document& message)
		{
			// sanity check
			if (!camera) return;

			if (message.HasMember("value") && message["value"].IsNumber())
			{
				camera->AdjustGainBy(message["value"].GetInt());
				
			}
			else {
				Logger::Log("Remote") << "(changeGain) Error! No value received!" << std::endl;
			}

		});

		// runs the rmeote server with the above callbacks (yeah, I should remove
		// them from the constructor...)
		remoteControlServer.Run();


	

		bool exit = false;
		while (!exit)
		{
			switch (getchar())
			{
				case '+':
					camera->AdjustExposureBy(1);
					break;
				case '-':
					camera->AdjustExposureBy(-1);
					break;
				case 'q':
					exit = true;
					break;
				case 'r':
					if (videoRecorderThread.isRecordingInProgress())
						videoRecorderThread.StopRecording();

					videoRecorderThread.StartRecording(true, false, "manual-recording", "");
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
		camera->Stop();

		// wait for video recording to end
		videoRecorderThread.Stop();

		// done
	}
	catch (const std::exception& ex)
	{
		Logger::Log("Main") << "[FATAL ERROR] Unhandled exception: " << ex.what() << std::endl << std::endl;
		Logger::Log("Main") << "\a\a\aShutting down in 30 seconds...\n";

		std::this_thread::sleep_for(std::chrono::seconds(30));
		return 1;
	}

	return 0;
}

