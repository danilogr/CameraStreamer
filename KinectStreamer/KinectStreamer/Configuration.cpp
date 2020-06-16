#include "Configuration.h"
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


/*rapidjson::Document&& Configuration::GetApplicationStatusJSON()
{

	rapidjson::Document applicationStatusJson;
	applicationStatusJson.SetObject();
	rapidjson::Document::AllocatorType& allocator = applicationStatusJson.GetAllocator();
	
	// locks to make sure that the entire json object is consistent
	std::lock_guard<std::mutex> guard(configurationChangeLock);
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

		// application ports
		applicationStatusJson.AddMember("port", streamerPort, allocator);
		applicationStatusJson.AddMember("controlPort", controlPort, allocator);
	}

	return std::move(applicationStatusJson);
}*/

bool  Configuration::LoadConfiguration(const std::string& filepath)
{
	bool successReading = true;
	// blocks anyone from reading from ApplicationStatus while we are reading from file
	std::lock_guard<std::mutex> guard(dataLock);
	{
		// let's read the entire configuration file to memory first
		std::ifstream file(filepath, std::ios::binary | std::ios::ate);

		// were we able to open it?
		if (!file.is_open())
		{
			Logger::Log("Config") << "Could not open configuration file: " << filepath << std::endl;
			successReading = false;
			parsedConfigurationFile = rapidjson::Document();
			parsedConfigurationFile.SetObject();
		}
		else // reads a file if one is available
		{

			// gets file size ( 100 mb should be enough; this is a basic safety check)
			std::streamsize fsize = file.tellg();

			// check file size
			if (fsize > 100 * 1024 * 1024)
			{
				Logger::Log("Config") << "Configuration file is too big (> 100mb)! (" << filepath << ")" << std::endl;
				successReading = false;
				file.close();
			} else {
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
						successReading = false;
					}
				}
				catch (const std::exception & e)
				{
					Logger::Log("Config") << "Error parsing configuration file: " << filepath << "! " << e.what() << std::endl << std::endl;
					successReading = false;
				}

			}

		}

		// now that json parsing is done, time to read individual components
		ParseConfiguration(successReading);

		if (successReading)
			Logger::Log("Config") << "Loaded configuration file: " << filepath << std::endl;
	}

	return successReading;
}



void  Configuration::ParseConfiguration(bool warn)
{
	// trick to always use default elements in case parts of the document are missing
	rapidjson::Value emptyDoc;
	emptyDoc.SetObject();
	rapidjson::Value currentDoc;
	currentDoc.SetObject();

	// ports
	ReadJSONDefaultInt(parsedConfigurationFile, "streamerPort", streamerPort, 3614, true);
	ReadJSONDefaultInt(parsedConfigurationFile, "controlPort", controlPort, 6606, true);

	// =======================================================================================
	// camera
	if (parsedConfigurationFile.HasMember("camera") && parsedConfigurationFile["camera"].IsObject())
	{
		currentDoc = parsedConfigurationFile["camera"].GetObject();
	}
	else {
		currentDoc = emptyDoc;
	}

	ReadJSONDefaultString(currentDoc, "type", cameraName, "k4a", true);
	ReadJSONDefaultBool(currentDoc, "requestColor", requestColorCamera, true, warn);
	ReadJSONDefaultInt(currentDoc, "colorWidth", cameraColorWidth, 1280, warn);
	ReadJSONDefaultInt(currentDoc, "colorHeight", cameraColorHeight, 720, warn);
	ReadJSONDefaultBool(currentDoc, "requestDepth", requestDepthCamera, true, warn);
	ReadJSONDefaultInt(currentDoc, "depthWidth", cameraDepthWidth, 640, warn);
	ReadJSONDefaultInt(currentDoc, "depthHeight", cameraDepthHeight, 576, warn);

	// should we force a specific camera serial number?
	ReadJSONDefaultString(currentDoc, "serialNumber", cameraSerial, std::string(), false);
	if (!cameraSerial.empty())
	{
		requestFirstCameraAvailable = false; // depending on camera implementation, this will force the application to use a specific camera
	}


	// =======================================================================================
	// streaming
	if (parsedConfigurationFile.HasMember("streaming") && parsedConfigurationFile["streaming"].IsObject())
	{
		currentDoc = parsedConfigurationFile["streaming"].GetObject();
	}
	else {
		currentDoc = emptyDoc;
	}

	ReadJSONDefaultBool(currentDoc, "streamColor", isStreamingColor, true, warn);
	//ReadJSONDefaultInt(currentDoc, "colorWidth", cameraColorWidth, 1280, warn);
	//ReadJSONDefaultInt(currentDoc, "colorHeight", cameraColorHeight, 720, warn);
	ReadJSONDefaultBool(currentDoc, "streamDepth", isStreamingDepth, true, warn);
	//ReadJSONDefaultInt(currentDoc, "depthWidth", cameraDepthWidth, 640, warn);
	//ReadJSONDefaultInt(currentDoc, "depthHeight", cameraDepthHeight, 576, warn);

	// validate streaming entries
	if (isStreamingColor && !requestColorCamera)
	{
		isStreamingColor = false;
		Logger::Log("Config") << "Disabling \"streamColor\" in \"streaming\" because \"requestColor\" is false in \"camera\"" << std::endl;
	}

	if (isStreamingDepth && !requestDepthCamera)
	{
		isStreamingDepth = false;
		Logger::Log("Config") << "Disabling \"streamDepth\" in \"streaming\" because \"requestDepth\" is false in \"camera\"" << std::endl;
	}


}



bool Configuration::SaveConfiguration(const std::string& filepath)
{
	return false;
}