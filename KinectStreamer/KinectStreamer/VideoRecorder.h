#pragma once

#include "Frame.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>
#include <filesystem>
#include <experimental/filesystem> // Header file for pre-standard implementation
#include <memory>
#include <mutex>
#include <set>
#include <map>
#include <queue>
#include <thread>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <opencv2/opencv.hpp>

#include "Logger.h"

class VideoRecorder
{
	// event queue
	boost::asio::io_service io_service;

	// pointer to the thread that will be managing client connections
	std::shared_ptr<std::thread> sThread;

	// are we recording?
	bool isRecordingColor, isRecordingDepth;
	bool acceptNewTasks;
	std::string filenameColor, filenameDepth;
	std::string filePrefix, filePath;
	int takeNumber;

	int framesLeft, framesRecorded;

	cv::VideoWriter colorVideoWriter;
	std::ofstream depthVideoWriter;


	// returns the number of ticks (C# / .NET equivalent) in GMT
	static const long long TicksNow()
	{
		std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
		auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());

		// to suport timezones we can do something like the below.. unfortunately it crashes
		//auto now = date::make_zoned(date::current_zone(), std::chrono::system_clock::now()).get_local_time();

		return ((long long)ticks.count() / (long long)100) + (long long)621355968000000000;
	}



	void RequestInternalStop()
	{
		EndRecording();

		// sanity check
		if (framesLeft != 0)
		{
			Logger::Log("Recorder") << "Missed some frames.. not sure how...: " << FramesLeft() << " frames left" << std::endl;
			framesLeft = 0;
		}

		// frees work
		m_work.reset();
	}

	void EndRecording()
	{
		// are we recording?
		if (colorVideoWriter.isOpened())
		{
			colorVideoWriter.release();
			isRecordingColor = false;
			Logger::Log("Recorder") << "Closed file " << filenameColor << " after recording " << framesRecorded << " frames" << std::endl;
		}

		if (depthVideoWriter.is_open())
		{
			depthVideoWriter.close();
			isRecordingDepth = false;

			Logger::Log("Recorder") << "Closed file " << filenameDepth << " after recording " << framesRecorded << " frames" << std::endl;
		}

		// reset variables 
		framesRecorded = 0;
		filenameColor.clear();
		filenameDepth.clear();
	}

	// work that keeps the thread busy
	std::shared_ptr<boost::asio::io_service::work> m_work;



public:

	VideoRecorder(const std::string& filePrefix = "StandardCamera") : isRecordingColor(false), isRecordingDepth(false), acceptNewTasks(false), takeNumber(0), framesLeft(0), framesRecorded(0)
	{

	}

	~VideoRecorder()
	{
		Stop();
	}

	bool isRunning()
	{
		return (sThread && sThread->joinable());
	}

	void Run()
	{
		if (!isRunning())
			sThread.reset(new std::thread(std::bind(&VideoRecorder::VideoRecorderThreadLoop, this)));
	}

	void Stop()
	{
		if (!isRunning()) return;

		// the videorecorder thread has to stop itself to make sure
		// that we are done recording all video files
		if (isRecordingDepth || isRecordingColor)
		{
			Logger::Log("Recorder") << "Still recording... waiting for recording to end so that files are saved successfully! (" << FramesLeft() << " frames left)" << std::endl;
		}

		// make sure that any requests from now on are ignored
		acceptNewTasks = false;

		// puts a request to stop from the internal event queue (making sure that all video frames queued
		// are saved/handled before that event is processed)
		io_service.post(std::bind(&VideoRecorder::RequestInternalStop, this));

		// is this request happening on a different thread than the recorder's thread?
		if (std::this_thread::get_id() != sThread->get_id())
		{
			// is it running ? wait for thread to finish
			if (sThread && sThread->joinable())
				sThread->join();

			sThread = nullptr;
		}
		
	}

	// basically starts an io_service with "work"
	void VideoRecorderThreadLoop()
	{


		std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
		auto ticks = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());

		std::cout << ((long long) ticks.count() / (long long)100) << std::endl;

		std::cout << ((long long) ticks.count() / (long long) 100) + (long long) 621355968000000000 << std::endl;
		Logger::Log("Recorder") << "Thread started" << std::endl;

		// we can start accepting requests
		acceptNewTasks = true;

		// creates work for the thread
		m_work = std::make_shared<boost::asio::io_service::work>(io_service);

		// runs service
		io_service.run();


		Logger::Log("Recorder") << "Thread ended" << std::endl;
	}


	bool StartRecording(const std::string& path, bool color, bool depth)
	{
		// if not running
		if (!sThread)
		{
			Logger::Log("Recorder") << "Error w/ \"StartRecording\"! Thread is not running!" << std::endl;
			return false;
		}

		// first, let's make sure that this method gets called from the same thread that
		// is running the server
		if (std::this_thread::get_id() != sThread->get_id())
		{
			// we only check this at the time of an external request
			if (!acceptNewTasks)
			{
				Logger::Log("Recorder") << "Error! Thread is exiting and cannot accept new record jobs!" << std::endl;
				return false;
			}

			// posts the request to the VideoRecorder thread
			io_service.post(std::bind(&VideoRecorder::StartRecording, this, path, color, depth));
			return true;
		}


		// everything below is guaranteed to be running on the VideoRecorder thread

		// this is a sanity check more for the sake of complaining that the other party did not request the current
		// recording to stop before requesting a new recording to to start.
		if (isRecordingInProgress())
		{
			Logger::Log("Recorder") << "Received a new request to record while already recording! Stopping current recording..." << std::endl;
			EndRecording();
		}

		// todo: lowercase path before comparing?

		// are we recording to the same path?
		if (path == filePath)
		{
			takeNumber++;
		}
		else {
			takeNumber = 1;
			filePath = path;
		}

		// gets the timestamp for the videoname

		std::stringstream depthFileName, colorFileName;
		depthFileName << filePrefix << "_Depth_Take-" << takeNumber << "_Time-";
		colorFileName << filePrefix << "_Color_Take-" << takeNumber << "_Time-";

		// create file names
		std::experimental::filesystem::path saveFolder(filePath);
		

		// open file handles


		// ready to record!
		isRecordingDepth = depth;
		isRecordingColor = color;
	}

	bool StopRecording()
	{
		// if not running
		if (!sThread)
		{
			Logger::Log("Recorder") << "Error w/ \"StopRecording\"! Thread is not running!" << std::endl;
			return false;
		}

		// first, let's make sure that this method gets called from the same thread that
		// is running the server
		if (std::this_thread::get_id() != sThread->get_id())
		{
			if (!acceptNewTasks)
			{
				// if thread is not accepting new tasks it means that someone already requested it to stop
				// this guarantees that recording will stop too. We don't need to post a request here
				return false;
			}

			io_service.post(std::bind(&VideoRecorder::StopRecording, this));
			return true;
		}

		// everything below is guaranteed to be running on the VideoRecorder thread
	}

	bool RecordFrame(std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
	{
		// if not running
		if (!sThread)
		{
			Logger::Log("Recorder") << "Error w/ \"RecordFrame\"! Thread is not running!" << std::endl;
			return false;
		}

		// first, let's make sure that this method gets called from the same thread that
		// is running the server
		if (std::this_thread::get_id() != sThread->get_id())
		{
			if (!acceptNewTasks)
			{
				Logger::Log("Recorder") << "Error! Thread is exiting and cannot accept new frames!" << std::endl;
				return false;
			}

			++framesLeft;

			io_service.post(std::bind(&VideoRecorder::RecordFrame, this, color, depth));

			return true;
		}

		// are we recording at this point?
		if (isRecordingColor)
		{

		}

		if (isRecordingDepth)
		{

		}

		// everything below is guaranteed to be running on the VideoRecorder thread

		// done recording another frame
		--framesLeft;
	}

	// returns true if there is a recording in progresss
	bool isRecordingInProgress()
	{
		return (isRecordingDepth || isRecordingColor);
	}

	// returns the number of frames queued to record
	int FramesLeft()
	{
		return framesLeft;
	}


};

