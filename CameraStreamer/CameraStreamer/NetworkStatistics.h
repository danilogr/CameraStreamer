#pragma once

#include <chrono>
#include <string>

struct NetworkStatistics
{
	// number of packets received and sent on the long run
	std::chrono::time_point<std::chrono::system_clock> connectedTime, disconnectedTime;

	// remote address info
	std::string remoteAddress;
		int remotePort;

	// local address info
	std::string localAddress;
	int localPort;

	// if true, it means that this structure represents the statistics of 
	// an incoming connection  (and not an outgoing connection)
	bool incomingConnection;
	// number of packets received and sent during this last session
	unsigned long long messagesSent;	  // messages is protocol dependent (and it doesn't mean network packets.)
	unsigned long long messagesDropped;   // messages dropped due to timeouts, disconnects
	unsigned long long bytesSent;

	unsigned long long messagesReceived;
	unsigned long long bytesReceived;

	NetworkStatistics(bool incoming = false) : connectedTime(std::chrono::system_clock::now()),
		messagesSent(0), messagesDropped(0), bytesSent(0), messagesReceived(0),
		bytesReceived(0), remotePort(0), localPort(0), incomingConnection(incoming),
		currentlyConnected(incoming) {}

	void reset(bool currentlyconnected = false)
	{
		connectedTime = std::chrono::system_clock::now();
		currentlyConnected = currentlyconnected;
		messagesSent = 0;
		messagesDropped = 0;
		bytesSent = 0;
		messagesReceived = 0;
		bytesReceived = 0;
		remotePort = 0;
		localPort = 0;
		incomingConnection = false;
	}

	void connected()
	{
		reset(true);
	}

	void disconnected()
	{
		disconnectedTime = std::chrono::system_clock::now();
		currentlyConnected = false;
	}

	inline long long durationInSeconds()
	{
		if (currentlyConnected)
			return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - connectedTime).count();
		else
			return std::chrono::duration_cast<std::chrono::seconds>(disconnectedTime - connectedTime).count();
	}

private:
	bool currentlyConnected;

};