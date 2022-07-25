#pragma once

#include "Frame.h"

#include <iostream>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <map>
#include <queue>
#include <thread>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <opencv2/opencv.hpp>

#include "Logger.h"
#include "NetworkStatistics.h"
#include "Configuration.h"
#include "ApplicationStatus.h"

using boost::asio::ip::tcp;

/*
  The TCPStreamingServer class sends camera color and depth 
  frames (whichever is available) to all tcp clients connected to it.

  TCPStreamingServer runs on a separate thread and queues up packages
  if encoding rate is not as fast as the rate in which a camera 
  capture frames.
*/
class TCPStreamingServer
{

	std::shared_ptr<ApplicationStatus> appStatus;
	std::shared_ptr<Configuration> configuration;

	bool streamingColor, streamingDepth, streamingJPEGLengthValue;

public:
	TCPStreamingServer(std::shared_ptr<ApplicationStatus> appStatus, std::shared_ptr<Configuration> configuration) : appStatus(appStatus),
		configuration(configuration), streamingColor(false), streamingDepth(false), streamingJPEGLengthValue(false),
		acceptor(io_context, tcp::endpoint(tcp::v4(), configuration->GetStreamerPort()))
	{
		Logger::Log("Streamer") << "Listening on " << configuration->GetStreamerPort() << std::endl;
	}

	~TCPStreamingServer()
	{
		Stop();
	}

	bool IsThreadRunning()
	{
		return (sThread && sThread->joinable());
	}

	void Run()
	{
		sThread.reset(new std::thread(std::bind(&TCPStreamingServer::thread_main, this)));
	}

	void Stop()
	{

		if (IsThreadRunning())
		{
			// stops io service
			io_context.stop();

			// is it running ? wait for it to finish
			if (sThread && sThread->joinable())
				sThread->join();

			// next line is technically not necessary, but
			// we are doing it for book keeping
			appStatus->SetStreamingDisabled();

			// gets done with thread
			sThread = nullptr;
		}

		// any clients connected?
		for (std::shared_ptr<tcp::socket> client : clients)
		{
			try
			{
				// thread is not running, so we need to account for the packets we were about to send, but didn't send in time
				clientsStatistics[client].messagesDropped += clientsQs[client].size();
				clientsStatistics[client].disconnected();
				client->close();
			}
			catch (std::exception e)
			{
				Logger::Log("Streamer") << "Error closing connection w/ Client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << std::endl;
			}

			Logger::Log("Streamer") << "Client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << " disconnected" << std::endl;
			Logger::Log("Streamer") << "[Stats] Sent client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << ":"
				<< clientsStatistics[client].bytesSent << " bytes (" << clientsStatistics[client].messagesSent << "packets sent; " << clientsStatistics[client].messagesDropped << " dropped) -"
				<< " Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - clientsStatistics[client].connectedTime).count() / 1000.0f << " sec" << std::endl;
		}

		// erase list of clients
		clients.clear();

	}


	// sends a color and depth frame to all clients connected
	void ForwardToAll(std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
	{
		// if not running
		if (!sThread) return;

		// first, let's make sure that this method gets called from the same thread that
		// is running the server
		if (std::this_thread::get_id() != sThread->get_id())
		{
			boost::asio::post(io_context, std::bind(&TCPStreamingServer::ForwardToAll, this, color, depth));
			return;
		}

		// which streams are enabled?
		size_t imgWidth = 0, imgHeight = 0, depthImgSize = 0;

		// color vector
		std::vector<uchar> encodedColorImage;
		
		// converts color to jpeg
		if (streamingColor)
		{
			imgWidth = color->getWidth();
			imgHeight = color->getHeight();

			if (color->getEncoding() == FrameType::Encoding::Custom) {
				// enlarge the vector to the size of the image
				encodedColorImage.resize(color->size());
				// copy the data
				memcpy(encodedColorImage.data(), color->getData(), color->size());
			}
			else {

				// todo: better, flexible fix in the future?
				if (color->getPixelLen() == 3)
				{
					cv::Mat colorImage(imgHeight, imgWidth, CV_8UC3, color->getData());
					cv::imencode(".jpg", colorImage, encodedColorImage); // todo: use jpegturbo or mozjpeg instead of OpenCV
				}
				else {
					cv::Mat colorImage(imgHeight, imgWidth, CV_8UC4, color->getData());
					cv::imencode(".jpg", colorImage, encodedColorImage); // todo: use jpegturbo or mozjpeg instead of OpenCV
				}
			}
		}

		if (streamingDepth)
		{
			imgWidth = depth->getWidth();
			imgHeight = depth->getHeight();
			depthImgSize = depth->size();
		}

		// prepares the message
		std::shared_ptr<std::vector<uchar> > message;
		
		if (streamingJPEGLengthValue)
		{
			message = std::make_shared<std::vector<uchar> >(1 * sizeof(uint32_t) + encodedColorImage.size());

			// header [color jpeg size] - tells clients how many bytes they should read
			*((uint32_t*)&(*message)[0]) = encodedColorImage.size();

			// copies color
			memcpy((unsigned char*)&(*message)[4], &encodedColorImage[0], encodedColorImage.size());

		}
		else {

			message = std::make_shared<std::vector<uchar> >(5 * sizeof(uint32_t) + encodedColorImage.size() + depthImgSize);

			// header prefix [package length]  - tells clients how many bytes they should read
			*((uint32_t*)&(*message)[0]) = message->size() - sizeof(uint32_t); // header size doesn't include itself

			// header [width][height][rgb length][depth length]
			*((uint32_t*)&(*message)[4]) = imgWidth;
			*((uint32_t*)&(*message)[8]) = imgHeight;
			*((uint32_t*)&(*message)[12]) = encodedColorImage.size();
			*((uint32_t*)&(*message)[16]) = depthImgSize;

			// write color frame
			if (streamingColor)
			{
				memcpy((unsigned char*)&(*message)[20], &encodedColorImage[0], encodedColorImage.size());
			}

			// write depth frame
			if (streamingDepth)
			{
				memcpy((unsigned char*)&(*message)[20 + encodedColorImage.size()], (const char*)depth->getData(), depth->size());
			}
		}



		// sends to all clients
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);

			for (std::shared_ptr< tcp::socket> client : clients)
			{
				// pops any message pending 
				while (clientsQs[client].size() > 1)
				{
					clientsQs[client].pop();
					clientsStatistics[client].messagesDropped++;
				}

				// adds message to client Q
				clientsQs[client].push(message);
				
				if (clientsQs[client].size() == 1)
				{
					// only message? let's send it
					write_to_client_async(client);
				}
			}
		}
	}

private:
	// event queue
	boost::asio::io_context io_context;

	// tcp server that listens and waits for clients
	tcp::acceptor acceptor;

	// pointer to the thread that will be managing client connections
	std::shared_ptr<std::thread> sThread;

	// TODO: create a class for clients instead of keeping everything here
	// set with all clients currently connected to the server
	std::set<std::shared_ptr< tcp::socket> > clients;
	std::map < std::shared_ptr< tcp::socket>, std::queue < std::shared_ptr < std::vector<uchar> > > > clientsQs;
	std::map < std::shared_ptr< tcp::socket>, NetworkStatistics > clientsStatistics;
	std::mutex clientSetMutex;

	// this method implements the main thread for TCPStreamingServer
	void thread_main()
	{
		Logger::Log("Streamer") << "Waiting for connections on port " << appStatus->GetStreamerPort() << std::endl;
	
		// update application to tell wich streams are being enabled
		streamingJPEGLengthValue = configuration->IsStreamingTLVJPGProtocol(); // this has precedence over the 

		streamingColor = configuration->GetStreamingColorEnabled();
		streamingDepth = configuration->GetStreamingDepthEnabled();
		appStatus->SetStreamingColorEnabled(streamingColor);
		appStatus->SetStreamingDepthEnabled(streamingDepth);

		Logger::Log("Streamer") << "Streaming " <<
		(streamingColor && streamingDepth ? "color and depth" : 
		(streamingColor ? "color" : "depth")) <<
		" at a resolution of " <<
		configuration->GetStreamingWidth() << 'x' << configuration->GetStreamingHeight() << std::endl;

		// let users know that we are using a comms protocol
		if (streamingJPEGLengthValue)
		{
			Logger::Log("Streamer") << "Streaming using JPEG Length Value Protocol " << std::endl;
		}

		aync_accept_connection(); // adds some work to the io_context, otherwise it exits
		io_context.run();	      // starts listening for connections
		
		// make sure others knows that the thread is not running
		streamingColor = false;
		streamingDepth = false;
		appStatus->SetStreamingDisabled();

		Logger::Log("Streamer") << "Thread exited successfully" << std::endl;
	}

	// waits for connections
	void aync_accept_connection()
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// creates a new socket to received the connection
		std::shared_ptr<tcp::socket> newClient = std::make_shared<tcp::socket>(io_context);

		// waits for a new connection
		acceptor.async_accept(*newClient, std::bind(&TCPStreamingServer::async_handle_accept, this, newClient, _1));
	}

	// as soon as a new client connects, adds client to the list and waits for a new connection
	void async_handle_accept(std::shared_ptr<tcp::socket> newClient, const boost::system::error_code& error)
	{
		// adds a new client to the list
		if (!error)
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);
			clients.insert(newClient);
			clientsQs[newClient] = std::queue<std::shared_ptr<std::vector<uchar> > >(); // creates a new Q for this client
			clientsStatistics[newClient] = NetworkStatistics(true);						// starts trackings stats for this client
			clientsStatistics[newClient].remoteAddress = newClient->remote_endpoint().address().to_string();
			clientsStatistics[newClient].remotePort = newClient->remote_endpoint().port();


			Logger::Log("Streamer") << "New client connected: " << clientsStatistics[newClient].remoteAddress << ':' << clientsStatistics[newClient].remotePort << std::endl;
		}

		// accepts a new connection
		aync_accept_connection();
	}

	// called when done writing to cleint
	void write_done(std::shared_ptr<tcp::socket> client, std::shared_ptr < std::vector<uchar> > buffer,
		            const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// there's nothing much we can do here besides remove the client if we get an error sending to it
		if (error)
		{
			{
				const std::lock_guard<std::mutex> lock(clientSetMutex);
				if (clients.find(client) != clients.end())
				{
					clientsStatistics[client].disconnected();
					clientsStatistics[client].messagesDropped++;

					Logger::Log("Streamer") << "Client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << " disconnected" << std::endl;
					Logger::Log("Streamer") << "[Stats] Sent client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << " --> "
						<< clientsStatistics[client].bytesSent << " bytes (" << clientsStatistics[client].messagesSent << " packets sent and " << clientsStatistics[client].messagesDropped << " dropped) -"
						<< " Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - clientsStatistics[client].connectedTime).count() / 1000.0f << " sec" << std::endl;
					
					clientsQs.erase(client);
					clients.erase(client);
					clientsStatistics.erase(client);
				}
			}
			return;
		}

		// pops the last read
		clientsQs[client].pop();
		clientsStatistics[client].messagesSent++;
		clientsStatistics[client].bytesSent += bytes_transferred;

		// moves on
		write_to_client_async(client);
	}

	void write_to_client_async(std::shared_ptr<tcp::socket> client)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// nothing to write -> done with asynchronous writings
		if (!clientsQs[client].size())
			return;

		// something to write? let's pop it!
		std::shared_ptr<std::vector<uchar > > message = clientsQs[client].back();

		// starts writing for this client
		boost::asio::async_write(*client, boost::asio::buffer(*message, message->size()), std::bind(&TCPStreamingServer::write_done, this, client, message, _1, _2));
	}

};

