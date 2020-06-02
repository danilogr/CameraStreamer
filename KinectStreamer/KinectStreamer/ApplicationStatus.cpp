#include "ApplicationStatus.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

rapidjson::Document&& ApplicationStatus::GetApplicationStatusJSON()
{
	rapidjson::Document applicationStatusJson;
	applicationStatusJson.SetObject();
	rapidjson::Document::AllocatorType& allocator = applicationStatusJson.GetAllocator();
	
	// locks to make sure that the entire json object is consistent
	std::lock_guard<std::mutex> guard(statusChangeLock);
	{
		// camera settings
		applicationStatusJson.AddMember("cameraRunning", isCameraColorRunning || isCameraDepthRunning, allocator);
		applicationStatusJson.AddMember("cameraName", rapidjson::Value().SetString(cameraName.c_str(), cameraName.length(), allocator), allocator);
		applicationStatusJson.AddMember("cameraSerial", rapidjson::Value().SetString(cameraSerial.c_str(), cameraSerial.length(), allocator), allocator);
		applicationStatusJson.AddMember("cameraDepth", isCameraDepthRunning, allocator);
		applicationStatusJson.AddMember("cameraColor", isCameraColorRunning, allocator);
		applicationStatusJson.AddMember("cameraDepthWidth", cameraRequestedDepthWidth, allocator);
		applicationStatusJson.AddMember("cameraDeptHeight", cameraRequestedDepthHeight, allocator);
		applicationStatusJson.AddMember("cameraColorWidth", cameraRequestedColorWidth, allocator);
		applicationStatusJson.AddMember("cameraColorHeight", cameraRequestedColorHeight, allocator);
		
		// streaming server
		applicationStatusJson.AddMember("streaming", isStreamingColor || isStreamingDepth, allocator);	// true if streaming either color, depth, or both
		applicationStatusJson.AddMember("streamingClients", streamingClients, allocator);				// number of clients currently connected to the stream
		applicationStatusJson.AddMember("streamingMaxFPS", streamingMaxFPS, allocator);						// FPS of the stream
		applicationStatusJson.AddMember("streamingColor", isStreamingColor, allocator);
		applicationStatusJson.AddMember("streamingColorWidth", streamingColorWidth, allocator);
		applicationStatusJson.AddMember("streamingColorFormat", rapidjson::Value().SetString(streamingColorFormat.c_str(), streamingColorFormat.length(), allocator), allocator);
		applicationStatusJson.AddMember("streamingColorBitrate", streamingColorBitrate, allocator);
		applicationStatusJson.AddMember("streamingDepth", isStreamingColor, allocator);
		applicationStatusJson.AddMember("streamingDepthWidth", streamingDepthWidth, allocator);
		applicationStatusJson.AddMember("streamingDepthHeight", streamingDepthHeight, allocator);
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

	return std::move(applicationStatusJson);
}

bool LoadConfiguration(const std::string& filepath)
{

}