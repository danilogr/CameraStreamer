#pragma once

#include "Frame.h"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <functional>
#include <filesystem>
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
	int externalColorTakeNumber, externalDepthTakeNumber, externalColorWidth, externalColorHeight, externalDepthWidth, externalDepthHeight;

	std::string filePrefix, colorFolderPath, depthFolderPath;
	int framesLeft, internalColorFramesRecorded, internalDepthFramesRecorded, internalColorFramesDropped, internalDepthFramesDropped;

	


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
			Logger::Log("Recorder") << "Closed file " << internalFilenameColor << " after recording " << internalColorFramesRecorded << " frames (" << internalColorFramesDropped << " dropped)" << std::endl;
		}

		if (depthVideoWriter.is_open())
		{
			depthVideoWriter.close();
			Logger::Log("Recorder") << "Closed file " << internalFilenameDepth << " after recording " << internalDepthFramesRecorded << " frames (" << internalDepthFramesDropped << " dropped)" << std::endl;
		}

		// reset variables 
		internalColorFramesRecorded = internalDepthFramesRecorded = internalColorFramesDropped = internalDepthFramesDropped  = 0;
		internalFilenameColor.clear();
		internalFilenameDepth.clear();
		internalIsRecordingDepth = false;
		internalIsRecordingColor = false;
	}

	// similar to InternalStopRecording. This method is guaranteed to be called by the VideoRecorder thread
	void InternalStartRecording(const std::string& colorPath, const std::string& depthPath, bool recordColor, bool recordDepth,
								int colorWidth, int colorHeight, int depthWidth, int depthHeight)
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

		try
		{
			// prepare the files
			if (internalIsRecordingColor)
			{
	//			vw.open(filenameDepth, cv::VideoWriter::fourcc('F', 'M', 'P', '4'), 30, cv::Size(KinectV2Source::cColorWidth, KinectV2Source::cColorHeight));
				colorVideoWriter.open(internalFilenameColor, cv::VideoWriter::fourcc('F', 'M', 'P', '4'), 30, cv::Size(colorWidth, colorHeight));
			}
		}
		catch (const std::exception& e)
		{
			internalIsRecordingColor = false; // sorry
			Logger::Log("Recorder") << "Error creating color video stream: " << e.what() << std::endl;
		}

		try
		{
			if (internalIsRecordingDepth)
			{
				// open depth file
				depthVideoWriter.open(internalFilenameDepth, std::ios::out | std::ios::binary);

				// save ugly depth header
				std::stringstream header;
				header << "{\"filetype\":\"depth\", \"datatype\": \"numpy.int16\", \"resolution\": [";
				header << depthWidth << ", " << depthHeight << "]}\n";
				std::string headerStr = header.str();
				depthVideoWriter.write(headerStr.c_str(), headerStr.length());
			}
		}
		catch (const std::exception & e)
		{
			internalIsRecordingDepth = false; // sorry
			Logger::Log("Recorder") << "Error creating deth video stream: " << e.what() << std::endl;
		}

	}

	// work that keeps the thread busy
	std::shared_ptr<boost::asio::io_service::work> m_work;


	// this method is called internally when it is time to save a frame to file
	// pointers to the depth or color frame might be empty if we are not recording one of the streams
	void InternalRecordFrame(long long ticksSoFar, std::shared_ptr<Frame> colorFrame, std::shared_ptr<Frame> depthFrame)
	{

		if (colorFrame)
		{
			if (internalIsRecordingColor && colorVideoWriter.isOpened())
			{
				try
				{
					cv::Mat frame(colorFrame->getHeight(), colorFrame->getWidth() , CV_8UC4, colorFrame->getData());
					colorVideoWriter.write(frame);
					++internalColorFramesRecorded;
				}
				catch (const std::exception& e)
				{
					++internalColorFramesDropped;
				}
			}
			else {
				++internalColorFramesDropped;
			}
		}

		if (depthFrame)
		{
			if (internalIsRecordingDepth && depthVideoWriter.is_open())
			{
				try
				{
					depthVideoWriter.write((const char*)& ticksSoFar, sizeof(long long));
					depthVideoWriter.write((const char*) depthFrame->getData(), depthFrame->size());
					++internalDepthFramesRecorded;
				}
				catch (const std::exception & e)
				{
					++internalDepthFramesDropped;
				}
			}
			else {
				++internalDepthFramesDropped;
			}
		}


		// done recording another frame
		--framesLeft;
	}

public:

	VideoRecorder(std::shared_ptr<ApplicationStatus> appStatus, const std::string& filePrefix = "StandardCamera") :
	appStatus(appStatus), acceptNewTasks(false), internalIsRecordingColor(false), internalIsRecordingDepth(false),
	externalIsRecordingColor(false), externalIsRecordingDepth(false), externalColorTakeNumber(1), externalDepthTakeNumber(1),
	externalColorWidth(0), externalColorHeight(0), externalDepthWidth(0), externalDepthHeight(0), filePrefix(filePrefix),
	framesLeft(0), internalColorFramesRecorded(0), internalDepthFramesRecorded(0), internalColorFramesDropped(0), internalDepthFramesDropped(0)
	{

	}

	~VideoRecorder()
	{
		Stop();
	}

	bool IsThreadRunning()
	{
		return (sThread && sThread->joinable());
	}

	void Run()
	{
		if (!IsThreadRunning())
			sThread.reset(new std::thread(std::bind(&VideoRecorder::VideoRecorderThreadLoop, this)));
	}

	void Stop()
	{
		// can't stop what is not running ;)
		if (!IsThreadRunning()) return;

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


	bool StartRecording(bool color, bool depth, const std::string& colorPath, const std::string& depthPath)
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

		// is the camera streaming at all?
		if (!appStatus->IsAppCapturing())
		{
			Logger::Log("Recorder") << "Error! Cannot record without a camera..." << std::endl;
			return false;
		}

		// can we record? what's the resolution so far
		if (color && !appStatus->IsColorCameraEnabled())
		{
			Logger::Log("Recorder") << "Error! Cannot record color frames as camera is not streaming color (yet)..." << std::endl;
			return false;
		}

		if (depth && !appStatus->IsDepthCameraEnabled())
		{
			Logger::Log("Recorder") << "Error! Cannot record depth frames as camera is not streaming depth (yet)..." << std::endl;
			return false;
		}

		// yay, we are good to record!
		externalColorHeight = appStatus->GetStreamingHeight();
		externalColorWidth  = appStatus->GetStreamingWidth();
		externalDepthHeight = appStatus->GetStreamingHeight();
		externalDepthWidth  = appStatus->GetStreamingWidth();

		// are we recording to the same path?
		// (this might happen if the camera gets unplugged and plugged back again)
		if (color)
		{
			if (colorPath == colorFolderPath)
			{
				externalColorTakeNumber++;
			}
			else {
				externalColorTakeNumber = 1;
				colorFolderPath = colorPath;
			}
		}

		if (depth)
		{
			if (depthPath == depthFolderPath)
			{
				externalDepthTakeNumber++;
			}
			else {
				externalDepthTakeNumber = 1;
				depthFolderPath = depthPath;
			}
		}

		// create file names
		std::string colorVideoPath, depthVideoPath;
		long long timestampNow = TicksNow();

		// creates file names
		{
			std::stringstream depthFileName, colorFileName;
			colorFileName << filePrefix << "_Color_Take-" << externalColorTakeNumber << "_Time-" << timestampNow << ".mp4";
			depthFileName << filePrefix << "_Depth_Take-" << externalDepthTakeNumber << "_Time-" << timestampNow << ".depth.artemis";
		
			// figure out paths
			std::filesystem::path colorVideoP(colorFolderPath), depthVideoP(depthFolderPath);
			colorVideoP.append(colorFileName.str());
			depthVideoP.append(depthFileName.str());

			// save them
			colorVideoPath = colorVideoP.string();
			depthVideoPath = depthVideoP.string();
		}

		// tell others that the VideoRecorder thread can start receiving frames
		externalIsRecordingColor = color;
		externalIsRecordingDepth = depth;

		// we start recording internally
		// whenever possible, that is (adds event to the end of the queue)
		io_service.post(std::bind(&VideoRecorder::InternalStartRecording, this, colorVideoPath, depthVideoPath, color, depth, externalColorWidth, externalColorHeight, externalDepthWidth, externalDepthHeight));
		
		// we start accepting frame requests
		appStatus->UpdateRecordingStatus(true, color, depth, colorVideoPath, depthVideoPath);

		// logs what just happened
		Logger::Log("Recorder") << "Request to record to " << colorVideoPath << " and "  <<  depthVideoPath << " processed succesfully!" << std::endl;
			
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
			appStatus->UpdateRecordingStatus(false, false, false);

			// we can also stop receiving frames
			externalIsRecordingColor = false;
			externalIsRecordingDepth = false;

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

		// drop memory references that we are not using
		// drop frames with the wrong resolution
		if (!externalIsRecordingColor)
			color.reset();
		else
		{
			if (!color || color->getWidth() != externalColorWidth || color->getHeight() != externalColorHeight)
			{
				Logger::Log("Recorder") << "Error! Invalid color frame size (Expected " << externalColorWidth << "x" << externalColorHeight << ")" << std::endl;
				return false;
			}
		}

		if (!externalIsRecordingDepth)
			depth.reset();
		else
		{
			if (!depth || depth->getWidth() != externalDepthWidth || depth->getHeight() != externalDepthHeight)
			{
				Logger::Log("Recorder") << "Error! Invalid depth frame size (Expected " << externalDepthWidth << "x" << externalDepthHeight << ")" << std::endl;
				return false;
			}
		}

		// good to go!
		io_service.post(std::bind(&VideoRecorder::InternalRecordFrame, this, timenow, color, depth));
		return true;
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

