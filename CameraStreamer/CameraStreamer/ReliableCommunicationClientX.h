#pragma once

#include <chrono>
#include <queue>
#include <tuple>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp> // future work -> move from io_context to io_context
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>
#include <boost/lexical_cast.hpp>

#include "CommsErrors.h"
#include "NetworkStatistics.h"
#include "NetworkBuffer.h"

namespace comms
{

	class ReliableCommunicationClientX;

	typedef std::function<void(std::shared_ptr<ReliableCommunicationClientX>, const boost::system::error_code&)> ReliableCommunicationCallback;

/**
 * ReliableCommunicationClientX is a TCP Client from the series
   Comms by Weibel Lab - see https://github.com/weibellab/comms

   v 1.0.0

   X stands for a generic protocol (and not necessarily the LengthValue used by most Comms) sockets.
   Hence, ReliableCommunicationClientX is an abstracted version of asio::tcp_client that has
   callbacks for timeouts. Implementing a protocol requires user intervention
   
   Being an asynchronous TCP client, it provides users with the power to put a timeout on specific
   functions (connect, read, write). However, users should only expect for callbacks when they are 
   set and when the request operation (connect, write, read) returns true. If the requested operation
   returns false, then the user should investigate whether or not a close has been invoked or the socket
   is not set yet.

   Notes:
   - ReliableCommunicationClientX can only be created as a shared_ptr through the method Create
     as most of its internal methods keep a shared_ptr to itself

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
	ReliableCommunicationClientX(boost::asio::io_context& io_context) : io_context(io_context), tag(0),
		connectDeadlineTimer(io_context), readDeadlineTimer(io_context), stopRequested (false), readOperationPending(false),
		readCallbackInvoked(false), connectCallbackInvoked(false) {}


	// constructor that receives an existing socket (probably connected)
	ReliableCommunicationClientX(std::shared_ptr<boost::asio::ip::tcp::socket> connection, bool incomingConnection = true) : ReliableCommunicationClientX(io_context) // c++11 only (delegating constructors)
	{
		tcpClient = connection;
		updateConnectionDetails();
		networkStatistics.incomingConnection = incomingConnection;
	}


	// method invoked asynchronously when done connecting to a client
	void connect_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, ReliableCommunicationCallback onConnectDoneCallback,
		const boost::system::error_code& error,	const boost::asio::ip::tcp::endpoint& endpoint);

	// method invoked asynchronously when a connect operation is taking too long
	void connect_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, ReliableCommunicationCallback onConnectDoneCallback,
		const boost::system::error_code& error);

	// write request added to the event loop queue whenever requesting to write to a client. Writes the entire buffer
	void write_request(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, NetworkBufferPtr buffer,
		ReliableCommunicationCallback onConnectDoneCallback = {});

	// method invoked asynchronously when a write operation has finalized
	void write_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, NetworkBufferPtr buffer,
					const boost::system::error_code& error, std::size_t bytes_transferred);

	// method invoked within write_done to start writing the next buffer in the queue
	void write_next_buffer(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper);

		// method invoked asynchronously when a read operation has finalized
	void read_request_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, NetworkBufferPtr buffer,
		const ReliableCommunicationCallback& onReadCallback, size_t bytes_requested, const boost::system::error_code& error,
		std::size_t bytes_transferred);

	// method invoked asynchronously when a read operation is taking too long
	void read_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, const ReliableCommunicationCallback& onReadCallback,
		const boost::system::error_code& error);
	
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


	// the core of boost::asio is an io_context
	// this socket class either receives one directly or it inherits one from a socket
	boost::asio::io_context& io_context;
	std::shared_ptr<boost::asio::ip::tcp::socket> tcpClient;

	// write buffer (users can only call one write at a time, so this class buffers repetead requests)
	typedef std::pair<NetworkBufferPtr, ReliableCommunicationCallback> bufferCallbackTuple;
	std::queue<bufferCallbackTuple> outputMessageQ;

	// tag can be set by the user to uniquely identify a connection
	int tag;

	// timeout timers
	boost::asio::steady_timer connectDeadlineTimer;
	boost::asio::steady_timer readDeadlineTimer;
	bool stopRequested;
	bool readOperationPending;
	bool readCallbackInvoked;
	bool connectCallbackInvoked;
	
public:

	NetworkStatistics networkStatistics;

	// creates a ReliableCommunicationClientX based on an existing TCPClient (e.g., when using a TCPServer). If not, set incomingConnection to false
	static std::shared_ptr<ReliableCommunicationClientX> createClient(std::shared_ptr<boost::asio::ip::tcp::socket> connection, bool incomingConnection = true)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(connection, incomingConnection));
		return c;
	}

	// creates a RelaibleCommunicationClientX based on an existing io_context
	static std::shared_ptr<ReliableCommunicationClientX> createClient(boost::asio::io_context& io_context)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(io_context));
		return c;
	}

	// returns true if socket is connected
	bool connected()
	{
		return (tcpClient && tcpClient->is_open());
	}

	bool connect(const std::string& host, int port, const ReliableCommunicationCallback& onConnectCallback)
	{
		using namespace std::placeholders; // for  _1, _2, ...
		// cannot connect when already connected
		if (connected())
			return false;

		boost::asio::ip::tcp::resolver resolver(io_context);
		auto endpoints = resolver.resolve(host, boost::lexical_cast<std::string>(port));
		tcpClient = std::make_shared<boost::asio::ip::tcp::socket>(io_context);

		boost::asio::async_connect(*tcpClient, endpoints, std::bind(&ReliableCommunicationClientX::connect_done, this, shared_from_this(), _1, _2));

		return true;
	}

	/// <summary>
	/// Connects to a tcp server running at host:port.
	/// Waits a max of @timeout ms before cancelling all operations if unable to connect
	/// (Accepts hostnames, ipv4, ipv6 and other operating-system dependent URIs)
	/// </summary>
	/// <param name="host">hostname</param>
	/// <param name="port">port</param>
	/// <param name="timeout">time</param>
	/// <returns></returns>
	bool connect(const std::string& host, int port, std::chrono::milliseconds timeout, const ReliableCommunicationCallback& onConnectCallback)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// cannot connect when already connected
		if (connected())
			return false;

		// sets deadline for as many milliseconds as the user requested
		connectDeadlineTimer.expires_from_now(timeout);
		connectDeadlineTimer.async_wait(std::bind(&ReliableCommunicationClientX::connect_timeout_done, this, shared_from_this(), _1));

		// invokes connect
		return connect(host, port, onConnectCallback);

	}

	const std::string& remoteAddress() const { return networkStatistics.remoteAddress; }
	int remotePort() const { return networkStatistics.remotePort; }
	const std::string& localAddress() const { return networkStatistics.localAddress; }
	int localPort() const { return networkStatistics.localPort; }
	int getTag() const { return tag; }
	void setTag(int val) { tag = val; }

	// (non-blocking) writes a buffer to the remote endpoint (no protocol). returns false if a stop was requested
	bool write(NetworkBufferPtr& buffer)
	{
		if (stopRequested)
			return false;

		boost::asio::post(io_context, std::bind(&ReliableCommunicationClientX::write_request, this, shared_from_this(), buffer));
		return true;
	}

	// (non-blocking) reads as many bytes as specified in @param count into @param buffer. returns false if socket is not connected or stopped
	void read(NetworkBufferPtr& buffer, size_t count, const ReliableCommunicationCallback& onReadCallback, std::chrono::milliseconds timeout = std::chrono::milliseconds{ 0 });

	// stops socket
	void close(const boost::system::error_code& error = boost::system::error_code())
	{
		stopRequested = true;
		if (tcpClient)
		{
			bool isConnected = tcpClient->is_open();

			// gracefully shutdown connections and stops all asynchronous operations
			boost::system::error_code e;
			tcpClient->shutdown(boost::asio::ip::tcp::socket::shutdown_both, e);

			// cancel pending operations for this socket
			tcpClient->close(e);

			// let others know that this socket is no more - this should be called after all pending operations are done failing
			if (isConnected && onDisconnected)
			{
				boost::asio::post(io_context, std::bind(onDisconnected, shared_from_this(), error));
			}
		}

		// this makes sure that the same socket cannot be re-used 
		// (thereby not messing with pending operations)
		tcpClient == nullptr;
	}

	// destructor
	~ReliableCommunicationClientX()
	{
		close();
	}

	// invoked when the socket disconnected from the server
	std::function<void(std::shared_ptr<ReliableCommunicationClientX>, const boost::system::error_code&)> onDisconnected;

};

}