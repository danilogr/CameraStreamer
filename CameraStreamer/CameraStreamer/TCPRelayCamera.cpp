#include "TCPRelayCamera.h"



// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_TCPCLIENT_RELAY_CAMERA


#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>


const char* TCPRelayCamera::TCPRelayCameraConstStr = "TCPRelayCam";

void TCPRelayCamera::CameraLoop()
{
	using namespace boost::asio;
	using namespace boost::asio::ip;

	// all asio methods rely on io_service
	boost::asio::io_service io_service;

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
		// Step #1) OPEN CAMERA
		//
		while (thread_running && !IsAnyCameraEnabled())
		{
			// load configuration to figure out how to read packets and handle their content


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