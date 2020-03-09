// KinectStreamer.cpp
// author: Danilo Gasques
// email: danilod100 at gmail.com / gasques at ucsd.edu
// danilogasques.com

#include <iostream>
#include <k4a/k4a.h>
#include <chrono>

#include "TCPStreamingServer.h"
#include "RemoteControlServer.h"
#include "VideoRecorder.h"
#include "AzureKinect.h"
#include "Frame.h"
#include "Logger.h"
#include "ApplicationStatus.h"

#include <opencv2/opencv.hpp>


void OnFrameReadyCallback(std::chrono::microseconds, std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
{
	cv::Mat colorImage(color->getHeight(), color->getWidth(), CV_8UC4, color->getData());
	cv::Mat depthImage(depth->getHeight(), depth->getWidth(), CV_16UC1, depth->getData());

	cv::imshow("Color", colorImage);
	cv::imshow("Depth", depthImage);
	cv::waitKey(-1);
}


using namespace std;
int main()
{

	Logger::Log("Main") << "There are " << k4a_device_get_installed_count() << " kinect devices connected to this computer" << endl;

	// no devices installed ?
	//if (k4a_device_get_installed_count() == 0)
	//{
	//	Logger::Log("Main") << "No AzureKinect devices connected ... exiting" << endl;
	//	return 1;
	//}

	// ApplicationStatus is the data structure the application uses to synchronize 
		// the overall application state machine across threads (e.g.: VideoRecorder uses it
		// to let other threads know when it is recording, for instance)
	std::shared_ptr<ApplicationStatus> appStatus = std::make_shared<ApplicationStatus>();
	ApplicationStatus& appStatusPtr = *appStatus;

	appStatus->streamerPort = 3614;
	appStatus->controlPort = 6606;

	// main application loop where it waits for a user key to stop everything
	{

		// starts listening but not yet dealing with client connections
		TCPStreamingServer server(appStatus);
		VideoRecorder videoRecorderThread(appStatus, "Kinect");
		AzureKinect kinectDevice(appStatus);		

		kinectDevice.onFramesReady = [&](std::chrono::microseconds, std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
		{
			server.ForwardToAll(color, depth);

			if (appStatusPtr._redirectFramesToRecorder)
			{
				videoRecorderThread.RecordFrame(color, depth);
			}

		};

		k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		config.color_format     = K4A_IMAGE_FORMAT_COLOR_BGRA32; // we need BGRA32 because JPEG won't allow transformation
		config.camera_fps       = K4A_FRAMES_PER_SECOND_30;		 // at 30 fps
		config.color_resolution = K4A_COLOR_RESOLUTION_720P;     // 1280x720
		config.depth_mode       = K4A_DEPTH_MODE_NFOV_UNBINNED;  // 640x576 - fov 75x65 - 0.5m-3.86m
		config.synchronized_images_only = true;					 // depth and image should be synchronized

		// start device, streaming server, and recording thread
		kinectDevice.Run(config);
		server.Run();
		videoRecorderThread.Run();

		// finally 
		RemoteControlServer remoteControlServer(appStatus,

		// on start kinect request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			// we are already running
			if (kinectDevice.isStreaming())
			{
				Logger::Log("Remote") << "(startKinect) Kinect is already running!" << std::endl;
				return;
			}

			kinectDevice.Run(config);
			
		},

		// on stop kinect request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			if (!kinectDevice.isRunning())
			{
				Logger::Log("Remote") << "(stopKinect) Kinect is not running!" << std::endl;
				return;
			}

			kinectDevice.Stop();
		},


		// on start recording request
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document & message)
		{
			std::string recordingPath = message["path"].GetString();

			bool recordingColor = (message.HasMember("color") && message["color"].IsBool()) ? message["color"].GetBool() : true; // records color by default
			bool recordingDepth = (message.HasMember("depth") && message["depth"].IsBool()) ? message["depth"].GetBool() : true; // records depth by default

			videoRecorderThread.StartRecording(recordingPath, recordingColor, recordingDepth);
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
			kinectDevice.Stop();

			// wait for video recording to end
			videoRecorderThread.Stop();

			// kicks the bucket
			exit(0);

		},

		// change exposure
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document& message)
		{
			if (message.HasMember("value") && message["value"].IsNumber())
			{
				kinectDevice.AdjustExposureBy(message["value"].GetInt());
			}
			else {
				Logger::Log("Remote") << "(changeExposure) Error! No value received!" << std::endl;
			}

		},


		// change gain
		[&](std::shared_ptr<RemoteClient> client, const rapidjson::Document& message)
		{
			if (message.HasMember("value") && message["value"].IsNumber())
			{
				kinectDevice.AdjustGainBy(message["value"].GetInt());
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
					kinectDevice.AdjustExposureBy(1);
					break;
				case '-':
					kinectDevice.AdjustExposureBy(-1);
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
		kinectDevice.Stop();

		// wait for video recording to end
		videoRecorderThread.Stop();

		// done
	}


}

