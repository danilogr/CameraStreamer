#pragma once

#include <string>
#include <mutex>

struct ApplicationStatus
{
	// this is only used when a recording starts or stops as many fields are changed at once
	std::mutex statusChangeLock;

	std::string recordingColorPath, recordingDepthPath;
	bool isRecordingColor, isRecordingDepth;
	bool _redirectFramesToRecorder;

	int streamerPort;
	int controlPort;

	int streamingColorWidth, streamingColorHeight;
	int streamingDepthWidth, streamingDepthHeight;

	bool isStreaming;
	bool isCameraRunning;

	ApplicationStatus() : isRecordingColor(false), isRecordingDepth(false), _redirectFramesToRecorder(false), streamerPort(0), controlPort(0),
	streamingColorWidth(0), streamingColorHeight(0),
	streamingDepthWidth(0), streamingDepthHeight(0),
	isStreaming(false), isCameraRunning(false) {};

};
