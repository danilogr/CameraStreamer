#include "TCPRelayCamera.h"



// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_TCPCLIENT_RELAY_CAMERA

#include <iostream>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/array.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>

#include <libyuv.h>

const char* TCPRelayCamera::TCPRelayCameraConstStr = "TCPRelayCam";

bool TCPRelayCamera::LoadConfigurationSettings()
{
	using namespace boost::asio;
	using namespace boost::asio::ip;

	// makes sure to invoke base class implementation of settings
	if (Camera::LoadConfigurationSettings())
	{

		// creates the asio endpoint based on configuration settings
		hostAddr = configuration->GetCameraCustomString("host", "localhost", true);
		hostPort = configuration->GetCameraCustomInt("port", 1234, true);
		hostEndpoint = tcp::endpoint(address::from_string(hostAddr), hostPort);

		// selects correct header parser
		// (does not need to do anything for now)
		// but in the future, it will be something like
		// protocol = "LWHYUV420"
		// instantiates a new protocol parser based on script

		// serial number? might be protocol dependent, but for now
		cameraSerialNumber = hostAddr + std::string(":") + configuration->GetCameraCustomString("port", "1234", false);

		return true;
	}
	return false;
}

// yuv protocol: future uses would rely on a different protocol
void YUVReadHeader(boost::asio::ip::tcp::socket &clientSocket, int& width, int& height)
{


}

// yuv protocol: future uses
std::shared_ptr<Frame> YUVReadFrame(boost::asio::ip::tcp::socket& clientSocket, int width, int height)
{

	using namespace libyuv;


	char* frameY;
	char* frameU;
	char* frameV;

	// creates BGRA frame
	std::shared_ptr<Frame> rgbFrame = Frame::Create(width, height, FrameType::Encoding::RGBA32);

	I422ToRGBA( (const uint8_t *) frameY, width,
			    (const uint8_t *) frameU, width / 2,
			    (const uint8_t *) frameV, width / 2,
		rgbFrame->getData(), width, width, height);

	return rgbFrame;


}

// Get Resolution
//byte[] resolution = new byte[sizeof(int) * 2];
//Buffer.BlockCopy(BitConverter.GetBytes((int)frame.Width), 0, resolution, 0, sizeof(int));
//Buffer.BlockCopy(BitConverter.GetBytes((int)frame.Height), 0, resolution, sizeof(int), sizeof(int));
// Add Resolution as header
//byte[] combined = new byte[frame.Buffer.Length + resolution.Length];
//Buffer.BlockCopy(resolution, 0, combined, 0, resolution.Length);
//Buffer.BlockCopy(frame.Buffer, 0, combined, resolution.Length, frame.Buffer.Length);
// Send
//CommOutput.Send(combined);


void TCPRelayCamera::CameraLoop()
{
	using namespace boost::asio;
	using namespace boost::asio::ip;

	// all asio methods rely on io_context
	boost::asio::io_context io_context;

	// we need a way to stop operations that take too long
	boost::asio::deadline_timer deadlineTimer(io_context);

	Logger::Log(TCPRelayCameraConstStr) << "Started TCP Relay Camera polling thread: " << std::this_thread::get_id << std::endl;

	// if the thread is stopped but we did execute the connected callback,
	// then we will execute the disconnected callback to maintain consistency
	bool didWeCallConnectedCallback = false;
	unsigned long long totalTries = 0;

	while (thread_running)
	{
		// start again ...
		didWeCallConnectedCallback = false;
		totalTries = 0;
		
		// creates client tcp socket
		tcp::socket client_socket(io_context);

		//
		// Step #1) OPEN CAMERA --> here it means that it connects to the socket
		//
		while (thread_running && !IsAnyCameraEnabled())
		{
			// load configuration to figure out how to read packets and handle their content
			while (!LoadConfigurationSettings() && thread_running)
			{
				Logger::Log(TCPRelayCameraConstStr) << "Trying again in 5 seconds..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}

			//  if we stop the application while waiting...
			if (!thread_running)
			{
				break;
			}

			// try to connect
						// tries to start the streaming pipeline
			while (!IsAnyCameraEnabled() && thread_running)
			{
				try
				{
					Logger::Log(TCPRelayCameraConstStr) << "Connecting to: " << hostAddr << ":" << hostPort << std::endl;
					//deadlineTimer.expires_from_now(boost::posix_time::seconds(1));
					client_socket.connect(hostEndpoint);

					// hardcoded for now - yuv case
					if (client_socket.is_open())
					{
						depthCameraEnabled = false;
						colorCameraEnabled = true;
					}

					// read camera settings from the network
					int colorCameraWidth = 0, colorCameraHeight = 0;
					YUVReadHeader(client_socket, colorCameraWidth, colorCameraHeight);
					if (colorCameraWidth > 0 && colorCameraHeight > 0)
					{
						colorCameraParameters.resolutionWidth  = colorCameraWidth;
						colorCameraParameters.resolutionHeight = colorCameraHeight;
					}

					// reads a frame to ignore things
					YUVReadFrame(client_socket, colorCameraWidth, colorCameraHeight);

				}
				catch (const std::exception& e)
				{
					// did something fail?
					Logger::Log(TCPRelayCameraConstStr) << "ERROR! Could not connect to camera: " << e.what() << std::endl;
					if (client_socket.is_open())
						client_socket.close();
					colorCameraEnabled = false;
					depthCameraEnabled = false;
					std::this_thread::sleep_for(std::chrono::seconds(1));
					break;
				}
			}

			// error openning all cameras?
			if (!IsAnyCameraEnabled())
			{
				// we have to close the device and try again
				colorCameraEnabled = false;
				depthCameraEnabled = false;

				// closes the socket if its open / connected
				if (client_socket.is_open())
					client_socket.close();

				Logger::Log(TCPRelayCameraConstStr) << "Trying again in 1 second..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			else {
				// reminder that we have to wrap things in the end, even if
				// no frames were captured
				
			}

			//Logger::Log(RealSenseConstStr) << "Opened RealSense device id: " << cameraSerialNumber << std::endl;




		}

		//
		// Step #2) START, LOOP FOR FRAMES, STOP
		//

		// start keeping track of incoming frames / failed frames
		statistics.StartCounting();

		// loop to capture frames
		if (thread_running && IsAnyCameraEnabled())
		{
			// time to start reading frames and streaming
			unsigned int triesBeforeRestart = 5;
			totalTries = 0;


			// updates app with capture and stream status
			appStatus->UpdateCaptureStatus(colorCameraEnabled, depthCameraEnabled, cameraSerialNumber,
				OpenCVCameraMatrix(colorCameraEnabled ? colorCameraParameters : depthCameraParameters),

				// color camera
				colorCameraEnabled ? colorCameraParameters.resolutionWidth : 0,
				colorCameraEnabled ? colorCameraParameters.resolutionHeight : 0,

				// depth camera
				depthCameraEnabled ? depthCameraParameters.resolutionWidth : 0,
				depthCameraEnabled ? depthCameraParameters.resolutionHeight : 0,

				// streaming (color resolution when  color is available, depth resolution otherwise)
				colorCameraEnabled ? colorCameraParameters.resolutionWidth : depthCameraParameters.resolutionWidth,
				colorCameraEnabled ? colorCameraParameters.resolutionHeight : depthCameraParameters.resolutionHeight);


		}

		// starts
		Logger::Log(TCPRelayCameraConstStr) << "Started capturing" << std::endl;

		// invokes camera connect callback
		if (thread_running && onCameraConnect)
		{
			didWeCallConnectedCallback = true; // we will need this later in case the thread is stopped
			onCameraConnect();
		}

		// capture loop
		
		try
		{
			while (thread_running)
			{

				int colorWidth, colorHeight;

				std::chrono::microseconds timestamp = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
				YUVReadHeader(client_socket, colorWidth, colorHeight);
				std::shared_ptr<Frame> frame = YUVReadFrame(client_socket, colorWidth, colorHeight);

				// invoke callback
				if (onFramesReady)
					onFramesReady(timestamp, frame, nullptr);

				// update info
				++statistics.framesCaptured;

			}
		}
		catch (const std::exception& e)
		{
			// did something fail?
			Logger::Log(TCPRelayCameraConstStr) << "ERROR! Could not read frame: " << e.what() << std::endl;

			// closes connection
			if (client_socket.is_open())
				client_socket.close();

			// closes cameras
			colorCameraEnabled = false;
			depthCameraEnabled = false;
			appStatus->UpdateCaptureStatus(false, false);
			statistics.StopCounting();

			// waits 1 second before trying again
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		//
		// Step #3) Shutdown
		//

		// stop statistics
		statistics.StopCounting();

		// let other threads know that we are not capturing anymore
		appStatus->UpdateCaptureStatus(false, false);

		// stop cameras that might be running
		if (IsAnyCameraEnabled())
		{
			// closes connection
			if (client_socket.is_open())
				client_socket.close();

			depthCameraEnabled = false;
			colorCameraEnabled = false;
		}

		// calls the camera disconnect callback if we called onCameraConnect() - consistency
		if (didWeCallConnectedCallback && onCameraDisconnect)
			onCameraDisconnect();

		// waits one second before restarting...
		if (thread_running)
		{
			Logger::Log(TCPRelayCameraConstStr) << "Restarting device..." << std::endl;
		}
		

		// try to connect (synchronously) - times out every second? then waits 5 seconds and tries again

		// still running?
		// - call camera connected event
		// - (marks true for boolean of camera connected)

		// reading loop
		//  - check if thread_running
		//  - --- starts timer
		//  - read_from_network_loop: header
		//  -   times out in 1 second, check if thread_running and try again (if 5 timeouts == reconnect (brutal!))
		//  -   receives zero bytes (disconnect), then wait five seconds to reconnect
		//  - read_from_network_loop: frames (repeat above procedure for frames)
		//  - received frames? update stats
		//  - convert them to the format expected above (or do a future pass-through for JPEG)
		//  - --- stops timer
		//  - -- were we faster than 33ms? wait until we have around 1 ms left
		//  - invoke frame received message
				
		// thread stopped runnig? quit
		// - disconnect
		// - (check boolean of camera connected before, true?) then call camera disconnected event
		// - exit
	}

}

#endif