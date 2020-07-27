#include "ReliableCommunicationClientX.h"

namespace comms
{

	void ReliableCommunicationClientX::write(NetworkBufferPtr& message, const ReliableCommunicationCallback& onWriteCallback)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// operation aborted
		if (stopRequested)
		{
			++networkStatistics.messagesDropped; // whoops
			if (onWriteCallback)
				boost::asio::post(io_context, std::bind(onWriteCallback, shared_from_this(), boost::asio::error::operation_aborted));

			return;
		}

		// there is not even a connection here
		if (!tcpClient)
		{
			if (onWriteCallback)
				boost::asio::post(io_context, std::bind(onWriteCallback, shared_from_this(), boost::asio::error::not_connected));

			return;
		}


		// adds message to client Q
		outputMessageQ.push(BufferCallbackTuple(message, onWriteCallback));

		// if this is the first message, then there are no pending messages in the queue
		// (and no async writing pending to execute messages in the queue)
		if (outputMessageQ.size() == 1)
		{
			// something to write? let's get it!
			BufferCallbackTuple bufferCallbackTuple = outputMessageQ.back();
			NetworkBufferPtr& msgBuffer = std::get<0>(bufferCallbackTuple);

			// starts writing for this client (buffer is there for a worst case scenario)
			boost::asio::async_write(*tcpClient, boost::asio::buffer(msgBuffer.Data(), msgBuffer.Size()),
				std::bind(&ReliableCommunicationClientX::write_request_done, this, shared_from_this(), msgBuffer, std::get<1>(bufferCallbackTuple), _1, _2));
		}
	}


	// called when done writing to client
	void ReliableCommunicationClientX::write_request_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		NetworkBufferPtr buffer, const ReliableCommunicationCallback& onWriteCallback, 
		const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// updates the number of bytes sent
		networkStatistics.bytesSent += bytes_transferred;

		// lovely! let's invoke the right callback!
		if (!error)
		{
			// pops the last written message (this message)
			outputMessageQ.pop();

			// updates statistics for this client
			networkStatistics.messagesSent++;

			// invokes callback (yay!)
			if (onWriteCallback)
			{
				onWriteCallback(clientLifeKeeper, error);
			}

		}

		// did this operation fail or did someone stop the socket?
		if (error || stopRequested)
		{
			// report the correct error
			boost::system::error_code errorToReport = stopRequested ? boost::asio::error::operation_aborted : error;
			
			// update statistics (we are going to drop this and other messages that were enqueued)
			networkStatistics.messagesDropped += outputMessageQ.size();

			// clears the queue invoking all callbacks!!
			while (outputMessageQ.size() > 0)
			{
				// something to write? let's get it!
				BufferCallbackTuple bufferCallbackTuple = outputMessageQ.back();
				ReliableCommunicationCallback& onWriteCallback = std::get<1>(bufferCallbackTuple);

				// report the error to all pending write operations
				if (onWriteCallback)
				{
					onWriteCallback(clientLifeKeeper, errorToReport);
				}
				
				// pops this write request and goes to the next one
				outputMessageQ.pop();
			}

			// disconnects
			close(error);

			return;
		}

		// moves on with next writes
		if (outputMessageQ.size() > 0)
		{
			// something to write? let's get it!
			BufferCallbackTuple bufferCallbackTuple = outputMessageQ.back();
			NetworkBufferPtr& msgBuffer = std::get<0>(bufferCallbackTuple);

			// starts writing for this client
			boost::asio::async_write(*tcpClient, boost::asio::buffer(msgBuffer.Data(), msgBuffer.Size()),
				std::bind(&ReliableCommunicationClientX::write_request_done, this, shared_from_this(), msgBuffer, std::get<1>(bufferCallbackTuple), _1, _2));
		}
	}



	void ReliableCommunicationClientX::read(NetworkBufferPtr& buffer, size_t count, const ReliableCommunicationCallback& onReadCallback, std::chrono::milliseconds timeout)
	{
		static const std::chrono::milliseconds zero = std::chrono::milliseconds{ 0 };
		using namespace std::placeholders; // for  _1, _2, ...

		// another read operation in progress?
		if (readOperationPending)
		{
			if (onReadCallback)
				boost::asio::post(io_context, std::bind(onReadCallback, shared_from_this(), boost::asio::error::try_again));

			return;
		}

		// operation aborted
		if (stopRequested)
		{
			if (onReadCallback)
				boost::asio::post(io_context, std::bind(onReadCallback, shared_from_this(), boost::asio::error::operation_aborted));

			return;
		}

		// there is not even a connection here
		if (!tcpClient)
		{
			if (onReadCallback)
				boost::asio::post(io_context, std::bind(onReadCallback, shared_from_this(), boost::asio::error::not_connected));

			return;
		}

		// make sure that onReadCallback is only called once in case a time out timer is set
		readCallbackInvoked = false;

		// make sure that we block other read operations
		readOperationPending = true;

		// did we set a timer?
		if (timeout > zero)
		{
			// starts the deadline timer
			readDeadlineTimer.expires_from_now(timeout);
			readDeadlineTimer.async_wait(std::bind(&ReliableCommunicationClientX::read_timeout_done, this, shared_from_this(), _1));
		}

		// reads the entire message
		boost::asio::async_read(*tcpClient, boost::asio::buffer(buffer.Data(), count),
			boost::asio::transfer_exactly(count), std::bind(&ReliableCommunicationClientX::read_request_done, this, shared_from_this(), buffer, onReadCallback, count, _1, _2));
	}

	void ReliableCommunicationClientX::read_request_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		NetworkBufferPtr buffer, const ReliableCommunicationCallback& onReadCallback, 
		size_t bytes_requested, const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// no need for a deadline anymore
		readDeadlineTimer.cancel();

		// done reading. Allow user to request again
		readOperationPending = false;

		// saves the amount of bytes read
		networkStatistics.bytesReceived += bytes_transferred;

		// any problems?
		if (error)
		{
			// invoke read callback with error
			if (onReadCallback && !readCallbackInvoked)
			{
				readCallbackInvoked = true;
				onReadCallback(clientLifeKeeper, error);
			}

			// disconnects
			close(error);

			return;
		}

		// outcome of boost::asio::transfer_exactly
		assert(bytes_requested == bytes_transferred);

		// everything went well. this counts as a message
		++networkStatistics.messagesReceived;

		// invoke read callback to inform the user that we completed (error should be 0)
		if (onReadCallback && !readCallbackInvoked)
		{
			readCallbackInvoked = true;
			onReadCallback(clientLifeKeeper, error);
		}

	}

	void ReliableCommunicationClientX::read_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		const ReliableCommunicationCallback& onReadCallback, const boost::system::error_code& error)
	{
		// we don't need to timeout when a stop was requested or the tcpClient was never set in first place
		// (both conditions will lead to a callback getting posted by the read method)
		// case 1: condition below is valid during a call to ReliableCommunicationClientX::read (then it doesn't even start a timer / never reaches here)
		// case 2: condition below becomes valid after a call to ReliableCommunicationClientX::read
		//       -> this means that onReadCallback will be called from ReliableCommunicationClientX::read_quest_done with an error such as operation_aborted
		//       -> the timeout is not owned by the tcp socket, so it will continue normally, eventually hitting the condition below.
		//          hence, not doing anything is fine
		if (stopRequested || !tcpClient)
			return;

		// operation_aborted means that a timer was rescheduled (a read operation ended sucessfully and another one started)
		// if this timer wasn't aborted, it means that read took too long
		if (error != boost::asio::error::operation_aborted)
		{
			// prepare the socket to stop reading or writing
			boost::system::error_code e;
			tcpClient->shutdown(boost::asio::ip::tcp::socket::shutdown_both, e);

			if (!readCallbackInvoked && onReadCallback)
			{
				readCallbackInvoked = true; // in the unlikely scenario  read finalizes sucessfully at the same time as the timer, we have to avoid calling onReadCallback twice
				onReadCallback(clientLifeKeeper, comms::error::TimedOut);
			}

			// closes the connection because user requested time out
			close(comms::error::TimedOut);
		}
	}

	void ReliableCommunicationClientX::connect(const std::string& host, int port,
		const ReliableCommunicationCallback& onConnectCallback, std::chrono::milliseconds timeout)
	{
		static const std::chrono::milliseconds zero = std::chrono::milliseconds{ 0 };
		using namespace std::placeholders; // for  _1, _2, ...

		// cannot connect when already connected
		if (connected())
		{
			if (onConnectCallback)
			{
				boost::asio::post(io_context, std::bind(onConnectCallback, shared_from_this(), boost::asio::error::already_connected));
			}

			return;
		}

		// if tcpClient is defined but not connected, a connection is on the way, so we will ignore this (as a callback will come soon)
		if (tcpClient)
			return;

		// starts resolving remote address
		boost::asio::ip::tcp::resolver resolver(io_context);
		auto endpoints = resolver.resolve(host, boost::lexical_cast<std::string>(port));

		// creates a new tcpClient (safe to do here because all pending operations are done - if the socket was used before)
		tcpClient = std::make_shared<boost::asio::ip::tcp::socket>(io_context);
		connectCallbackInvoked = false;

		// sets deadline for as many milliseconds as the user requested
		if (timeout > zero)
		{
			connectDeadlineTimer.expires_from_now(timeout);
			connectDeadlineTimer.async_wait(std::bind(&ReliableCommunicationClientX::connect_timeout_done, this, shared_from_this(), onConnectCallback, _1));
		}

		boost::asio::async_connect(*tcpClient, endpoints, std::bind(&ReliableCommunicationClientX::connect_done, this, shared_from_this(), onConnectCallback, _1, _2));
	}


	void ReliableCommunicationClientX::connect_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, 
		const ReliableCommunicationCallback& onConnectCallback, const boost::system::error_code& error,
		const boost::asio::ip::tcp::endpoint& endpoint)
	{
		// no need for a deadline anymore
		connectDeadlineTimer.cancel();

		// did we connect?
		boost::system::error_code errorToReport = error;
		if (!errorToReport)
		{
			if (stopRequested || !tcpClient)
			{
				errorToReport = comms::error::Cancelled;

			} else {

				// connected. yay! updates statistics
				socketEverConnect = true;
				networkStatistics.connected();
				updateConnectionDetails();
			}
		}

		// invokes onConnectCallback if it hasn't been invoked yet
		if (onConnectCallback && !connectCallbackInvoked)
		{
			connectCallbackInvoked = true;
			onConnectCallback(clientLifeKeeper, errorToReport);
		}

	}

	void ReliableCommunicationClientX::connect_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		const ReliableCommunicationCallback& onConnectCallback,	const boost::system::error_code& error)
	{
		// we don't need to timeout when a stop was requested or the tcpClient was never set in first place
		// (both conditions will lead to an immediate error from the socket)
		if (stopRequested || !tcpClient)
			return;

		// operation_aborted means that a timer was rescheduled. We don't care about those scenarios
		// hence, if this timer wasn't aborted
		if (error != boost::asio::error::operation_aborted)
		{
			// prepare the socket to stop reading or writing
			boost::system::error_code e;
			tcpClient->shutdown(boost::asio::ip::tcp::socket::shutdown_both, e);

			// let the user know that something went wrong
			if (onConnectCallback && !connectCallbackInvoked)
			{
				onConnectCallback(clientLifeKeeper, comms::error::TimedOut);
				connectCallbackInvoked = true;
			}

			// cleans up after dealing with a bunch of callbacks
			close();
		}
	}

}