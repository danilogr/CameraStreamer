#pragma once

#include <string>
#include <mutex>

struct ApplicationStatus
{
	// this is only used when a recording starts or stops as many fields are changed at once
	std::mutex statusChangeLock;

	std::string recordingPath, recordingColorPath, recordingDepthPath;
	bool isRecordingColor, isRecordingDepth;
	bool _redirectFramesToRecorder;

	int streamerPort;
	int controlPort;
	bool isStreaming;
	

	bool isTCPStreamerServerRunning, isCameraRunning;

	ApplicationStatus() : isRecordingColor(false), isRecordingDepth(false), _redirectFramesToRecorder(false), streamerPort(0), controlPort(0), isStreaming(false), isTCPStreamerServerRunning(false), isCameraRunning(false) {};

};
