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

void TCPRelayCamera::CameraLoop()
{
	using namespace boost::asio;
	using namespace boost::asio::ip;

	// all asio methods rely on io_service
	boost::asio::io_service io_service;

	// we need a way to stop operations that take too long
	boost::asio::deadline_timer deadlineTimer(io_service);

	Logger::Log(TCPRelayCameraConstStr) << "Started TCP Relay Camera polling thread: " << std::this_thread::get_id << std::endl;

	bool didWeEverInitializeTheCamera = false;
	// if the thread is stopped but we did execute the connected callback,
	// then we will execute the disconnected callback to maintain consistency
	bool didWeCallConnectedCallback = false;
	unsigned long long totalTries = 0;

	while (thread_running)
	{
		// start again ...
		didWeEverInitializeTheCamera = false;
		didWeCallConnectedCallback = false;
		totalTries = 0;
		
		// creates client tcp socket
		tcp::socket client_socket(io_service);

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
					client_socket.connect()
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
				Logger::Log(TCPRelayCameraConstStr) << "Trying again in 1 second..." << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			else {
				// reminder that we have to wrap things in the end, even if
				// no frames were captured
				didWeEverInitializeTheCamera = true;
			}

			Logger::Log(RealSenseConstStr) << "Opened RealSense device id: " << cameraSerialNumber << std::endl;




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