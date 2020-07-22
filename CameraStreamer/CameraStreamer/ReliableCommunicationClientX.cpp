#include "ReliableCommunicationClientX.h"


// called when done writing to client
void ReliableCommunicationClientX::write_done(std::shared_ptr<ReliableCommunicationClientX> client,
	NetworkBufferPtr buffer, const boost::system::error_code& error, std::size_t bytes_transferred)
{
	// updates the number of bytes sent
	client->networkStatistics.bytesSent += bytes_transferred;

	// there's nothing much we can do here besides remove the client if we get an error sending a message to it
	if (error)
	{
		// update statistics (we are going to drop this and other messages that were enqueued)
		client->networkStatistics.messagesDropped += client->outputMessageQ.size() + 1; // accounts for this message

		// clears the queue
		while (client->outputMessageQ.size() > 0)
			client->outputMessageQ.pop();

		// connected still? time to disconnect
		client->close();

		return;
	}

	// pops the last written message
	client->outputMessageQ.pop();

	client->networkStatistics.messagesSent++;


	// moves on with next writes
	write_next_message(client);
}

void ReliableCommunicationClientX::write_next_message(std::shared_ptr<ReliableCommunicationClientX> client)
{
	using namespace std::placeholders; // for  _1, _2, ...

	// not supposed to keep going / socket is gone
	if (client->stopRequested || !client->tcpClient)
	{
		// did we have any pending messages?
		if (client->outputMessageQ.size() != 0)
		{
			// update statistics (we are going to drop this and other messages that were enqueued)
			client->networkStatistics.messagesDropped += client->outputMessageQ.size();

			// clears the queue
			while (client->outputMessageQ.size() > 0)
				client->outputMessageQ.pop();
		}
		return;
	}

	// nothing to write? -> done with asynchronous writings
	if (client->outputMessageQ.size() == 0)
		return;

	// something to write? let's get it!
	NetworkBufferPtr msgBuffer = client->outputMessageQ.back();

	// starts writing for this client
	boost::asio::async_write(*client->tcpClient, boost::asio::buffer(msgBuffer.Data(), msgBuffer.Size()), std::bind(&ReliableCommunicationClientX::write_done, client, msgBuffer, _1, _2));
}

// start reading for the client
void ReliableCommunicationClientX::read_header_async(std::shared_ptr<ReliableCommunicationClientX> client)
{

}

void ReliableCommunicationClientX::read_message_async(std::shared_ptr<ReliableCommunicationClientX> client,
	NetworkBufferPtr buffer, size_t bytes_requested)
{
	using namespace std::placeholders; // for  _1, _2, ...

	// can't keep reading if disconnected
	if (!client->tcpClient || client->stopRequested)
	{
		return;
	}

	// reads the entire message
	boost::asio::async_read(*client->tcpClient, boost::asio::buffer(buffer.Data(), bytes_requested),
		boost::asio::transfer_exactly(bytes_requested), std::bind(&ReliableCommunicationClientX::read_message_done, client, buffer, bytes_requested, _1, _2));
}

void ReliableCommunicationClientX::read_message_done(std::shared_ptr<ReliableCommunicationClientX> client,
	NetworkBufferPtr buffer, size_t bytes_requested, const boost::system::error_code& error, std::size_t bytes_transferred)
{
	using namespace std::placeholders; // for  _1, _2, ...

	// saves the amount of bytes read
	client->networkStatistics.bytesReceived += bytes_transferred;

	// any problems?
	if (error)
	{
		// remove itself from the server
		client->close();
		return;
	}

	// did we read the right amount? (sanit check)
	if (bytes_transferred != bytes_requested)
	{
		// disconnect client
		client->close();
		return;
	}

	// everything went well. this counts as a message
	++client->networkStatistics.messagesReceived;

	// invoke read callback
	

	// restarts the process
	read_header_async(client);
}

void ReliableCommunicationClientX::write_request(std::shared_ptr<ReliableCommunicationClientX> client,
	NetworkBufferPtr message)
{
	// is the client still connected?
	if (client->stopRequested || client->tcpClient)
	{
		++client->networkStatistics.messagesDropped; // whoops
		return;
	}


	// adds message to client Q
	client->outputMessageQ.push(message);

	// if this is the first message, then there is no pending messages so
	// we can start writing to clients
	if (client->outputMessageQ.size() == 1)
	{
		// only message? let's send it
		write_next_message(client);
	}
}