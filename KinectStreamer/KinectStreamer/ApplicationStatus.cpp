#include "ApplicationStatus.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/reader.h>
#include <rapidjson/error/error.h>

#include <fstream>
#include "Logger.h"

#define ReadJSONDefaultInt(d,name,destination,defaultvalue,warn) if (d.HasMember(name) && d[name].IsNumber()) { destination = d[name].GetInt(); } else { destination = defaultvalue; if (warn) { Logger::Log("Config") << "Error! Element \""<< name << "\" should have a valid integer! Using default: " << defaultvalue << std::endl; } }
#define ReadJSONDefaultFloat(d,name,destination,defaultvalue,warn) if (d.HasMember(name) && d[name].IsNumber()) { destination = d[name].GetFloat(); } else { destination = defaultvalue; if (warn) { Logger::Log("Config") << "Error! Element \""<< name << "\" should have a valid float! Using default: " << defaultvalue  << std::endl; } }
#define ReadJSONDefaultBool(d,name,destination,defaultvalue,warn) if (d.HasMember(name) && d[name].IsBool()) { destination = d[name].GetBool(); } else { destination = defaultvalue; if (warn) { Logger::Log("Config") << "Error! Element \""<< name << "\" should have a valid boolean! Using default: " << defaultvalue  << std::endl; } }
#define ReadJSONDefaultString(d,name,destination,defaultvalue,warn) if (d.HasMember(name)) { destination = d[name].GetString(); } else { destination = defaultvalue;  if (warn) { Logger::Log("Config") << "Error! Element \""<< name << "\" should have a valid string! Using default: " << defaultvalue << std::endl; } }

rapidjson::Document&& ApplicationStatus::GetApplicationStatusJSON()
{

	rapidjson::Document applicationStatusJson;
	applicationStatusJson.SetObject();
	rapidjson::Document::AllocatorType& allocator = applicationStatusJson.GetAllocator();
	
	// locks to make sure that the entire json object is consistent
	std::lock_guard<std::mutex> guard(dataLock);
	{
		// camera settings
		applicationStatusJson.AddMember("cameraRunning", IsAppCapturing(), allocator);
		applicationStatusJson.AddMember("cameraName", rapidjson::Value().SetString(cameraName.c_str(), cameraName.length(), allocator), allocator);
		applicationStatusJson.AddMember("cameraSerial", rapidjson::Value().SetString(cameraSerial.c_str(), cameraSerial.length(), allocator), allocator);
		applicationStatusJson.AddMember("cameraDepth", isCameraDepthRunning, allocator);
		applicationStatusJson.AddMember("cameraColor", isCameraColorRunning, allocator);
		applicationStatusJson.AddMember("cameraDepthWidth", cameraDepthWidth, allocator);
		applicationStatusJson.AddMember("cameraDeptHeight", cameraDepthHeight, allocator);
		applicationStatusJson.AddMember("cameraColorWidth", cameraColorWidth, allocator);
		applicationStatusJson.AddMember("cameraColorHeight", cameraColorHeight, allocator);
		
		// streaming server
		applicationStatusJson.AddMember("streaming", IsAppStreaming(), allocator);	  // true if streaming either color, depth, or both
		applicationStatusJson.AddMember("streamingClients", streamingClients, allocator); // number of clients currently connected to the stream
		applicationStatusJson.AddMember("streamingMaxFPS", streamingMaxFPS, allocator);	  // FPS of the stream
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
