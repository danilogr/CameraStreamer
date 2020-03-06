#include "RemoteControlServer.h"


RemoteClient::RemoteClient(RemoteControlServer& server, std::shared_ptr<tcp::socket> connection) : server(server)
{
	// access socket
	socket = connection;

	// is this a valid connection
	if (socket)
	{
		remoteAddress = socket->remote_endpoint().address().to_string();
		remotePort = socket->remote_endpoint().port();
	}

	Logger::Log("Remote") << "New client connected: " << remoteAddress << ':' << remotePort << std::endl;

}

RemoteClient::~RemoteClient()
{
	// disconnects the socket if it is still connected (this should not happen here)
	if (socket && socket->is_open())
	{
		socket->close();
	}

	Logger::Log("Remote") << "Client " << remoteAddress << ':' << remotePort << " disconnected" << std::endl;
	Logger::Log("Remote") << "[Stats] Sent client " << remoteAddress << ':' << remotePort << " --> "
		<< statistics.bytesSent << " bytes (" << statistics.packetsSent << " messages sent and " << statistics.packetsDropped << " dropped) -"
		<< " Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - statistics.connected).count() / 1000.0f << " sec" << std::endl;
}

void RemoteClient::close()
{
	// if we still have an ongoing socket...
	if (socket)
	{
		if (socket->is_open())
		{
			// closes the connection
			socket->close();

			// frees the socket so that we do not need to worry about it anymore
			socket = nullptr;
		}

		// if io_service is not running, we need to make sure our statistics are correct
		if (server.io_service.stopped())
		{
			// add a new count of packets dropped
			statistics.packetsDropped += outputMessageQ.size();
		}
	}
}

bool RemoteClient::send(std::shared_ptr<std::vector<uchar> > message)
{
	if (socket)
	{
		// thread is not running anymore...
		if (server.io_service.stopped())
			return false;

		// makes sure that we are running from the right thread before continuing
		if (std::this_thread::get_id() != server.sThread->get_id())
		{
			server.io_service.post(std::bind(&RemoteClient::write_request, shared_from_this(), message));
			return true;
		}
	}
	else {
		Logger::Log("Remote") << "Error sending message! Client " << remoteAddress << ':' << remotePort << " is not connected" << std::endl;
	}

	return true;
}

// ======================================= static methods =========================================== //

void RemoteClient::write_done(std::shared_ptr<RemoteClient> client, std::shared_ptr < std::vector<uchar> > buffer,
	const boost::system::error_code& error, std::size_t bytes_transferred)
{
	// updates the number of bytes sent
	client->statistics.bytesSent += bytes_transferred;

	// there's nothing much we can do here besides remove the client if we get an error sending a message to it
	if (error || !client->socket)
	{
		// update statistics (we are going to drop this and other messages that were enqueued)
		client->statistics.packetsDropped += client->outputMessageQ.size() + 1; // accounts for this message

		// clears the queue
		while (client->outputMessageQ.size() > 0)
			client->outputMessageQ.pop();

		// remove itself from the server
		client->server.DisconnectClient(client);

		return;
	}

	// pops the last read
	client->outputMessageQ.pop();
	client->statistics.packetsSent++;
	
	// moves on with next writes
	write_next_message(client);
}

void RemoteClient::write_next_message(std::shared_ptr<RemoteClient> client)
{
	using namespace std::placeholders; // for  _1, _2, ...

	// can we still right / do we still have a socket?
	if (!client->socket)
		return;

	// nothing to write? -> done with asynchronous writings
	if (!client->outputMessageQ.size())
		return;

	// something to write? let's pop it!
	std::shared_ptr<std::vector<uchar > > message = client->outputMessageQ.back();

	// starts writing for this client
	boost::asio::async_write(*client->socket, boost::asio::buffer(*message, message->size()), std::bind(&RemoteClient::write_done, client, message, _1, _2));
}


// this method is called when someone wants to initiate sending a message to the client
void RemoteClient::write_request(std::shared_ptr<RemoteClient> client, std::shared_ptr < std::vector<uchar> > message)
{

	// is the client still connected?
	if (!client->socket)
	{
		++client->statistics.packetsDropped; // whoops
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
