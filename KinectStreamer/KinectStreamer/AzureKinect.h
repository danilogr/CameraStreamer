#pragma once

#include "Logger.h"
#include <k4a/k4a.h>
#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>

class AzureKinect
{
	std::shared_ptr<std::thread> sThread;
	k4a_device_t kinectDevice = nullptr;
	bool thread_running;
	bool runningCameras;
	k4a_device_configuration_t kinectConfiguration;

	std::string kinectDeviceSerial;

public:
	AzureKinect() : thread_running(false), runningCameras(false)
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
			k4a_device_stop_cameras(kinectDevice);

		if (kinectDevice != nullptr)
			k4a_device_close(kinectDevice);
	}

	void Run(k4a_device_configuration_t kinectConfig)
	{
		// saves the configuration
		kinectConfiguration = kinectConfig;

		// starts thread
		sThread.reset(new std::thread(std::bind(&AzureKinect::thread_main, this)));
	}

	bool AdjustGain()
	{
		return false;
	}

	const std::string& getSerial()
	{
		return kinectDeviceSerial;
	}


private:

	bool OpenDefaultKinect()
	{
		if (K4A_FAILED(k4a_device_open(K4A_DEVICE_DEFAULT, &kinectDevice)))
		{
			Logger::Log("AzureKinect") << "Could not open default device..." << std::endl;
			kinectDevice = nullptr;
			return false;
		}


		// let's update the serial number
		size_t serial_size = 0;
		k4a_device_get_serialnum(kinectDevice, NULL, &serial_size);

		std::vector<char> serial(serial_size+5);
		k4a_device_get_serialnum(kinectDevice, &serial[0], &serial_size);
		serial[serial_size] = 0; // end of string

		kinectDeviceSerial = std::string(&serial[0]);

		return true;
	}

	
	void thread_main()
	{
		Logger::Log("AzureKinect") << "Started Azure Kinect polling thread: " << std::this_thread::get_id << std::endl;
		thread_running = true;

		while (thread_running)
		{
			while (!OpenDefaultKinect())
			{
				// waits one second
				std::this_thread::sleep_for(std::chrono::seconds(1));
				Logger::Log("AzureKinect") << "Trying again..." << std::endl;
			}

			Logger::Log("AzureKinect") << "Opened kinect device id: " << kinectDeviceSerial << std::endl;

			while (thread_running) {  }
		}
	}



};


