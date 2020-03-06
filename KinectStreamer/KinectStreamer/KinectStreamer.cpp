// KinectStreamer.cpp
// author: Danilo Gasques
// email: danilod100 at gmail.com / gasques at ucsd.edu
// danilogasques.com

#include <iostream>
#include <k4a/k4a.h>
#include <chrono>

#include "TCPStreamingServer.h"
#include "AzureKinect.h"
#include "Frame.h"
#include "Logger.h"

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
	if (k4a_device_get_installed_count() == 0)
	{
		Logger::Log("Main") << "No AzureKinect devices connected ... exiting" << endl;
		return 1;
	}

	// main application loop where it waits for a user key to stop everything
	{
		TCPStreamingServer server(3614);
		server.Run();

		AzureKinect kinectDevice;
		kinectDevice.onFramesReady = [&](std::chrono::microseconds, std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
		{
			server.ForwardToAll(color, depth);
		};

		k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
		config.color_format     = K4A_IMAGE_FORMAT_COLOR_BGRA32; // we need BGRA32 because JPEG won't allow transformation
		config.camera_fps       = K4A_FRAMES_PER_SECOND_30;		 // at 30 fps
		config.color_resolution = K4A_COLOR_RESOLUTION_720P;     // 1280x720
		config.depth_mode       = K4A_DEPTH_MODE_NFOV_UNBINNED;  // 640x576 - fov 75x65 - 0.5m-3.86m
		config.synchronized_images_only = true;					 // depth and image should be synchronized

		kinectDevice.Run(config);


		std::cout << endl;
		Logger::Log("Main") << "To close this application, press 'q'" << endl;

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
	}


}

