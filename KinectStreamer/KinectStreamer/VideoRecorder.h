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

#include "ApplicationStatus.h"


//#include <date/date.h>
//#include <date/tz.h>

#include "Logger.h"

class VideoRecorder
{

	std::shared_ptr<ApplicationStatus> appStatus;

	// event queue
	boost::asio::io_service io_service;

	// pointer to the thread that will be managing client connections
	std::shared_ptr<std::thread> sThread;

	// true if the thread is running and waiting for new jobs
	// (false if someone requests the thread to stop)
	bool acceptNewTasks;

	// are we recording? these variables are internal only
	// and represent what is happening with respect to the queue
	// (isRecordingColor/isRecordingDepth will be true until a StopRequest
	// is processed within the thread)
	bool internalIsRecordingColor, internalIsRecordingDepth;
	std::string internalFilenameColor, internalFilenameDepth, internalFilePath;
	cv::VideoWriter colorVideoWriter;
	std::ofstream depthVideoWriter;


	// externalIsRecordingColor/externalIsRecordingDepth represent the status of VideoRecorder
	// when all events have been processed (and not what it is currently doing)
	// (they are the same as what is repoerted in appStatus)
	bool externalIsRecordingColor, externalIsRecordingDepth;
	int externalTakeNumber;

	std::string filePrefix, filePath;
	int framesLeft, internalFramesRecorded;


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
		// let's wrap up with recording
		InternalStopRecording();

		// sanity check
		if (framesLeft != 0)
		{
			Logger::Log("Recorder") << "Missed some frames.. not sure how...: " << FramesLeft() << " frames left" << std::endl;
			framesLeft = 0;
		}

		// frees work
		m_work.reset();
	}


	// this method is guaranteed to be called by the VideoRecorder thread
	// it represents the internal state of the VideoRecorder (e.g.: internally,
	// VideoRecorder could be stopping a video stream while an outside call
	// has already been queued to start a new recording. The external call of the
	// VideoRecorder will be "recording")
	void InternalStopRecording()
	{
		// are we recording?
		if (colorVideoWriter.isOpened())
		{
			colorVideoWriter.release();
			Logger::Log("Recorder") << "Closed file " << internalFilenameColor << " after recording " << internalFramesRecorded << " frames" << std::endl;
		}

		if (depthVideoWriter.is_open())
		{
			depthVideoWriter.close();
			Logger::Log("Recorder") << "Closed file " << internalFilenameDepth << " after recording " << internalFramesRecorded << " frames" << std::endl;
		}

		// reset variables 
		internalFramesRecorded = 0;
		internalFilenameColor.clear();
		internalFilenameDepth.clear();
		internalIsRecordingDepth = false;
		internalIsRecordingColor = false;
	}

	// similar to InternalStopRecording. This method is guaranteed to be called by the VideoRecorder thread
	void InternalStartRecording(const std::string& colorPath, const std::string& depthPath, bool recordColor, bool recordDepth)
	{
		// there is a small change that an external request to start a new recording
		// while a recording is already happening will be ignored because another thread
		// requested VideoRecorder to stop. In this unlikely scenario, we have to close the previous recording gracefully

		if (internalIsRecordingColor || internalIsRecordingDepth)
			InternalStopRecording();

		// now are ready to record again

		// updates the state
		internalFilenameColor = colorPath;
		internalFilenameDepth = depthPath;
		internalIsRecordingColor = recordColor;
		internalIsRecordingDepth = recordDepth;

		// prepare the files
		if (internalIsRecordingColor)
		{
//			vw.open(filenameDepth, cv::VideoWriter::fourcc('F', 'M', 'P', '4'), 30, cv::Size(KinectV2Source::cColorWidth, KinectV2Source::cColorHeight));

		}

		if (internalIsRecordingDepth)
		{
			// open depth file

			// save ugly depth header
		}


		// done recording another frame
		++internalFramesRecorded;
		--framesLeft;
	}

	// work that keeps the thread busy
	std::shared_ptr<boost::asio::io_service::work> m_work;


	// this method is called internally when it is time to save a frame to file
	// pointers to the depth or color frame might be empty if we are not recording one of the streams
	void InternalRecordFrame(long long ticksSoFar, std::shared_ptr<Frame> colorFrame, std::shared_ptr<Frame> depthFrame)
	{

	}

	// This method updates appStatus (ApplicationStatus) so that Ping/Pong messages contain the right
	// information about recording status
	void UpdateAppStatus(const std::string& filePath = std::string(), const std::string& colorPath = std::string(), const std::string& depthPath = std::string())
	{
		std::lock_guard<std::mutex> guard(appStatus->statusChangeLock);
		{
			appStatus->isRecordingColor = externalIsRecordingColor;
			appStatus->isRecordingDepth = externalIsRecordingDepth;
			appStatus->recordingColorPath = colorPath;
			appStatus->recordingDepthPath = depthPath;
			appStatus->recordingPath = filePath;
		}
	}

public:



	VideoRecorder(std::shared_ptr<ApplicationStatus> appStatus, const std::string& filePrefix = "StandardCamera") :
	appStatus(appStatus), acceptNewTasks(false), internalIsRecordingColor(false), internalIsRecordingDepth(false),
	externalIsRecordingColor(false), externalIsRecordingDepth(false), externalTakeNumber(1), framesLeft(0), internalFramesRecorded(0)
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
		// can't stop what is not running ;)
		if (!isRunning()) return;

		// the videorecorder thread has to stop itself to make sure
		// that we are done recording all video files
		if (isRecordingInProgress())
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

		acceptNewTasks = false;


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

		// we only check this at the time of an external request
		if (!acceptNewTasks)
		{
			Logger::Log("Recorder") << "Error! Thread is exiting and cannot accept new record jobs!" << std::endl;
			return false;
		}

		// recording to stop before requesting a new recording to to start.
		if (isRecordingInProgress())
		{
			Logger::Log("Recorder") << "Received a new request to record while already recording! Stopping current recording..." << std::endl;
			StopRecording();
		}

		// are we recording to the same path?
		if (path == filePath)
		{
			externalTakeNumber++;
		}
		else {
			externalTakeNumber = 1;
			filePath = path;
		}

		// create file names
		std::string colorVideoPath, depthVideoPath;
		long long timestampNow = TicksNow();

		// creates file names
		{
			std::stringstream depthFileName, colorFileName;
			colorFileName << filePrefix << "_Color_Take-" << externalTakeNumber << "_Time-" << timestampNow << ".mp4";
			depthFileName << filePrefix << "_Depth_Take-" << externalTakeNumber << "_Time-" << timestampNow << ".depth.artemis";
		
			// figure out paths
			std::experimental::filesystem::path colorVideoP(filePath), depthVideoP(filePath);
			colorVideoP.append(colorFileName.str());
			depthVideoP.append(depthFileName.str());

			// save them
			colorVideoPath = colorVideoP.string();
			depthVideoPath = depthVideoP.string();
		}

		// tell others that the VideoRecorder thread can start receiving frames
		appStatus->_redirectFramesToRecorder = true;
		externalIsRecordingColor = color;
		externalIsRecordingDepth = depth;

		// let others know that we are recording
		UpdateAppStatus(filePath, colorVideoPath, depthVideoPath);

		// now we start recording internally
		// whenever possible, that is (adds event to the end of the queue)
		io_service.post(std::bind(&VideoRecorder::InternalStartRecording, this, colorVideoPath, depthVideoPath, color, depth));
		
		// logs what just happened
		Logger::Log("Recorder") << "Request to record to " << path  << " processed succesfully!" << std::endl;
			
		return true;
	}

	bool StopRecording()
	{
		// if not running
		if (!sThread)
		{
			Logger::Log("Recorder") << "Error w/ \"StopRecording\"! Thread is not running!" << std::endl;
			return false;
		}

		if (isRecordingInProgress())
		{
			Logger::Log("Recorder") << "Request to stop recording processed succesfully!" << std::endl;

			// we can already tell cameras to stop sending us frames
			appStatus->_redirectFramesToRecorder = false;

			// we can also stop receiving frames
			externalIsRecordingColor = false;
			externalIsRecordingDepth = false;

			// stops recording externally
			UpdateAppStatus();
			
			// stops recording internally
			io_service.post(std::bind(&VideoRecorder::InternalStopRecording, this));
			return true;
		}

		Logger::Log("Recorder") << "Cannot stop recording when there is no recording in progress!" << std::endl;
		return false;
	}

	bool RecordFrame(std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
	{
		long long timenow = TicksNow();

		// if not running
		if (!sThread)
		{
			Logger::Log("Recorder") << "Error w/ \"RecordFrame\"! Thread is not running!" << std::endl;
			return false;
		}

		if (!acceptNewTasks)
		{
			Logger::Log("Recorder") << "Error! Thread is exiting and cannot accept new frames!" << std::endl;
			return false;
		}

		// writes down that we have another frame to record
		++framesLeft;
		io_service.post(std::bind(&VideoRecorder::InternalRecordFrame, this, timenow, color, depth));
	}

	// returns true if there is a recording in progresss
	bool isRecordingInProgress()
	{
		return (externalIsRecordingColor || externalIsRecordingDepth);
	}

	// returns the number of frames queued to record
	int FramesLeft()
	{
		return framesLeft;
	}


};

