#include "TCPRelayCamera.h"



// we have compilation flags that determine whether this feature
// is supported or not
#include "CompilerConfiguration.h"
#ifdef ENABLE_TCPCLIENT_RELAY_CAMERA

#include <iostream>

//#include "VectorNetworkBuffer.h"
#include "FrameNetworkBuffer.h"

// custom packet readers
#include "RAWYUVProtocolReader.h"

// int to string
#include <boost/lexical_cast.hpp>



const char* TCPRelayCamera::TCPRelayCameraConstStr = "TCPRelayCam";

bool TCPRelayCamera::LoadConfigurationSettings()
{

	// makes sure to invoke base class implementation of settings
	if (Camera::LoadConfigurationSettings())
	{

		// creates the asio endpoint based on configuration settings
		hostAddr = configuration->GetCameraCustomString("host", "localhost", true);
		hostPort = configuration->GetCameraCustomInt("port", 1234, true);

		// creates a raw yuv packet reader
		packetReader = RAWYUVProtocolReader::Create();

		// selects correct header parser
		// (does not need to do anything for now)
		// but in the future, it will be something like
		// protocol = "LWHYUV420"
		// instantiates a new drotocol parser based on script

		// serial number? might be protocol dependent, but for now
		cameraSerialNumber = packetReader->ProtocolName() + ":\\" + hostAddr + std::string(":") + configuration->GetCameraCustomString("port", "1234", false);

		return true;
	}
	return false;
}


void TCPRelayCamera::onSocketDisconnect(std::shared_ptr<comms::ReliableCommunicationClientX> oldConnection, const boost::system::error_code& e)
{

	using namespace std::placeholders; // for  _1, _2, ...

	if (oldConnection)
		Logger::Log(TCPRelayCameraConstStr) << "Disconnected from " << oldConnection->remoteAddress() << ':' << oldConnection->remotePort() << std::endl;
	else
		Logger::Log(TCPRelayCameraConstStr) << "Disconnected" << std::endl;

	// print network statistics

	if (tcpClient == oldConnection)
	{
		// stop statistics
		statistics.StopCounting();

		// let other threads know that we are not capturing anymore
		appStatus->UpdateCaptureStatus(false, false);

		// stop cameras that might be running
		depthCameraEnabled = false;
		colorCameraEnabled = false;

		// calls the camera disconnect callback if we called onCameraConnect() - consistency
		if (didWeCallConnectedCallback && onCameraDisconnect)
			onCameraDisconnect();

		didWeCallConnectedCallback = false;

		// if we are supposed to reconnect, an asynchronous request will
		// be pending and dealing with it!


		// try again after done disconnecting
		if (thread_running)
		{
			Logger::Log(TCPRelayCameraConstStr) << "Trying again in 1 second..." << std::endl;
			reconnectTimer.expires_from_now(boost::posix_time::seconds(1));
			reconnectTimer.async_wait(std::bind(&TCPRelayCamera::startAsyncConnection, this, tcpClient, _1));
		}

	}
	else {
		Logger::Log(TCPRelayCameraConstStr) << "Something is not right... " << std::endl;
	}

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
		tcpClient->connect(hostAddr, hostPort, std::bind(&TCPRelayCamera::onSocketConnect, this, _1, _2), std::chrono::milliseconds(3000));
	}

}


void TCPRelayCamera::onSocketReadHeader(std::shared_ptr<comms::ReliableCommunicationClientX> socket, const boost::system::error_code& e)
{

	using namespace std::placeholders; // for  _1, _2, ...
	// this shouldn't really happen
	if (socket != tcpClient) return;

	// got header and we can continue?
	if (!e && thread_running)
	{
		// parse the headers
		if (packetReader->ParseHeader(&(*headerBuffer)[0], headerBuffer->size()))
		{
			// is there anything to read?
			if (packetReader->getNetworkFrameSize() == 0)
			{
				// read a header again
				++statistics.framesCaptured;
				comms::NetworkBufferPtr buffer(headerBuffer);
				socket->read(buffer, packetReader->FixedHeaderSize(), std::bind(&TCPRelayCamera::onSocketReadHeader, this, _1, _2), getFrameTimeout);
				return;
			}

			// is this the first time we get header information?
			if (!IsAnyCameraEnabled())
			{
				// start keeping track of incoming frames / failed frames
				statistics.StartCounting();

				// updates what frames will be streamed
				colorCameraEnabled = packetReader->supportsColor();
				depthCameraEnabled = packetReader->supportsDepth();

				// fill up camera parameters (if known)
				colorCameraParameters.resolutionWidth = packetReader->getColorFrameWidth();
				colorCameraParameters.resolutionHeight = packetReader->getColorFrameHeight();

				depthCameraParameters.resolutionWidth = packetReader->getDepthFrameWidth();
				depthCameraParameters.resolutionHeight = packetReader->getDepthFrameHeight();

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

				// starts
				Logger::Log(TCPRelayCameraConstStr) << "Started capturing" << std::endl;

				// alocates memory for next reads (extra 1kb as a safety net)
				frameBuffer = std::make_shared<std::vector<unsigned char> >(packetReader->getNetworkFrameSize() + 1024);

				// invokes camera connect callback
				didWeCallConnectedCallback = true; // we will need this later in case the thread is stopped

				// tell others that the camera connected
				if (onCameraConnect)
					onCameraConnect();
			}

			// do we have enough memory for this next packet?
			if (packetReader->getNetworkFrameSize() > frameBuffer->size())
			{
				// alocates memory for next reads
				frameBuffer = std::make_shared<std::vector<unsigned char> >(packetReader->getNetworkFrameSize() + 1024);
			}

			// read the entire network packet (asynchronously)
			socket->read(frameBuffer, packetReader->getNetworkFrameSize(), std::bind(&TCPRelayCamera::onSocketRead, this, _1, _2), getFrameTimeout);
			return;

		}
		else {
			Logger::Log(TCPRelayCameraConstStr) << "Error parsing header..." << std::endl;
			++totalTries;
			++statistics.framesFailed;
			statistics.StopCounting();
			socket->close();
		}

	}
	else if (e) {
		Logger::Log(TCPRelayCameraConstStr) << "Error reading frame: " << e.message() << std::endl;
		++totalTries;
		++statistics.framesFailed;
		statistics.StopCounting();
	}
}

void TCPRelayCamera::onSocketRead(std::shared_ptr<comms::ReliableCommunicationClientX> socket, const boost::system::error_code& e)
{
	using namespace std::placeholders; // for  _1, _2, ...
	// this shouldn't really happen
	if (socket != tcpClient);

	// got a frame and we can continue?
	if (!e && thread_running)
	{
		// parse frame
		if (packetReader->ParseFrame(&(*frameBuffer)[0], packetReader->getNetworkFrameSize()))
		{
			++statistics.framesCaptured;
			 
			// invoke frame ready callback
			if (onFramesReady)
				onFramesReady(packetReader->getLastFrameTimestamp(), packetReader->getLastColorFrame(), packetReader->getLastDepthFrame());

			// read another frame
			socket->read(headerBuffer, packetReader->FixedHeaderSize(), std::bind(&TCPRelayCamera::onSocketReadHeader, this, _1, _2), getFrameTimeout);

			return;
		}
		else {
			Logger::Log(TCPRelayCameraConstStr) << "Error parsing frame..." << std::endl;
			++totalTries;
			++statistics.framesFailed;
			statistics.StopCounting();
			socket->close();
		}
	}
	else if (e) {
		Logger::Log(TCPRelayCameraConstStr) << "Error reading frame: " << e.message() << std::endl;
		++totalTries;
		++statistics.framesFailed;
		statistics.StopCounting();
	}

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
			return;
		}
		else {
			// we are finally good to start reading frames

			// we need to get camera info from the stream before we report that we are connected.
			// Thus, we will read a header, and then read the first frame

			// wraps shared_ptr in a generic shared_ptr wrapper used by ReliableCommunicationTCPClientX


			// async read
			socket->read(headerBuffer, headerBuffer->size(), std::bind(&TCPRelayCamera::onSocketReadHeader, this, _1, _2), getFrameTimeout);
			return;
		}
		

	} else if (e == comms::error::TimedOut)
	{
		Logger::Log(TCPRelayCameraConstStr) << "Timed out..." << std::endl;
		++totalTries;

	}
	else {
		Logger::Log(TCPRelayCameraConstStr) << "Error connecting to remote host: " << e.message() << std::endl;
		++totalTries;
	}

	// if it hasn't returned so far, it means that we are going to try again
	if (thread_running)
	{
		Logger::Log(TCPRelayCameraConstStr) << "Trying again in 1 second..." << std::endl;
		reconnectTimer.expires_from_now(boost::posix_time::seconds(1));
		reconnectTimer.async_wait(std::bind(&TCPRelayCamera::startAsyncConnection, this, tcpClient, _1));
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

		// read configuration (todo: make async)
		while (!LoadConfigurationSettings() && thread_running)
		{
			Logger::Log(TCPRelayCameraConstStr) << "Trying again in 5 seconds..." << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}

		//  if we stop the application while waiting...
		if (!thread_running) break;

		Logger::Log(TCPRelayCameraConstStr) << "Using protocol " << packetReader->ProtocolName() << std::endl;

		// async event loop: connect, read header, read frame, repeat read header, read frame until disconnected or stopped.
		try
		{
			if (packetReader->HasFixedHeaderSize())
			{
				// allocate memory needed to read the header
				headerBuffer = std::make_shared<std::vector<unsigned char> >(packetReader->FixedHeaderSize());

				// now all the operations migrate to an asynchronous model
				boost::asio::post(io_context, std::bind(&TCPRelayCamera::startAsyncConnection, this, nullptr, boost::system::error_code()));

				// runs the async event loop until an exception happens or until we are done
				io_context.run();
			}
			else {
				Logger::Log(TCPRelayCameraConstStr) << "Protocol " << packetReader->ProtocolName() << " does not support fixed header size! Use a different protocol!" << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
		catch (const std::exception& e)
		{
			Logger::Log(TCPRelayCameraConstStr) << "Unexpected error " << e.what() << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}

		//
		// got out of the read loop. everything should've been shut, but just in case
		// 

		// stop statistics
		statistics.StopCounting();

		// let other threads know that we are not capturing anymore
		appStatus->UpdateCaptureStatus(false, false);

		// calls the camera disconnect callback if we called onCameraConnect() - consistency
		if (didWeCallConnectedCallback && onCameraDisconnect)
			onCameraDisconnect();

		// waits one second before restarting...
		if (thread_running)
		{
			Logger::Log(TCPRelayCameraConstStr) << "Restarting device..." << std::endl;
		}
		
	}

}

#endif