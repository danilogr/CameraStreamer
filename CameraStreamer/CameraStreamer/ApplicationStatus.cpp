#include "ApplicationStatus.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/reader.h>
#include <rapidjson/error/error.h>

#include <fstream>
#include "Logger.h"


rapidjson::Document ApplicationStatus::GetApplicationStatusJSON()
{

	rapidjson::Document applicationStatusJson;
	applicationStatusJson.SetObject();
	rapidjson::Document::AllocatorType& allocator = applicationStatusJson.GetAllocator();
	
	// locks to make sure that the entire json object is consistent
	std::lock_guard<std::mutex> guard(dataLock);
	{

		// camera settings
		applicationStatusJson.AddMember("capturing", IsAppCapturing(), allocator);
		applicationStatusJson.AddMember("captureDeviceUserDefinedName", rapidjson::Value().SetString(cameraUserDefinedName.c_str(), cameraUserDefinedName.length(), allocator), allocator);
		applicationStatusJson.AddMember("captureDeviceType", rapidjson::Value().SetString(cameraType.c_str(), cameraType.length(), allocator), allocator);
		applicationStatusJson.AddMember("captureDeviceSerial", rapidjson::Value().SetString(cameraSerial.c_str(), cameraSerial.length(), allocator), allocator);
		applicationStatusJson.AddMember("capturingDepth", isCameraDepthRunning, allocator);
		applicationStatusJson.AddMember("capturingColor", isCameraColorRunning, allocator);
		applicationStatusJson.AddMember("captureDepthWidth", cameraDepthWidth, allocator);
		applicationStatusJson.AddMember("captureDepthHeight", cameraDepthHeight, allocator);
		applicationStatusJson.AddMember("captureColorWidth", cameraColorWidth, allocator);
		applicationStatusJson.AddMember("captureColorHeight", cameraColorHeight, allocator);
		
		// streaming server
		applicationStatusJson.AddMember("streaming", IsAppStreaming(), allocator);	  // true if streaming either color, depth, or both
		applicationStatusJson.AddMember("streamingClients", streamingClients, allocator); // number of clients currently connected to the stream
		applicationStatusJson.AddMember("streamingMaxFPS", streamingMaxFPS, allocator);	  // FPS of the stream
		applicationStatusJson.AddMember("streamingCameraParameters", rapidjson::Value().SetString(calibrationMatrix.c_str(), calibrationMatrix.length(), allocator), allocator);
		applicationStatusJson.AddMember("streamingColor", isStreamingColor, allocator);
		applicationStatusJson.AddMember("streamingColorWidth", streamingWidth, allocator);
		applicationStatusJson.AddMember("streamingColorHeight", streamingHeight, allocator);
		applicationStatusJson.AddMember("streamingColorFormat", rapidjson::Value().SetString(streamingColorFormat.c_str(), streamingColorFormat.length(), allocator), allocator);
		applicationStatusJson.AddMember("streamingColorBitrate", streamingColorBitrate, allocator);
		applicationStatusJson.AddMember("streamingDepth", isStreamingDepth, allocator);
		applicationStatusJson.AddMember("streamingDepthWidth", streamingWidth, allocator);
		applicationStatusJson.AddMember("streamingDepthHeight", streamingHeight, allocator);
		applicationStatusJson.AddMember("streamingDepthFormat", rapidjson::Value().SetString(streamingDepthFormat.c_str(), streamingDepthFormat.length(), allocator), allocator);
		applicationStatusJson.AddMember("streamingDepthBitrate", streamingDepthBitrate, allocator);

		// recording
		applicationStatusJson.AddMember("recording", isRecordingColor || isRecordingDepth, allocator);
		applicationStatusJson.AddMember("recordingColor", isRecordingColor, allocator);
		applicationStatusJson.AddMember("recordingDepth", isRecordingDepth, allocator);
		applicationStatusJson.AddMember("recordingDepthPath", rapidjson::Value().SetString(recordingDepthPath.c_str(), recordingDepthPath.length(), allocator), allocator);
		applicationStatusJson.AddMember("recordingColorPath", rapidjson::Value().SetString(recordingColorPath.c_str(), recordingColorPath.length(), allocator), allocator);
		
		// application ports
		applicationStatusJson.AddMember("port", streamerPort, allocator);
		applicationStatusJson.AddMember("controlPort", controlPort, allocator);
	}

	return applicationStatusJson;
}
