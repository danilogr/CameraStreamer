#pragma once

#include <functional>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>

#include "Logger.h"
#include "Frame.h"

#include "Configuration.h"
#include "ApplicationStatus.h"



struct DataSourceStatistics
{
	// number of frames captured on the long run
	unsigned long long framesCapturedTotal;
	unsigned long long framesFailedTotal;
	unsigned int sessions;
	std::chrono::steady_clock::time_point startTimeTotal;
	std::chrono::steady_clock::time_point endTimeTotal;

	// number of frames captured in this session
	unsigned long long framesCaptured;
	unsigned long long framesFailed;
	std::chrono::steady_clock::time_point startTime;
	std::chrono::steady_clock::time_point endTime;


	DataSourceStatistics() : framesCapturedTotal(0), framesFailedTotal(0), sessions(0), framesCaptured(0), framesFailed(0), initialized(false), inSession(false) {}

	void StartCounting()
	{
		// did we forget to stop counting?
		if (inSession)
		{
			StopCounting(); // not super accurate, but whatevs
		}

		startTime = std::chrono::high_resolution_clock::now();
		framesCaptured = 0;
		framesFailed = 0;

		// this is the first time we are running the camera loop
		// thus, we will set both the startTimeTotal and the startTime
		if (!initialized)
		{
			initialized = true;
			startTimeTotal = startTime;
		}

		++sessions;
		inSession = true;
	}

	void StopCounting()
	{
		if (inSession)
		{
			endTime = std::chrono::high_resolution_clock::now();

			// add information to the long run
			inSession = false;
			endTimeTotal = endTime;
			framesCapturedTotal += framesCaptured;
			framesFailedTotal += framesFailed;
		}
	}

	inline long long durationInSeconds()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
	}

	inline long long totalDurationInSeconds()
	{
		return std::chrono::duration_cast<std::chrono::seconds>(endTimeTotal - startTimeTotal).count();
	}

private:
	bool initialized, inSession;
};

/**
  The Camera class is an abstraction to all cameras supported by this application.
  In an ideal world, cameras would be loaded dynamically so that our application doesn't
  require lots of DLLs. For now, we compile and link to all the cameras that we might need.

  Todo: Support Building with / without specific data sources
  Todo: Support loading data source modules dynamically from DLLs
  */
class DataSource
{

public:

protected:
	// DataSource properties that help us print what type
	// of interface is implemented by a DataSource
	std::string dataSourceDescriptor;

	// the DataSource Serial Number is a developer-defined 
	// string that helps one uniquely identify a data source
	std::string dataSourceSN;

	// timeouts?
	std::chrono::milliseconds getFrameTimeout;
	unsigned int getFrameTimeoutMSInt;

	// throttling? (TODO)
	std::chrono::milliseconds getThrottlingPeriod;
	unsigned int getThrottlingPeriodMSInt;

	// all threads use a shared data structure to report their status
	std::shared_ptr<ApplicationStatus> appStatus;

	// configuration (from either a configuration file or a setting set by the user)
	std::shared_ptr<Configuration> configuration;

	// DataSources have their own threads that request frames requesting frames
	// Todo: we should create an abstract thread class for future uses ;)
	std::shared_ptr<std::thread> sThread;
	bool thread_running;

	// should be set to a value
	bool connected; 

	// the DataSourceLoop is the thread responsible for implementing the DataSource logic
	// (for example, connecting to the data source, reporting it to the application, invoking events)
	virtual void DataSourceLoop() = 0;

	void thread_main()
	{
		// executes the internal camera loop
		thread_running = true;

		// loops through the camera loop implementation
		while (thread_running)
		{
			try
			{
				DataSourceLoop();
			}
			catch (...)
			{
				Logger::Log("DataSource") << "Unhandled exception in " << dataSourceDescriptor << " (" << dataSourceSN << "). Restarting DataSource thread in 5 seconds..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5)); // Todo: make this configurable
			}
		}

		// This should not happen because this method
		// is running within the thread!
		//sThread = nullptr;
	}


	// timestamp, color, depth
	typedef std::function<void(std::chrono::microseconds, std::shared_ptr<Frame>)> FrameReadyCallback;
	typedef std::function<void()> ConnectedCallback;
	typedef std::function<void()> DisconnectedCallback;

	// method used to parse configuration settings
	// other camera classes should extend to add implementation specific settings

	virtual bool LoadConfigurationSettings()
	{
		// camera frame timeout
		getFrameTimeoutMSInt = configuration->GetCameraFrameTimeoutMs();
		getFrameTimeout = configuration->GetCameraFrameTimoutMsChrono();

		return true;
	}

public:

	// callback invoked when a frame is ready
	FrameReadyCallback onFramesReady;

	// callback invoked when the data source is running
	ConnectedCallback onConnect;

	// callback invoked when the data source disconnects
	DisconnectedCallback onDisconnect;

	// constructor explicitly defining a configuration file (as well as appStatus)
	DataSource(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : appStatus(appStatus), configuration(configuration), thread_running(false), getFrameTimeout(1000), getFrameTimeoutMSInt(1000), getThrottlingPeriod(25), getThrottlingPeriodMSInt(25), connected(false)
	{
	}

	~DataSource()
	{
		Stop();
	}

	inline bool IsThreadRunning()
	{
		return (sThread && sThread->joinable());
	}

	// Returns true if kinect is opened and streaming video
	inline bool IsConnected()
	{
		return connected;
	}

	// responsible for stopping the thread
	virtual void Stop()
	{
		// if we are running
		if (thread_running)
		{
			thread_running = false;     // stops the loop in case it is running

			if (sThread && sThread->joinable())
				sThread->join();

			// makes sure that sthread doesn't point to anything
			sThread = nullptr;
		}

	}


	void Run()
	{
		if (!thread_running && !sThread)
		{
			// starts thread
			sThread.reset(new std::thread(std::bind(&DataSource::thread_main, this)));
		}
	}

	

	// Prints generic information about the data source
	virtual void PrintDataSourceInfo()
	{
		
	}

	// Camera statistics
	DataSourceStatistics statistics;

	const std::string& getDataSourceDescriptor() const
	{
		return dataSourceDescriptor;
	}

	const std::string& getDataSourceSN() const
	{
		return dataSourceSN;
	}

};

