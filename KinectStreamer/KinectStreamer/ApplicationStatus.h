#pragma once

#include <string>
#include <mutex>
#include <rapidjson/document.h>

class ApplicationStatus
{
	// this is only used when a recording starts or stops as many fields are changed at once
	std::mutex statusChangeLock;

	std::string recordingColorPath, recordingDepthPath;
	bool isRecordingColor, isRecordingDepth;
	bool _redirectFramesToRecorder;

	// can be set by the configuration file
	int streamerPort;
	int controlPort;

	// can be set by the configuration file
	int streamingClients;
	int streamingMaxFPS;
	int streamingColorWidth, streamingColorHeight;
	int streamingDepthWidth, streamingDepthHeight;
	std::string streamingColorFormat, streamingDepthFormat;
	float streamingColorBitrate, streamingDepthBitrate, streamingCurrentFPS;
	
	// true if the camera is streaming to at least one client?
	bool isStreaming;
	bool isStreamingColor, isStreamingDepth;

	// true if the camera is connected and receiving frames
	std::string cameraName, cameraSerial;
	bool isCameraDepthRunning, isCameraColorRunning;
	int cameraRequestedDepthWidth, cameraRequestedDepthHeight;
	int cameraRequestedColorWidth, cameraRequestedColorHeight;

public:

	ApplicationStatus() : isRecordingColor(false), isRecordingDepth(false), _redirectFramesToRecorder(false), streamerPort(0), controlPort(0),
	streamingClients(0), streamingMaxFPS(0),
	streamingColorWidth(0), streamingColorHeight(0),
	streamingDepthWidth(0), streamingDepthHeight(0),
    streamingColorBitrate(0.0f), streamingDepthBitrate(0.0f), streamingCurrentFPS(0.0f),
	isStreaming(false), isStreamingColor(false), isStreamingDepth(false),
	isCameraDepthRunning(false), isCameraColorRunning(false),
	cameraRequestedDepthWidth(0), cameraRequestedDepthHeight(0),
	cameraRequestedColorWidth(0), cameraRequestedColorHeight(0){};

	// basic getters and setters - changes to these are only guaranteed to have
	// an effect when they happen before the other threads are instantiated
	void SetStreamerPort(int port) { streamerPort = port; }
	void SetControlPort(int port) { controlPort = port; }
	int GetStreamerPort() const { return streamerPort; }
	int GetControlPort() const { return controlPort;  }
	bool isRedirectingFramesToRecorder() const { return _redirectFramesToRecorder;  }
	int GetStreamingColorHeight() const { return streamingColorHeight; }
	int GetStreamingColorWidth() const { return streamingColorWidth; }
	int GetStreamingDepthHeight() const { return streamingDepthHeight; }
	int GetStreamingDepthWidth() const { return streamingDepthWidth; }
	
	void SetStreamingClients(int value) { streamingClients = value; }
	void SetStreamingMaxFPS(int value) { streamingMaxFPS = value;  }
	int GetStreamingMaxFPS() const { return streamingMaxFPS; }
	int GetStreamingClients() const { return streamingClients;}
	void SetStreamingStatus(bool value) { isStreaming = value; if (!value) { isStreamingColor = false; isStreamingDepth = false; } }
	bool GetStreamingStatus() const { return isStreaming; }
	bool IsCameraRunning() const { return isStreamingColor || isStreamingDepth;  }
	float GetCurrentStreamingFPS() const { return streamingCurrentFPS;  }

	// this function sets the current streaming FPS. This should be updated by the class
	// responsible for streaming
	void SetCurrentStreamingFPS(float value) { streamingCurrentFPS = value;  }

	// create a function to read this from a configuration file
	bool LoadConfiguration(const std::string& filepath);
	bool SaveConfiguration(const std::string& filepath);

	/**
	 * Updates the application status internally (recording)
	 **/
	void UpdateRecordingStatus(bool readyToStartRecording, bool isRecordingColor, bool isRecordingDepth, const std::string& colorPath = std::string(), const std::string& depthPath = std::string())
	{
		std::lock_guard<std::mutex> guard(statusChangeLock);
		{
			// tells other threads in the application that the video recorder
			// thread is ready to start recording
			_redirectFramesToRecorder = readyToStartRecording;

			// saves information that can be used across the application
			// to report what is being recorded
			isRecordingColor = isRecordingColor;
			isRecordingDepth = isRecordingDepth;
			recordingColorPath = colorPath;
			recordingDepthPath = depthPath;
		}
	}

	/**
	 * Updates the application status internally (streaming)
	 */
	void UpdateCaptureStatus(bool isColorCameraRunning, bool isDepthCameraRunning,
		int colorCameraWidth = 0, int colorCameraHeight = 0,
		int colorStreamWidth = 0, int colorStreamHeight = 0,
		int depthCameraWidth = 0, int depthCameraHeight = 0,
		int depthStreamWidth = 0, int depthStreamHeight = 0)
	{

		std::lock_guard<std::mutex> guard(statusChangeLock);
		{
			// cameras running
			this->isCameraColorRunning = isColorCameraRunning;
			this->isCameraDepthRunning = isDepthCameraRunning;

			// resolution capture
			this->cameraRequestedColorWidth = colorCameraWidth;
			this->cameraRequestedColorHeight = colorCameraHeight;
			this->cameraRequestedDepthWidth = depthCameraWidth;
			this->cameraRequestedDepthHeight = depthCameraHeight;

			// resolution stream
			this->streamingColorWidth  = colorStreamWidth;
			this->streamingColorHeight = colorStreamHeight;
			this->streamingDepthWidth = depthStreamWidth;
			this->streamingDepthHeight  = depthStreamHeight;
		}
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
	 * returns a RapidJson::Document with a summary of what 
	 * the application is doing at the time
	 */
	rapidjson::Document&& GetApplicationStatusJSON();
};
