#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <rapidjson/document.h>

class Configuration
{

protected:
	// this is only used when a recording starts or stops as many fields are changed at once
	std::mutex dataLock;

	// streaming server port
	int streamerPort;

	// control server port
	int controlPort;

	// streamer: should we throtlle streaming FPS by default for all clients?
	bool streamingThrottleFPS;

	// streamer: if true, it streams only a JPG of the color stream (yeah, very specific case)
	bool streamingJpegLengthValueProtocol;

	// streamer: if steramingThrottleFPS is true, what is the max FPS that we accept / allow ?
	int streamingMaxFPS;

	// streamer: default streaming width and height
	int streamingWidth, streamingHeight;

	// streamer: default streaming width and height for the color camera
//	int streamingColorWidth, streamingColorHeight;

	// streamer: default steraming width and height for the depth camera
//	int streamingDepthWidth, streamingDepthHeight;

	// streamer: default streaming format for all clients (e.g., color: jpeg, depth: raw 16bit)
	std::string streamingColorFormat, streamingDepthFormat;

	// streamer: should we stream color by default?
	bool isStreamingColor;
	
	// streamer: should we stream depth by default?
	bool isStreamingDepth;

	// camera: what camera should we connect to?
	std::string cameraName;
	
	// camera: should we connect to a specific camera (given its serial number)
	std::string cameraSerial;

	// camera: if no serial number is available in the configuration file, then
	// this variable is true, and we should connect to any camera
	bool requestFirstCameraAvailable;

	// camera: which cameras should we request?
	bool requestDepthCamera, requestColorCamera;// , requestInfraredCamera;

	// camera: depth camera resolution
	int cameraDepthWidth, cameraDepthHeight;
	 
	// camera: color camera resolution
	int cameraColorWidth, cameraColorHeight;
	


private:
	// camera specific configuration (stored as a json document)
	// stores configuration specific information in memory
	rapidjson::Document parsedConfigurationFile;
	
	// cached version of the configuration file
	std::vector<char> configurationFileString;
	
	// reads the contents of parsedConfigurationFile into class elements
	void ParseConfiguration(bool warn=true);
	
	// saves the contents of class elements into the parsedConfigurationFile
	void SerializeConfiguration();

public:

	Configuration() : streamerPort(0), controlPort(0),
	streamingThrottleFPS(false), streamingJpegLengthValueProtocol(false), streamingMaxFPS(60),
	streamingWidth(0), streamingHeight(0),
	//streamingColorWidth(0), streamingColorHeight(0),
	//streamingDepthWidth(0), streamingDepthHeight(0),
	isStreamingColor(false), isStreamingDepth(false),
	requestDepthCamera(true), requestColorCamera(true),
	cameraDepthWidth(0), cameraDepthHeight(0),
	cameraColorWidth(0), cameraColorHeight(0), requestFirstCameraAvailable(true) {};

	//
	// streaming ports
	//

	int GetStreamerPort() const { return streamerPort; }
	int GetControlPort() const { return controlPort; }
	void SetStreamerPort(int port) { streamerPort = port; }
	void SetControlPort(int port) { controlPort = port; }

	//
	// camera configuration	
	//

	bool IsDepthCameraEnabled() const { return requestDepthCamera; }
	bool IsColorCameraEnabled() const { return requestColorCamera; }
	//bool IsInfraredCameraEnabled() const { return requestInfraredCamera; }
	const std::string& GetCameraName() const { return cameraName; }
	const std::string& GetCameraSN() const { return cameraSerial; }
	bool UseFirstCameraAvailable() const { return requestFirstCameraAvailable; }
	bool SetUseFirstCameraAvailable(bool value) { requestFirstCameraAvailable = value; }
	int GetCameraColorHeight() const { return cameraColorHeight; }
	int GetCameraColorWidth() const { return cameraColorWidth; }
	int GetCameraDepthHeight() const { return cameraDepthHeight; }
	int GetCameraDepthWidth() const { return cameraDepthWidth; }
	void SetCameraColorHeight(int value) {  cameraColorHeight = value; }
	void SetCameraColorWidth(int value) {  cameraColorWidth = value; }
	void SetCameraDepthHeight(int value) { cameraDepthHeight = value; }
	void SetCameraDepthWidth(int value) { cameraDepthWidth = value; }


	//
	// streamer configuration
	//

	void SetStreamingColorEnabled(bool value) { isStreamingColor = value; }
	bool GetStreamingColorEnabled() const { return isStreamingColor; }
	void SetStreamingDepthEnabled(bool value) { isStreamingDepth = value; }
	bool GetStreamingDepthEnabled() const { return isStreamingDepth; }
	
	int GetStreamingHeight() const { return streamingHeight; }
	int GetStreamingWidth() const { return streamingWidth; }

	const std::string& GetStreamingColorFormat() const { return streamingColorFormat; }
	const std::string& GetStreamingDepthFormat() const { return streamingDepthFormat; }

	//int GetStreamingColorHeight() const { return streamingColorHeight; }
	//int GetStreamingColorWidth() const { return streamingColorWidth; }
	//int GetStreamingDepthHeight() const { return streamingDepthHeight; }
	//int GetStreamingDepthWidth() const { return streamingDepthWidth; }
	
	void SetStreamingMaxFPS(int value) { streamingMaxFPS = value;  }
	int GetStreamingMaxFPS() const { return streamingMaxFPS; }

	void SetStreamingThrottleMaxFPS(bool value) { streamingThrottleFPS = value; }
	bool IsStreamingThrottleMaxFPS() const { return streamingThrottleFPS; }

	void SetStreamingTLVJPGProtocol(bool value) { streamingJpegLengthValueProtocol = value; }
	bool IsStreamingTLVJPGProtocol() const { return streamingJpegLengthValueProtocol;  }



	//
	// Saving and laoding
	//
	

	// Load configuration from a json file
	bool LoadConfiguration(const std::string& filepath);

	// Saves configuration to a json file
	bool SaveConfiguration(const std::string& filepath);

};
