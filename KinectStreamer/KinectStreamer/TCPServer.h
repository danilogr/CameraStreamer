#pragma once

#include "Frame.h"

#include <iostream>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

using boost::asio::ip::tcp;

/*
  The TCPServer class sends kinect color and depth
  frames to all members connected.

  TCPServer runs on a separate thread
*/
class TCPServer
{
public:
	TCPServer(unsigned int port) : acceptor(io_service, tcp::endpoint(tcp::v4(), port)) {
		std::cout << "[TCPServer] - Listening on " << port << std::endl;
	};
	~TCPServer()
	{
		// stops io service
		io_service.stop();
		
		// joins the thread
		if (sThread)
			sThread->join();

		// any client still connected?
		for (std::shared_ptr<tcp::socket> client : clients)
		{
			client->close();
		}
	}

	void Run()
	{
		sThread.reset(new std::thread(std::bind(&TCPServer::thread_main, this)));
	}



	// sends a color and depth frame to all clients connected
	void ForwardToAll(unsigned int width, unsigned int height,
		              std::shared_ptr<Frame> color, std::shared_ptr<Frame> depth)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// prepares the message
		boost::asio::streambuf response;
		std::ostream message(&response);

		// header [width][height][rgb length][depth length]
		uint32_t toWrite = width;
		message.write((const char * )& toWrite, sizeof(toWrite));

		toWrite = height;
		message.write((const char * )& toWrite, sizeof(toWrite));

		toWrite = color->size();
		message.write((const char *)& toWrite, sizeof(toWrite));

		toWrite = depth->size();
		message.write((const char *)& toWrite, sizeof(toWrite));

		// write color frame
		message.write((const char *) color->getData(), color->size());

		// write depth frame
		message.write((const char*) depth->getData(), depth->size());


		// sends to all clients
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);

			for (std::shared_ptr< tcp::socket> client : clients)
			{
				boost::asio::async_write(*client, response, std::bind(&TCPServer::write_done, this, client, _1, _2));
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

	// set with all clients currently connected to the server
	std::set<std::shared_ptr< tcp::socket> > clients;
	std::mutex clientSetMutex;


	// this method implements the main thread for TCPServer
	void thread_main()
	{
		std::cout << "[TCPServer] - Waiting for connections" << std::endl;
		aync_accept_connection(); // adds some work to the io_service, otherwise it exits
		io_service.run();	      // starts listening for connections
	}

	// waits for connections
	void aync_accept_connection()
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// creates a new socket to received the connection
		std::shared_ptr<tcp::socket> newClient = std::make_shared<tcp::socket>(io_service);

		// waits for a new connection
		acceptor.async_accept(*newClient, std::bind(&TCPServer::async_handle_accept, this, newClient, _1));
	}

	// as soon as a new client connects, adds client to the list and waits for a new connection
	void async_handle_accept(std::shared_ptr<tcp::socket> newClient, const boost::system::error_code& error)
	{
		// adds a new client to the list
		if (!error)
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);
			clients.insert(newClient);

			std::cout << "[TCPServer] - New client connected: " << newClient->remote_endpoint().address().to_string() << ':' << newClient->remote_endpoint().port() << std::endl;

		}

		// accepts a new connection
		aync_accept_connection();
	}

	// called when done writing to cleint
	void write_done(std::shared_ptr<tcp::socket> client, const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// there's nothing much we can do here besides remove the client if we get an error sending to it
		if (error)
		{
			std::cout << "[TCPServer] - Client " << client->remote_endpoint().address().to_string() << ':' << client->remote_endpoint().port() << " disconnected" << std::endl;
			{
				const std::lock_guard<std::mutex> lock(clientSetMutex);
				clients.erase(client);
			}
		}
	}

	//void async_client_read()
	//{
    //
	//}


};

