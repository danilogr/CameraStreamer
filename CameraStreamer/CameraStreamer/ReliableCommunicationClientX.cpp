#include "ReliableCommunicationClientX.h"


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

		// connected still? time to disconnect
		close();

		// todo: invoke write error callback

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

		// todo: invoke write error callback (use pending messages)

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

	// can't keep reading if not connected
	if (stopRequested || !tcpClient)
	{
		// todo: invoked read error callback
		return;
	}

	// reads the entire message
	boost::asio::async_read(*tcpClient, boost::asio::buffer(buffer.Data(), bytes_requested),
		boost::asio::transfer_exactly(bytes_requested), std::bind(&ReliableCommunicationClientX::read_request_done, this, clientLifeKeeper, buffer, bytes_requested, _1, _2));
}

void ReliableCommunicationClientX::read_request_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
	NetworkBufferPtr buffer, size_t bytes_requested, const boost::system::error_code& error, std::size_t bytes_transferred)
{
	using namespace std::placeholders; // for  _1, _2, ...

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

		// remove itself from the server
		close();
		return;
	}

	// everything went well. this counts as a message
	++networkStatistics.messagesReceived;

	// invoke read callback to inform the user that we completed (error should be 0)
	if (onRead)
		onRead(clientLifeKeeper, error);

}

void ReliableCommunicationClientX::read_timeout_done(std::shared_ptr<ReliableCommunicationClientX> clientLifeKeeper,
	const boost::system::error_code& error)
{
	// operation_aborted means that a timer was rescheduled. We don't care about those scenarios
	// hence, if this timer wasn't aborted
	if (error != boost::asio::error::operation_aborted)
	{
		// oh no, time out

		// (todo) invoke read timeout callback

		// (todo) if true, close connection
		close();
	}
}