#pragma once


#include <iostream>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <map>
#include <queue>
#include <thread>
#include <boost/asio.hpp>
#include <opencv2/opencv.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


#include "ApplicationStatus.h"
#include "Logger.h"
#include "NetworkStatistics.h"

using boost::asio::ip::tcp;

class RemoteControlServer; // forward declaration for the RemoteClient implementation
const unsigned int RemoteClientHeaderLength = sizeof(uint32_t);
const unsigned int RemoteClientMaxIncomingMessageLength= 1024*1024*100; // 100kb

class RemoteClient : public std::enable_shared_from_this<RemoteClient>
{
	// member variables
	RemoteControlServer& server;

	// keeps track of messages received and sent
	NetworkStatistics statistics;

	// read buffer
	boost::asio::streambuf request;

	// write buffer (we can only call one async write once, so we have to buffer messages until they are fully written)
	std::queue<std::shared_ptr<std::vector<uchar> > > outputMessageQ;

	// all the messages received will be stored here until they are consumed
	uint32_t incomingMessageLength;

	// client connection
	std::shared_ptr<tcp::socket> socket;


	// constructor is private to force everyone to make a shared_copy
	RemoteClient(RemoteControlServer& server, std::shared_ptr<tcp::socket> connection);

	// called when done writing to cleint
	static void write_done(std::shared_ptr<RemoteClient> client, std::shared_ptr < std::vector<uchar> > buffer, const boost::system::error_code& error, std::size_t bytes_transferred);
	static void write_next_message(std::shared_ptr<RemoteClient> client);

	// start reading for the client
	static void read_header_async(std::shared_ptr<RemoteClient> client);
	static void read_message_async(std::shared_ptr<RemoteClient> client, const boost::system::error_code& error, std::size_t bytes_transferred);
	static void read_message_done (std::shared_ptr<RemoteClient> client, std::shared_ptr < std::vector<uchar> > buffer, const boost::system::error_code& error, std::size_t bytes_transferred);

	// book-keeping
	std::string remoteAddress;
	int remotePort;

protected:
	static void write_request(std::shared_ptr<RemoteClient> client, std::shared_ptr<std::vector<uchar> > message);
	friend class RemoteControlServer;
public:

	// This is the only way of creating a remote client
	static std::shared_ptr<RemoteClient> createClient(RemoteControlServer& server, std::shared_ptr<tcp::socket> connection)
	{
		std::shared_ptr<RemoteClient> c(new RemoteClient(server, connection));
		return c;
	}

	void close();
	bool send(std::shared_ptr<std::vector<uchar> > message);
	bool message(const std::string& content);

	bool isConnected() const
	{
		return (socket && socket->is_open());
	}

	const std::string& RemoteAddress() const
	{
		return remoteAddress;
	}

	int RemotePort() const
	{
		return remotePort;
	}

	// destructor
	~RemoteClient();
};


class RemoteControlServer
{

	// support remote commands
	std::map < std::string, std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> > remoteCommandsCallbacks;

	// keeps track of information from the application
	std::shared_ptr<ApplicationStatus> appStatus;

public:
	RemoteControlServer(std::shared_ptr<ApplicationStatus> appStatus,
		std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> startCameraCallback,
		std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> stopCameraCallback,
		std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> startRecordingCallback,
		std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> stopRecordingCallback,
		std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> shutdownCallback,
		std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> changeExposureCallback,
		std::function<void(std::shared_ptr<RemoteClient>, const rapidjson::Document&)> changeGainCallback) : appStatus(appStatus), acceptor(io_service, tcp::endpoint(tcp::v4(), appStatus->GetControlPort()))
	{
		using namespace std::placeholders; // for  _1, _2, ...
		remoteCommandsCallbacks["startCamera"] = startCameraCallback;
		remoteCommandsCallbacks["stopCamera"] = stopCameraCallback;
		remoteCommandsCallbacks["startRecording"] = startRecordingCallback;
		remoteCommandsCallbacks["stopRecording"] = stopRecordingCallback;
		remoteCommandsCallbacks["shutdown"] = shutdownCallback;
		remoteCommandsCallbacks["changeExposure"] = changeExposureCallback;
		remoteCommandsCallbacks["changeGain"] = changeGainCallback;
		remoteCommandsCallbacks["ping"] = std::bind(&RemoteControlServer::pingRequest, this, _1, _2);
	};

	~RemoteControlServer()
	{
		Stop();
	}

	bool IsThreadRunning()
	{
		return (sThread && sThread->joinable());
	}

	void Run()
	{
		sThread.reset(new std::thread(std::bind(&RemoteControlServer::thread_main, this)));
	}

	// sends a color and depth frame to all clients connected
	void ForwardToAll(const std::string& messageStr)
	{
		// if not running
		if (!sThread) return;

		// first, let's make sure that this method gets called from the same thread that
		// is running the server
		if (std::this_thread::get_id() != sThread->get_id())
		{
			this->io_service.post(std::bind(&RemoteControlServer::ForwardToAll, this, messageStr));
			return;
		}

		// prepares the message
		std::shared_ptr<std::vector<uchar> > message = std::make_shared<std::vector<uchar> >(sizeof(uint32_t) + messageStr.length());

		// header [message length]
		*((uint32_t*) & (*message)[0]) = messageStr.length();

		// gets message as an array of chars
		const char* messageBuffer = messageStr.c_str();

		// write color frame
		memcpy((unsigned char*) & (*message)[4], messageBuffer, messageStr.size());

		// sends to all clients
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);

			for (std::shared_ptr<RemoteClient> client : clients)
			{
				// the server is a friend class so that it can bypass an individual
				// thread class per client
				RemoteClient::write_request(client, message);
			}
		}
	}

	void Stop()
	{
		if (IsThreadRunning())
		{
			// stops io service
			io_service.stop();

			// is it running ?
			if (sThread && sThread->joinable())
				sThread->join();

			sThread = nullptr;
		}

		// any clients connected? (close all the ongoing connections)
		for (std::shared_ptr<RemoteClient> client : clients)
		{
			client->close();

			// callbacks won't be called because io_service is not running anymore
		}

		// erase list of clients
		clients.clear();

	}

	bool DisconnectClient(std::shared_ptr<RemoteClient> client)
	{
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);


			// do we have this client in file?
			if (clients.find(client) != clients.end())
			{
				// removes client from the list
				client->close();
				clients.erase(client);
				return true;
			}
		}

		// client did not exist?
		return false;
	}

	// event queue
	boost::asio::io_service io_service;


private:
	// tcp server that listens and waits for clients
	tcp::acceptor acceptor;

	// pointer to the thread that will be managing client connections
	std::shared_ptr<std::thread> sThread;

	std::set<std::shared_ptr<RemoteClient> > clients;
	std::mutex clientSetMutex;	

	// this method implements the main thread for TCPStreamingServer
	void thread_main()
	{

		Logger::Log("Remote") << "Waiting for connections on port " << appStatus->GetControlPort() << std::endl;
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
		acceptor.async_accept(*newClient, std::bind(&RemoteControlServer::async_handle_accept, this, newClient, _1));
	}

	// as soon as a new client connects, adds client to the list and waits for a new connection
	void async_handle_accept(std::shared_ptr<tcp::socket> newClient, const boost::system::error_code& error)
	{
		// adds a new client to the list
		if (!error)
		{
			const std::lock_guard<std::mutex> lock(clientSetMutex);
			std::shared_ptr<RemoteClient> newRemoteClient = RemoteClient::createClient(*this, newClient);
			clients.insert(newRemoteClient);
			RemoteClient::read_header_async(newRemoteClient);
		}

		// accepts a new connection
		aync_accept_connection();
	}


	void ParseMessage(std::shared_ptr < std::vector<uchar> > messageBuffer, std::shared_ptr<RemoteClient> client)
	{
		if (!messageBuffer)
			return;

		if (!client)
		{
			Logger::Log("Remote") << "Error! Cannot parse message without client!" << std::endl;
			return;
		}


		rapidjson::Document message;
		try
		{
			// parses message
			message.Parse((const char*) & (*messageBuffer)[0], messageBuffer->size());

			if (message.HasMember("type"))
			{
				std::string messageType = message["type"].GetString();

				auto messageCallback = remoteCommandsCallbacks.find(messageType);
				
				
				// if we have a callback for the message
				if (messageCallback != remoteCommandsCallbacks.end())
				{
					Logger::Log("Remote") << '[' << client->remoteAddress << ':' << client->remotePort << ']' << " message with type " << messageType << " received!" << std::endl;
					
					try
					{
						messageCallback->second(client, message);
					}
					catch (const std::exception & e)
					{
						Logger::Log("Remote") << '[' << client->remoteAddress << ':' << client->remotePort << ']' << " Exception throw while handling message " << messageType <<  " : " << e.what() << std::endl;
					}
				}
				else
				{
					Logger::Log("Remote") << '[' << client->remoteAddress << ':' << client->remotePort << ']' << " Error! Invalid message with type " << messageType << " received!" << std::endl;
				}

			}
			else {
				Logger::Log("Remote") << '[' << client->remoteAddress << ':' << client->remotePort << ']' << "Invalid messaged received (type is missing)" << std::endl;
			}

		}
		catch (const std::exception & e)
		{
			Logger::Log("Remote") << '[' << client->remoteAddress << ':' << client->remotePort << ']' << " Error parsing message: " << e.what() << std::endl;	
		}
	}


	friend class RemoteClient;

	// tell the client what's up
	void pingRequest(std::shared_ptr<RemoteClient> client, const rapidjson::Document& message)
	{
		// it only makes sense to message the client if the client is still connected
		if (client->isConnected())
		{

			rapidjson::Document pongMessage = appStatus->GetApplicationStatusJSON();
			pongMessage.AddMember("type", "pong", pongMessage.GetAllocator());
			rapidjson::StringBuffer buffer;
			rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
			pongMessage.Accept(writer);

			// serializes json
			const char* output = buffer.GetString();

			// sends message to client
			client->message(output);
			Logger::Log("Remote") << '[' << client->RemoteAddress() << ':' << client->RemotePort() << ']' << " Send Pong ! " << output << std::endl;

		}
		else {
			Logger::Log("Remote") << '[' << client->RemoteAddress() << ':' << client->RemotePort() << ']' << " Could not send Pong message back!" << std::endl;
		}
	}

};

