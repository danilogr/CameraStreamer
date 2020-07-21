#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
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
   - onReadTimeout

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

	// the core of boost::asio is an io_context
	// this socket class either receives one directly or it inherits one from a socket
	boost::asio::io_context& io_context;
	std::shared_ptr<boost::asio::ip::tcp::socket> tcpClient;


	// constructors are private to force everyone to make a shared_copy
	ReliableCommunicationClientX(boost::asio::io_context& io_context) : io_context(io_context), remotePort(0), localPort(0), tag(0) {}


	// constructor that receives an existing socket (probably connected)
	ReliableCommunicationClientX(std::shared_ptr<boost::asio::ip::tcp::socket> connection) : ReliableCommunicationClientX(io_context) // c++11 only (delegating constructors)
	{
		tcpClient = connection;
	}

	
	//
	// main static methods - most of the socket functions are written as static methods
	// because they will access the socket indirectly through a shared pointer
	//

	// called when done writing to cleint
	static void write_done(std::shared_ptr<ReliableCommunicationClientX> client, std::shared_ptr <NetworkBuffer> buffer, const boost::system::error_code& error, std::size_t bytes_transferred);
	static void write_next_message(std::shared_ptr<ReliableCommunicationClientX> client);

	// start reading for the client
	static void read_header_async(std::shared_ptr<ReliableCommunicationClientX> client);
	static void read_message_async(std::shared_ptr<ReliableCommunicationClientX> client, const boost::system::error_code& error, std::size_t bytes_transferred);
	static void read_message_done(std::shared_ptr<ReliableCommunicationClientX> client, std::shared_ptr <NetworkBuffer> buffer, const boost::system::error_code& error, std::size_t bytes_transferred);

	// book keeping
	std::string remoteAddress, localAddress;
	int remotePort, localPort, tag;
	
public:

	// creates a ReliableCommunicationClientX based on an existing TCPClient (e.g., when using a TCPServer)
	static std::shared_ptr<ReliableCommunicationClientX> createClient(std::shared_ptr<boost::asio::ip::tcp::socket> connection)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(connection));
		return c;
	}

	// creates a RelaibleCommunicationClientX based on an existing io_context
	static std::shared_ptr<ReliableCommunicationClientX> createClient(boost::asio::io_context& io_context)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(io_context));
		return c;
	}

	bool connected();

	void read(int bytes);
	void connect();

	// closes
	void close();

	// destructor
	~ReliableCommunicationClientX();

};

