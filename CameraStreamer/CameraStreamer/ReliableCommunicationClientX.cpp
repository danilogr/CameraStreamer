#include "ReliableCommunicationClientX.h"

namespace comms
{

	void ReliableCommunicationClientX::write_request(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, NetworkBufferPtr message)
	{
		// is the client still connected?
		if (stopRequested || tcpClient)
		{
			++networkStatistics.messagesDropped; // whoops
			return;
		}

		// adds message to client Q
		outputMessageQ.push(message);

		// if this is the first message, then there is no pending messages so
		// we can start writing to clients
		if (outputMessageQ.size() == 1)
		{
			// only message? let's send it
			write_next_buffer(clientLifeKeeper);
		}
	}


	// called when done writing to client
	void ReliableCommunicationClientX::write_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		NetworkBufferPtr buffer, const boost::system::error_code& error, std::size_t bytes_transferred)
	{
		// updates the number of bytes sent
		networkStatistics.bytesSent += bytes_transferred;

		// there's nothing much we can do here besides remove the client if we get an error sending a message to it
		if (error)
		{
			// update statistics (we are going to drop this and other messages that were enqueued)
			networkStatistics.messagesDropped += outputMessageQ.size() + 1; // accounts for this message

			// clears the queue
			while (outputMessageQ.size() > 0)
				outputMessageQ.pop();

			// invokes write error callback
			if (onWriteDone)
				onWriteDone(clientLifeKeeper, error);

			// disconnects
			close(error);

			return;
		}

		// pops the last written message
		outputMessageQ.pop();
		networkStatistics.messagesSent++;

		// moves on with next writes
		write_next_buffer(clientLifeKeeper);
	}

	void ReliableCommunicationClientX::write_next_buffer(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// not supposed to keep going / socket is gone
		if (stopRequested || !tcpClient)
		{

			// did we have any pending messages?
			const int pendingMessages = outputMessageQ.size();
			if (pendingMessages != 0)
			{
				// update statistics (we are going to drop this and other messages that were enqueued)
				networkStatistics.messagesDropped += pendingMessages;

				// clears the queue
				while (outputMessageQ.size() > 0)
					outputMessageQ.pop();
			}

			// invoke write error callback (use pending messages)
			if (onWriteDone)
			{
				if (stopRequested)
					onWriteDone(clientLifeKeeper, boost::asio::error::operation_aborted);
				else
					onWriteDone(clientLifeKeeper, boost::asio::error::not_connected);
			}

			return;
		}

		// nothing to write? -> done with asynchronous writings
		if (outputMessageQ.size() == 0)
			return;

		// something to write? let's get it!
		NetworkBufferPtr msgBuffer = outputMessageQ.back();

		// starts writing for this client
		boost::asio::async_write(*tcpClient, boost::asio::buffer(msgBuffer.Data(), msgBuffer.Size()), std::bind(&ReliableCommunicationClientX::write_done, this, clientLifeKeeper, msgBuffer, _1, _2));
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
				boost::asio::post(io_context, std::bind(onReadCallback, shared_from_this(), boost::asio::error::operation_aborted);

			return;
		}

		// there is not even a connection here
		if (!tcpClient)
		{
			if (onReadCallback)
				boost::asio::post(io_context, std::bind(onReadCallback, shared_from_this(), boost::asio::error::not_connected);

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


	void ReliableCommunicationClientX::connect_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper, const boost::system::error_code& error,
		const boost::asio::ip::tcp::endpoint& endpoint)
	{
		// did we connect?
		boost::system::error_code errorToReport = error;
		if (!errorToReport)
		{
			if (stopRequested || !tcpClient)
			{
				errorToReport = comms::error::Cancelled;
			} else {
				// updates statistics
				networkStatistics.connected();
				updateConnectionDetails();
			}
		}

		if (onConnectDone)
		{
			onConnectDone(clientLifeKeeper, errorToReport);
		}

	}


	void ReliableCommunicationClientX::connect_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		const boost::system::error_code& error)
	{
		// we don't need to timeout when a stop was requested or the tcpClient was never set in first place
		// (both conditions will lead to an immediate error from the socket)
		if (stopRequested || !tcpClient)
			return;

		// operation_aborted means that a timer was rescheduled. We don't care about those scenarios
		// hence, if this timer wasn't aborted
		if (error != boost::asio::error::operation_aborted)
		{
			// cancel any pending operations
			boost::system::error_code e;
			tcpClient->shutdown(boost::asio::ip::tcp::socket::shutdown_both, e);

			// let the user know that something went wrong
			if (onConnectDone)
				onConnectDone(clientLifeKeeper, comms::error::TimedOut);

			// cleans up
			close();
		}
	}

}