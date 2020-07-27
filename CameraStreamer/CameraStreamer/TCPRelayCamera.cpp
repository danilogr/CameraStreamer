#include "TCPRelayCamera.h"



// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_TCPCLIENT_RELAY_CAMERA

#include <iostream>

#include "VectorNetworkBuffer.h"
#include "FrameNetworkBuffer.h"


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

		// selects correct header parser
		// (does not need to do anything for now)
		// but in the future, it will be something like
		// protocol = "LWHYUV420"
		// instantiates a new drotocol parser based on script

		// serial number? might be protocol dependent, but for now
		cameraSerialNumber = hostAddr + std::string(":") + configuration->GetCameraCustomString("port", "1234", false);

		return true;
	}
	return false;
}



void TCPRelayCamera::startAsyncConnection(std::shared_ptr<comms::ReliableCommunicationClientX> oldConnection, const boost::system::error_code& e)
{
	if (e == boost::asio::error::operation_aborted)
		return;

	if (thread_running)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		tcpClient = comms::ReliableCommunicationClientX::createClient(io_context);
		tcpClient->onDisconnected = std::bind(&TCPRelayCamera::onSocketDisconnect, this, _1, _2);
		tcpClient->setTag(totalTries);

		Logger::Log(TCPRelayCameraConstStr) << "Connecting to " << hostAddr << ':' << hostPort << std::endl;
		tcpClient->connect(hostAddr, hostPort, std::bind(&TCPRelayCamera::onSocketConnect, this, _1, _2), std::chrono::milliseconds(1000));
	}

}


void TCPRelayCamera::onSocketReadHeader(std::shared_ptr<comms::ReliableCommunicationClientX> socket, const boost::system::error_code& e)
{
	// this shouldn't really happen
	if (socket != tcpClient);

	// got header and we can continue?
	if (!e && thread_running)
	{

	}
}

void TCPRelayCamera::onSocketRead(std::shared_ptr<comms::ReliableCommunicationClientX> socket, const boost::system::error_code& e)
{

}

void TCPRelayCamera::onSocketConnect(std::shared_ptr<comms::ReliableCommunicationClientX> socket, const boost::system::error_code& e)
{

	using namespace std::placeholders; // for  _1, _2, ...

	// tcpClient is the current connection. Any prior events from older connections should be ignored
	if (!socket || socket != tcpClient) return;

	if (!e)
	{
		// great, we are connected!
		Logger::Log(TCPRelayCameraConstStr) << "Connected to " << socket->remoteAddress() << ':' << socket->remotePort() << std::endl;

		// if the thread was cancelled, we will disconnect and wait for the disconnected event
		if (!thread_running)
		{
			if (!e)
				socket->close();
		}
		else {
			// we are finally good to start reading frames

			// we need to get camera info from the stream before we report that we are connected.
			// Thus, we will read a header, and then read the first frame

			// wraps shared_ptr in a generic shared_ptr wrapper used by ReliableCommunicationTCPClientX
			comms::VectorNetworkBuffer<unsigned char> buffer(headerBuffer);

			// async read
			socket->read(buffer, headerBuffer->size(), std::bind(&TCPRelayCamera::onSocketReadHeader, this, _1, _2), getFrameTimeout);
		}
		

	} else if (e == comms::error::TimedOut)
	{
		Logger::Log(TCPRelayCameraConstStr) << "Timed out..." << std::endl;
		++totalTries;

	}
	else {
		Logger::Log(TCPRelayCameraConstStr) << "Error connecting to remote host:" << e << std::endl;
		++totalTries;
	}

	// if it hasn't returned so far, it means that we are going to try again
	if (thread_running)
	{
		Logger::Log(TCPRelayCameraConstStr) << "Trying again in 1 second..." << std::endl;
		reconnectTimer.expires_from_now(boost::posix_time::seconds(1));
		reconnectTimer.async_wait(std::bind(&TCPRelayCamera::startAsyncConnection, tcpClient, _1));
	}
}


void TCPRelayCamera::CameraLoop()
{
	using namespace std::placeholders; // for  _1, _2, ...
	
	Logger::Log(TCPRelayCameraConstStr) << "Started TCP Relay Camera thread: " << std::this_thread::get_id << std::endl;
	unsigned long long totalTries = 0;

	while (thread_running)
	{
		//  makes sure to execute a disconnect callback whenever a connected callback has been called
		didWeCallConnectedCallback = false;
		

		// object responsible for handlinng the incoming network packet and decoding it into something our tool can make sense
		std::shared_ptr<FutureYUVProtocolClass> depacketizer;


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

			depacketizer = std::make_shared<FutureYUVProtocolClass>();


			Logger::Log(TCPRelayCameraConstStr) << "Using protocol " << depacketizer->ProtocolName << std::endl;

			try
			{
				if (depacketizer->HasFixedHeaderSize())
				{

					// allocate memory needed to read the header
					headerBuffer = std::make_shared<std::vector<unsigned char> >(depacketizer->FixedHeaderSize());

					// now all the operations migrate to an asynchronous model
					boost::asio::post(io_context, std::bind(&TCPRelayCamera::startAsyncConnection, this, nullptr, boost::system::error_code()));

					// runs the async event loop until an exception happens or until we are done
					io_context.run();

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
						if (tcpClient && tcpClient->connected())
							tcpClient->close();

						depthCameraEnabled = false;
						colorCameraEnabled = false;
					}

					// calls the camera disconnect callback if we called onCameraConnect() - consistency
					if (didWeCallConnectedCallback && onCameraDisconnect)
						onCameraDisconnect();


				} else {
					Logger::Log(TCPRelayCameraConstStr) << "Protocol " << depacketizer->ProtocolName << " does not support fixed header size! Use a different protocol!" << std::endl;
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}
			}
			catch (const std::exception& e)
			{
				Logger::Log(TCPRelayCameraConstStr) << "Unexpected error " << e.what() << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(5));
			}

			// waits one second before restarting...
			if (thread_running)
			{
				Logger::Log(TCPRelayCameraConstStr) << "Restarting device..." << std::endl;
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
					//if (client_socket.is_open())
//						client_socket.close();
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