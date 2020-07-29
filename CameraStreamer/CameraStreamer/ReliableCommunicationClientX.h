#pragma once

#include <chrono>
#include <queue>
#include <tuple>

// asynchronous networking api
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/io_context.hpp>

// custom boost::system::error_code errors
#include "CommsErrors.h"

// network statistics data structure
#include "NetworkStatistics.h"

// network buffer pointer -> wraps varied shared_pointers
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
	ReliableCommunicationClientX(boost::asio::io_context& io_context_) : io_context(io_context_), tag(0),
		connectDeadlineTimer(io_context_), readDeadlineTimer(io_context_), stopRequested (false), readOperationPending(false),
		readCallbackInvoked(false), connectCallbackInvoked(false), socketEverConnect(false), pendingConnectCallbacks(0) {}


	// constructor that receives an existing socket (probably connected)
	ReliableCommunicationClientX(boost::asio::io_context& io_context_,
		std::shared_ptr<boost::asio::ip::tcp::socket> connection, bool incomingConnection = true) 
		: ReliableCommunicationClientX(io_context_) // c++11 only (delegating constructors)
	{
		tcpClient = connection;
		networkStatistics.incomingConnection = incomingConnection;

		// update statistics, remote address, local address, etc..
		// (doesn't invoke an onConnect callback as this class inherited an open socket)
		if (tcpClient->is_open())
		{
			socketEverConnect = true;
			updateConnectionDetails();
		}
	}


	// method invoked asynchronously when done connecting to a client
	void connect_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, const ReliableCommunicationCallback& onReadCallback,
		const boost::system::error_code& error,	const boost::asio::ip::tcp::endpoint& endpoint);

	// method invoked asynchronously when a connect operation is taking too long
	void connect_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, const ReliableCommunicationCallback& onReadCallback,
		const boost::system::error_code& error);

	// method invoked asynchronously when a write operation has finalized
	void write_request_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, NetworkBufferPtr buffer,
		const ReliableCommunicationCallback& onWriteCallback, const boost::system::error_code& error, std::size_t bytes_transferred);

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
	typedef std::tuple<NetworkBufferPtr, ReliableCommunicationCallback> BufferCallbackTuple;
	std::queue<BufferCallbackTuple> outputMessageQ;

	// tag can be set by the user to uniquely identify a connection
	int tag;

	// timeout timers
	boost::asio::steady_timer connectDeadlineTimer;
	boost::asio::steady_timer readDeadlineTimer;
	bool stopRequested;
	bool readOperationPending;
	bool readCallbackInvoked;
	bool connectCallbackInvoked;
	bool socketEverConnect;

	// when connecting to a host, a set of different options might be available. 
	// this variable makes sure to report to the user only when the last try is finalized
	int pendingConnectCallbacks; 
	
public:

	NetworkStatistics networkStatistics;

	// creates a ReliableCommunicationClientX based on an existing TCPClient (e.g., when using a TCPServer). If not, set incomingConnection to false
	static std::shared_ptr<ReliableCommunicationClientX> createClient(boost::asio::io_context& io_context_,
		std::shared_ptr<boost::asio::ip::tcp::socket> connection, bool incomingConnection = true)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(io_context_, connection, incomingConnection));
		return c;
	}

	// creates a RelaibleCommunicationClientX based on an existing io_context
	static std::shared_ptr<ReliableCommunicationClientX> createClient(boost::asio::io_context& io_context_)
	{
		std::shared_ptr<ReliableCommunicationClientX> c(new ReliableCommunicationClientX(io_context_));
		return c;
	}

	/// <summary>
	/// Returns true if the socket is connected and ready to read/write
	/// </summary>
	/// <returns></returns>
	bool connected()
	{
		return (tcpClient && tcpClient->is_open());
	}

	const std::string& remoteAddress() const { return networkStatistics.remoteAddress; }
	int remotePort() const { return networkStatistics.remotePort; }
	const std::string& localAddress() const { return networkStatistics.localAddress; }
	int localPort() const { return networkStatistics.localPort; }
	int getTag() const { return tag; }
	void setTag(int val) { tag = val; }


	/// <summary>
	/// (non-blocking) Connects remote host at location @host:@port
	///
	/// onConnectCallback is always called (asynchronously through io_context) and it either has a sucessful state ( error == 0 )
	/// or an error indicating that the socket is already connected, already connecting, timedout, or couldn't connect
	/// </summary>
	/// <param name="host">remote host</param>
	/// <param name="port">remote port</param>
	/// <param name="onConnectCallback"></param>
	/// <param name="timeout">cancel the operation after the time specified here. Use zero to wait forever</param>
	void connect(const std::string& host, int port, const ReliableCommunicationCallback& onConnectCallback, std::chrono::milliseconds timeout = std::chrono::milliseconds{ 0 });

	/// <summary>
	/// (non-blocking) Sends the entire content of the buffer to the other end
	/// </summary>
	/// <param name="buffer"></param>
	/// <param name="onWriteCallback"></param>
	void write(NetworkBufferPtr& buffer, const ReliableCommunicationCallback& onWriteCallback = {});


	/// <summary>
	/// (non-blocking) Reads @count bytes to @buffer before @timeout. Calls @onReadCallback when done, timed out, or failed
	/// </summary>
	/// <param name="buffer">NetworkBufferPtr with pre-allocated memory to hold at least @count bytes</param>
	/// <param name="count">number of bytes to read</param>
	/// <param name="onReadCallback">(optional) callback to invoked when done</param>
	/// <param name="timeout">(optional) time in milliseconds to wait for read operation to complete</param>
	void read(NetworkBufferPtr& buffer, size_t count, const ReliableCommunicationCallback& onReadCallback, std::chrono::milliseconds timeout = std::chrono::milliseconds{ 0 });

	// stops socket
	void close(const boost::system::error_code& error = boost::system::error_code(), bool disposing = false)
	{
		// nothing to do here
		if (stopRequested || !tcpClient)
			return;

		// makes sure that any pending operations are stopped right away
		stopRequested = true;

		// if the tcpClient is availble, then
		if (tcpClient)
		{
			// gracefully shutdown connections and stops all asynchronous operations
			boost::system::error_code e;
			tcpClient->shutdown(boost::asio::ip::tcp::socket::shutdown_both, e);

			// cancel pending operations for this socket
			tcpClient->close(e);
		}

		// did we ever connect and invoke onConnect ?
		if (socketEverConnect)
		{
			socketEverConnect = false; // no need for this anymore

			// let others know that this socket is no more - this should be called after all pending operations are done failing
			if (onDisconnected)
			{
				boost::asio::post(io_context, std::bind(onDisconnected, disposing ? nullptr : shared_from_this(), error));
			}
		}

	}

	// destructor
	~ReliableCommunicationClientX()
	{
		close(boost::system::error_code(), true);
	}

	/// <summary>
	/// Callback invoked when disconnected
	/// </summary>
	std::function<void(std::shared_ptr<ReliableCommunicationClientX>, const boost::system::error_code&)> onDisconnected;

};

}