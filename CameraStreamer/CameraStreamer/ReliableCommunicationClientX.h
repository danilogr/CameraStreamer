#pragma once

#include <chrono>
#include <queue>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp> // future work -> move from io_service to io_context
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include "NetworkStatistics.h"
#include "NetworkBuffer.h"

/**
 * ReliableCommunicationClientX is a TCP Client from the series
   Comms by Weibel Lab - see https://github.com/weibellab/comms

   v 1.0.0

   X stands for a generic protocol (and not necessarily the LengthValue used by most Comms) sockets.
   Hence, ReliableCommunicationClientX is an abstracted version of asio::tcp_client that has
   callbacks for timeouts. Implementing a protocol requires user intervention
   
   Being an asynchronous TCP client, it provides users with the power to put a timeout on specific
   functions (connect, read, write).

   Callbacks:
   - onConnected
   - onDisconnected
   - onError
   - onConnectTimeout
   - onWriteTimeout
   - onWriteError
   - onReadTimeout
   - onReadError


   Inspiration:
   https://www.boost.org/doc/libs/1_73_0/doc/html/boost_asio/example/cpp03/timeouts/async_tcp_client.cpp

   Notes:
   - TCPClientX can only be created as a shared_ptr through the method Create as most of its
     internal methods keep a shared_ptr to itself

   Author:
   - Danilo Gasques (gasques@ucsd.edu)
 */

class ReliableCommunicationClientX : public std::enable_shared_from_this<ReliableCommunicationClientX>
{
protected:

	//
	// Construtors
	//


	// constructors are private to force everyone to make a shared_copy
	ReliableCommunicationClientX(boost::asio::io_service& io_service) : io_service(io_service), remotePort(0), localPort(0), tag(0),
	writeTimeout(io_service), readTimeout(io_service), stopRequested (false), readOperationPending(false) {}


	// constructor that receives an existing socket (probably connected)
	ReliableCommunicationClientX(std::shared_ptr<boost::asio::ip::tcp::socket> connection, bool incomingConnection = true) : ReliableCommunicationClientX(io_service) // c++11 only (delegating constructors)
	{
		tcpClient = connection;
	}

	
	//
	// main static methods - most of the socket functions are written as static methods
	// because they will access the socket indirectly through a shared pointer
	//

	// called when done writing to cleint
	static void write_done(std::shared_ptr<ReliableCommunicationClientX> client,
		NetworkBufferPtr buffer, const boost::system::error_code& error, std::size_t bytes_transferred);

	static void write_next_message(std::shared_ptr<ReliableCommunicationClientX> client);

	// start reading for the client
	static void read_header_async(std::shared_ptr<ReliableCommunicationClientX> client);

	static void read_request(std::shared_ptr<ReliableCommunicationClientX> client, NetworkBufferPtr buffer,
		size_t length);

	static void read_message_done(std::shared_ptr<ReliableCommunicationClientX> client,
		NetworkBufferPtr buffer, size_t bytes_requested, const boost::system::error_code& error, std::size_t bytes_transferred);

	static void write_request(std::shared_ptr<ReliableCommunicationClientX> client,
		NetworkBufferPtr message);

	//
	// methods used to update client related details
	//
	 
	void updateConnectionDetails()
	{
		if (tcpClient && tcpClient->is_open())
		{
			networkStatistics.remoteAddress = tcpClient->remote_endpoint().address().to_string();
			networkStatistics.remotePort    = tcpClient->remote_endpoint().port();

			networkStatistics.localAddress = tcpClient->local_endpoint().address().to_string();
			networkStatistics.localPort    = tcpClient->local_endpoint().port();
		}
	}

	//
	// Core implementation
	//


	// the core of boost::asio is an io_service
	// this socket class either receives one directly or it inherits one from a socket
	boost::asio::io_service& io_service;
	std::shared_ptr<boost::asio::ip::tcp::socket> tcpClient;

	// write buffer (users can only call one write at a time, so this class buffers repetead requests)
	std::queue<NetworkBufferPtr> outputMessageQ;

	// book keeping
	std::string remoteAddress, localAddress;
	int remotePort, localPort, tag;

	// timeout timers
	boost::asio::steady_timer writeTimeout;
	boost::asio::steady_timer readTimeout;
	bool stopRequested;
	bool readOperationPending;
	
public:

	NetworkStatistics networkStatistics;

	// creates a ReliableCommunicationClientX based on an existing TCPClient (e.g., when using a TCPServer). If not, set incomingConnection to false
	static std::shared_ptr<ReliableCommunicationClientX> createClient(std::shared_ptr<boost::asio::ip::tcp::socket> connection, bool incomingConnection = true)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(connection, incomingConnection));
		return c;
	}

	// creates a RelaibleCommunicationClientX based on an existing io_service
	static std::shared_ptr<ReliableCommunicationClientX> createClient(boost::asio::io_service& io_service)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(io_service));
		return c;
	}

	// returns true if socket is connected
	bool connected()
	{
		return (socket && socket->is_open());
	}

	void connect();

	const std::string& remoteAddress() const { return networkStatistics.remoteAddress; }
	int remotePort() const { return networkStatistics.remotePort; }
	const std::string& localAddress() const { return networkStatistics.localAddress; }
	int localPort() const { return networkStatistics.localPort; }


	// (non-blocking) writes a buffer to the remote endpoint (no protocol). returns false if a stop was requested
	bool write(NetworkBufferPtr buffer)
	{
		if (stopRequested)
			return false;

		io_service.post(std::bind(&ReliableCommunicationClientX::write_request, shared_from_this(), buffer));
		return true;
	}

	// (non-blocking) reads as many bytes as specified in @param count into @param buffer. returns false if socket is not connected or stopped
	bool read(NetworkBufferPtr buffer, size_t count)
	{
		if (stopRequested)
			return false;

		io_service.post(std::bind(&ReliableCommunicationClientX::read_request, shared_from_this(), buffer));
		return true;
	}

	// (non-blocking) reads as many bytes as specified in @param count into @param buffer. returns false if socket is not connected or stopped
	bool read(NetworkBufferPtr buffer, size_t count, std::chrono::milliseconds timeout)
	{
		if (stopRequested)
			return false;
	}

	// stops socket
	void close()
	{
		stopRequested = true;
		if (tcpClient && tcpClient->is_open())
		{
			tcpClient->close();
		}
	}

	// destructor
	~ReliableCommunicationClientX()
	{
		close();
		tcpClient = nullptr;
	}

};

