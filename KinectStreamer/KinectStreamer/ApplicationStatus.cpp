#include "ApplicationStatus.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/reader.h>
#include <rapidjson/error/error.h>

#include <fstream>
#include "Logger.h"

#define ReadJSONDefaultInt(d,name,destination,defaultvalue) if (d.HasMember(name)) { if (d[name].IsNumber()) {destination = d[name].GetInt();} else { destination = defaultvalue; Logger::Log("Config") << "Error! Element \""<< name << "\" should have a valid integer! (Using default value instead: " << defaultvalue <<')' << std::endl; } }
#define ReadJSONDefaultFloat(d,name,destination,defaultvalue) if (d.HasMember(name)) { if (d[name].IsNumber()) {destination = d[name].GetFloat();} else { destination = defaultvalue; Logger::Log("Config") << "Error! Element \""<< name << "\" should have a valid float! (Using default value instead: " << defaultvalue <<')' << std::endl; } }
#define ReadJSONDefaultBool(d,name,destination,defaultvalue) if (d.HasMember(name)) { if (d[name].IsBool()) {destination = d[name].GetBool();} else { destination = defaultvalue; Logger::Log("Config") << "Error! Element \""<< name << "\" should have a valid boolean! (Using default value instead: " << defaultvalue <<')' << std::endl; } }
#define ReadJSONDefaultString(d,name,destination,defaultvalue) if (d.HasMember(name)) { destination = d[name].GetString(); } else { destination = defaultvalue;}





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

bool  ApplicationStatus::LoadConfiguration(const std::string& filepath)
{
	// blocks anyone from reading from ApplicationStatus while we are reading from file
	std::lock_guard<std::mutex> guard(statusChangeLock);
	{
		// let's read the entire configuration file to memory first
		std::ifstream file(filepath, std::ios::binary | std::ios::ate);

		// were we able to open it?
		if (!file.is_open())
		{
			Logger::Log("Config") << "Could not open configuration file: " << filepath << std::endl;
		}

		// gets file size ( 100 mb should be enough; this is a basic safety check)
		std::streamsize fsize = file.tellg();

		if (fsize > 100 * 1024 * 1024)
		{
			Logger::Log("Config") << "Configuration file is too big (> 100mb)! (" << filepath << ")" << std::endl;
		}

		// parses file to a buffer
		file.seekg(0, std::ios::beg);
		std::vector<char> configurationFileContent(fsize + 32, 0); // pads string with zeros
		file.read(&configurationFileContent[0], fsize);
		file.close();

		// parses json
		rapidjson::StringStream filestream(&configurationFileContent[0]);

		try
		{
			rapidjson::ParseResult ok = parsedConfigurationFile.Parse((const char*)& configurationFileContent[0], fsize);

			// I am really trying to understand what this API does
			if (ok.IsError())
			{
				Logger::Log("Config") << "Error parsing configuration file: " << filepath << "! " << ok.Code() << " at location " << ok.Offset() << std::endl << std::endl;
				return false;
			}
		}
		catch (const std::exception & e)
		{
			Logger::Log("Config") << "Error parsing configuration file: " << filepath << "! " << e.what() << std::endl << std::endl;
			return false;
		}

		// now that json parsing is done, time to read individual components
		ParseConfiguration();

		Logger::Log("Config") << "Loaded configuration file: " << filepath << std::endl;
	}
}



void  ApplicationStatus::ParseConfiguration()
{
	// trick to always use default elements in case parts of the document are missing
	rapidjson::Value emptyDoc;
	rapidjson::Value currentDoc;

	// ports
	ReadJSONDefaultInt(parsedConfigurationFile, "streamerPort", streamerPort, 3614);
	ReadJSONDefaultInt(parsedConfigurationFile, "controlPort", streamerPort, 6606);

	// =======================================================================================
	// camera
	if (parsedConfigurationFile.HasMember("camera") && parsedConfigurationFile["camera"].IsObject())
	{
		currentDoc = parsedConfigurationFile["camera"].GetObject();
	}
	else {
		currentDoc = emptyDoc;
	}

	ReadJSONDefaultString(currentDoc, "type", cameraName, "k4a");
	ReadJSONDefaultBool(currentDoc, "requestColor", requestColorCamera, true);
	ReadJSONDefaultInt(currentDoc, "colorWidth", cameraRequestedColorWidth, 1280);
	ReadJSONDefaultInt(currentDoc, "colorHeight", cameraRequestedColorHeight, 720);
	ReadJSONDefaultBool(currentDoc, "requestDepth", requestDepthCamera, true);
	ReadJSONDefaultBool(currentDoc, "requestInfrared", requestInfraredCamera, true);
	ReadJSONDefaultInt(currentDoc, "depthWidth", cameraRequestedDepthWidth, 1280);
	ReadJSONDefaultInt(currentDoc, "depthHeight", cameraRequestedDepthHeight, 720);

	// color2
	ReadJSONDefaultBool(currentDoc, "transformation", requestInfraredCamera, true);

	// should we force a specific camera serial number?
	ReadJSONDefaultString(currentDoc, "serialNumber", cameraSerial, std::string());
	if (!cameraSerial.empty())
	{
		useFirstCameraAvailable = false; // depending on camera implementation, this will force the application to use a specific camera
	}

}



void  ApplicationStatus::SerializeConfiguration()
{

}