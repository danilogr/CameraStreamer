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
#include "Statistics.h"
#include "ApplicationStatus.h"

using boost::asio::ip::tcp;

/*
  The TCPStreamingServer class sends kinect color and depth
  frames to all members connected.

  TCPStreamingServer runs on a separate thread
*/
class TCPStreamingServer
{

	std::shared_ptr<ApplicationStatus> appStatus;

public:
	TCPStreamingServer(std::shared_ptr<ApplicationStatus> appStatus) : appStatus(appStatus), acceptor(io_service, tcp::endpoint(tcp::v4(), appStatus->GetStreamerPort())) {
		Logger::Log("Streamer") << "Listening on " << appStatus->GetStreamerPort() << std::endl;
	};

	~TCPStreamingServer()
	{
		Stop();
	}

	bool isRunning()
	{
		return (sThread && sThread->joinable());
	}

	void Run()
	{
		sThread.reset(new std::thread(std::bind(&TCPStreamingServer::thread_main, this)));
	}

	void Stop()
	{

		if (isRunning())
		{
			// stops io service
			io_service.stop();

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
				clientsStatistics[client].packetsDropped += clientsQs[client].size();
				client->close();
			}
			catch (std::exception e)
			{
				Logger::Log("Streamer") << "Error closing connection w/ Client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << std::endl;
			}

			Logger::Log("Streamer") << "Client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << " disconnected" << std::endl;
			Logger::Log("Streamer") << "[Stats] Sent client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << ":"
				<< clientsStatistics[client].bytesSent << " bytes (" << clientsStatistics[client].packetsSent << "packets sent; " << clientsStatistics[client].packetsDropped << " dropped) -"
				<< " Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - clientsStatistics[client].connected).count() / 1000.0f << " sec" << std::endl;
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
			this->io_service.post(std::bind(&TCPStreamingServer::ForwardToAll, this, color, depth));
			return;
		}

		// converts color to jpeg
		cv::Mat colorImage(color->getHeight(), color->getWidth(), CV_8UC4, color->getData());
		std::vector<uchar> encodedColorImage;
		cv::imencode(".jpg", colorImage, encodedColorImage);

		// prepares the message
		std::shared_ptr<std::vector<uchar> > message = std::make_shared<std::vector<uchar> >(4*sizeof(uint32_t) + encodedColorImage.size() + depth->size());

		// header [width][height][rgb length][depth length]
		*((uint32_t*) & (*message)[0])  = color->getWidth();
		*((uint32_t*) & (*message)[4])  = color->getHeight();
		*((uint32_t*) & (*message)[8])  = encodedColorImage.size();
		*((uint32_t*) & (*message)[12]) =  depth->size();

		// write color frame
		memcpy((unsigned char *) &(*message)[16], & encodedColorImage[0], encodedColorImage.size());

		// write depth frame
		memcpy((unsigned char *) &(*message)[16 + encodedColorImage.size()], (const char*)depth->getData(), depth->size());

		// sends to all clients
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);

			for (std::shared_ptr< tcp::socket> client : clients)
			{
				// pops any message pending 
				while (clientsQs[client].size() > 1)
				{
					clientsQs[client].pop();
					clientsStatistics[client].packetsDropped++;
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
	boost::asio::io_service io_service;

	// tcp server that listens and waits for clients
	tcp::acceptor acceptor;

	// pointer to the thread that will be managing client connections
	std::shared_ptr<std::thread> sThread;

	// TODO: create a class for clients instead of keeping everything here
	// set with all clients currently connected to the server
	std::set<std::shared_ptr< tcp::socket> > clients;
	std::map < std::shared_ptr< tcp::socket>, std::queue < std::shared_ptr < std::vector<uchar> > > > clientsQs;
	std::map < std::shared_ptr< tcp::socket>, Statistics > clientsStatistics;
	std::mutex clientSetMutex;

	// this method implements the main thread for TCPStreamingServer
	void thread_main()
	{
		Logger::Log("Streamer") << "Waiting for connections on port " << appStatus->GetStreamerPort() << std::endl;
		
		// update application to tell wich streams are being enabled
		appStatus->SetStreamingColorEnabled(true);
		appStatus->SetStreamingDepthEnabled(true);

		aync_accept_connection(); // adds some work to the io_service, otherwise it exits
		io_service.run();	      // starts listening for connections
		
		// make sure others knows that the thread is not running
		appStatus->SetStreamingDisabled();

		Logger::Log("Streamer") << "Thread exited successfully" << std::endl;
	}

	// waits for connections
	void aync_accept_connection()
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// creates a new socket to received the connection
		std::shared_ptr<tcp::socket> newClient = std::make_shared<tcp::socket>(io_service);

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
			clientsStatistics[newClient] = Statistics();								// starts trackings stats for this client
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
					clientsStatistics[client].packetsDropped++;

					Logger::Log("Streamer") << "Client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << " disconnected" << std::endl;
					Logger::Log("Streamer") << "[Stats] Sent client " << clientsStatistics[client].remoteAddress << ':' << clientsStatistics[client].remotePort << " --> "
						<< clientsStatistics[client].bytesSent << " bytes (" << clientsStatistics[client].packetsSent << " packets sent and " << clientsStatistics[client].packetsDropped << " dropped) -"
						<< " Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - clientsStatistics[client].connected).count() / 1000.0f << " sec" << std::endl;
					
					clientsQs.erase(client);
					clients.erase(client);
					clientsStatistics.erase(client);
				}
			}
			return;
		}

		// pops the last read
		clientsQs[client].pop();
		clientsStatistics[client].packetsSent++;
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

