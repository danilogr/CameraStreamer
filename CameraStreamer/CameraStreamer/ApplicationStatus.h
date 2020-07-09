#pragma once

// several of the rerported by ApplicationStatus are a replica of what
// we have in the configuration file
#include "Configuration.h"

#include <string>
#include <mutex>


#include <rapidjson/document.h>

/**
   ApplicationStatus is data structure used to synchronize action
   accross threads of this class. ApplicationStatus is also responsible
   for sorting and reporting the **current** parameters for a specific
   session. For example, while the requested FPS might be 30, the actual FPS might be 15;
   thus, ApplicationStatus will report 15

   To leverage existing members from the Configuration class, ApplicationStatus inherits 
   from it

  */
class ApplicationStatus : public Configuration
{
protected:
	//
	// Recording related
	//

	// recording: path for color and depth files
	std::string recordingColorPath, recordingDepthPath;
	// recording: are we recording?
	bool isRecordingColor, isRecordingDepth;
	// recording: should the camera callback send frames to the recording thread?
	bool _redirectFramesToRecorder;


	//
	// Streaming thread
	//

	// streamer: how many clients are currently connected to the stream? (only possible through TCP)
	int streamingClients;

	// streamer: bitrate for each stream
	float streamingColorBitrate, streamingDepthBitrate;
	
	// streamer: current FPS it is able to stream ? (can I even find this out?)
	float streamingCurrentFPS;

	//
	// Camera related
	// 
	// true if the camera is currently running / receiving fraems
	bool isCameraDepthRunning, isCameraColorRunning;

	// the calibration matrix of the camera currently running
	std::string calibrationMatrix;


	

public:


	ApplicationStatus() : isRecordingColor(false), isRecordingDepth(false), _redirectFramesToRecorder(false), 
	streamingClients(0), streamingColorBitrate(0.0f), streamingDepthBitrate(0.0f), streamingCurrentFPS(0.0f),
	isCameraDepthRunning(false), isCameraColorRunning(false) {};

	// Copies some values from the configuration
	void UpdateAppStatusFromConfig(const Configuration& config)
	{
		std::lock_guard<std::mutex> guard(dataLock);

		// camera
		cameraType = config.GetCameraType();
		cameraUserDefinedName = config.GetCameraUserDefinedName();

		// tcp servers
		controlPort = config.GetControlPort();
		streamerPort = config.GetStreamerPort();

		// streaming protocol
		streamingColorFormat = config.GetStreamingColorFormat();
		streamingDepthFormat = config.GetStreamingDepthFormat();
	}

	// ============================================================================

	//
	// Recording related
	//
	
	bool isRedirectingFramesToRecorder() const { return _redirectFramesToRecorder;  }

	// ============================================================================

	//
	// Streaming related status
	//

	// streaming summary status: are we streaming any camera?
	bool IsAppStreaming() const { return isStreamingColor || isStreamingDepth; }

	// equivalent to calling SetStreamingColorEnable(false) and SetStreamingDepthEnable(false)
	void SetStreamingDisabled() { isStreamingColor = false; isStreamingDepth = false; }
	
	// number of clients connected
	int GetStreamingClients() const { return streamingClients; }
	void SetStreamingClients(int value) { streamingClients = value; }
	
	// throttling max fps
	void SetStreamingMaxFPS(int value) { streamingMaxFPS = value;  }
	int GetStreamingMaxFPS() const { return streamingMaxFPS; }

	// current fps (not even sure I can calculate that)
	void SetCurrentStreamingFPS(float value) { streamingCurrentFPS = value; }
	
	// ============================================================================

	//
	// Camera realted
	//

	bool IsAppCapturing() const { return isCameraDepthRunning || isCameraColorRunning;  }
	float GetCurrentStreamingFPS() const { return streamingCurrentFPS;  }

	bool IsDepthCameraEnabled() const { return isCameraDepthRunning; }
	bool IsColorCameraEnabled() const { return isCameraColorRunning; }
	//bool IsInfraredCameraEnabled() const { return requestInfraredCamera; }
	

	// ============================================================================

	/**
	 * Updates the application status internally (recording)
	 **/
	void UpdateRecordingStatus(bool readyToStartRecording, bool isRecordingColor, bool isRecordingDepth, const std::string& colorPath = std::string(), const std::string& depthPath = std::string())
	{
		std::lock_guard<std::mutex> guard(dataLock);

		// tells other threads in the application that the video recorder
		// thread is ready to start recording
		_redirectFramesToRecorder = readyToStartRecording;

		// what streams are being recorded and where?
		this->isRecordingColor = isRecordingColor;
		this->isRecordingDepth = isRecordingDepth;
		this->recordingColorPath = colorPath;
		this->recordingDepthPath = depthPath;

	}

	/**
	 * Updates the application status internally (streaming)
	 */
	void UpdateCaptureStatus(bool isColorCameraRunning, bool isDepthCameraRunning,
		const std::string& sn = std::string(), const std::string& calibrationMatrix = std::string(),
		int colorCameraWidth = 0, int colorCameraHeight = 0,
		int depthCameraWidth = 0, int depthCameraHeight = 0,
		int streamWidth = 0, int streamHeight = 0)
	{
		std::lock_guard<std::mutex> guard(dataLock);



		// camera serial number
		this->cameraSerial = sn;

		// cameras running
		this->isCameraColorRunning = isColorCameraRunning;
		this->isCameraDepthRunning = isDepthCameraRunning;

		// resolution capture
		this->cameraColorWidth = colorCameraWidth;
		this->cameraColorHeight = colorCameraHeight;
		this->cameraDepthWidth = depthCameraWidth;
		this->cameraDepthHeight = depthCameraHeight;

		// resolution stream
		this->streamingWidth  = streamWidth;
		this->streamingHeight = streamHeight;

		// calibration matrix
		this->calibrationMatrix = calibrationMatrix;

	}

	/**
	 * This method signal threads in this application that the 
	 * video recorder thread is ready to start recording
	 */
	void ApplicationReadyToRecord(bool status=false)
	{
		_redirectFramesToRecorder = status;
	}

	/**
	 * Returns a summary of the application as a rapidjson document
	 */
	rapidjson::Document GetApplicationStatusJSON();

};
