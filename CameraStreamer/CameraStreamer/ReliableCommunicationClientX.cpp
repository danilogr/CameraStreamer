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
			if (onWrite)
				onWrite(clientLifeKeeper, error);

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
			if (onWrite)
			{
				if (stopRequested)
					onWrite(clientLifeKeeper, boost::asio::error::operation_aborted);
				else
					onWrite(clientLifeKeeper, boost::asio::error::not_connected);
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


	void ReliableCommunicationClientX::read_request(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		NetworkBufferPtr buffer, size_t bytes_requested)
	{
		using namespace std::placeholders; // for  _1, _2, ...

		// operation aborted
		if (stopRequested)	
		{
			readOperationPending = false;

			if (onRead)
				onRead(clientLifeKeeper, boost::asio::error::operation_aborted);

			return;
		}

		// there is not even a connection here
		if (!tcpClient)
		{
			readOperationPending = false;

			if (onRead)
				onRead(clientLifeKeeper, boost::asio::error::not_connected);

			return;
		}

		// reads the entire message
		boost::asio::async_read(*tcpClient, boost::asio::buffer(buffer.Data(), bytes_requested),
			boost::asio::transfer_exactly(bytes_requested), std::bind(&ReliableCommunicationClientX::read_request_done, this, clientLifeKeeper, buffer, bytes_requested, _1, _2));
	}

	void ReliableCommunicationClientX::read_request_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
		NetworkBufferPtr buffer, size_t bytes_requested, const boost::system::error_code& error, std::size_t bytes_transferred)
	{

		// done reading. Allow user to read more
		readOperationPending = false;

		// saves the amount of bytes read
		networkStatistics.bytesReceived += bytes_transferred;

		// any problems?
		if (error || bytes_transferred != bytes_requested)
		{
			// invoke read callback with error
			if (onRead)
				onRead(clientLifeKeeper, error);

			// disconnects
			close(error);

			return;
		}

		// everything went well. this counts as a message
		++networkStatistics.messagesReceived;

		// invoke read callback to inform the user that we completed (error should be 0)
		if (onRead)
			onRead(clientLifeKeeper, boost::system::error_code());

	}

	void ReliableCommunicationClientX::read_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
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
			// oh no, time out
			if (onRead)
				onRead(clientLifeKeeper, comms::error::TimedOut);

			// closes the connection because user requested time out
			close(comms::error::TimedOut);
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
			tcpClient->close();

			// let the user know that something went wrong
			if (onConnected)
				onConnected(clientLifeKeeper, comms::error::TimedOut);

			// cleans up
			close();
		}
	}

}