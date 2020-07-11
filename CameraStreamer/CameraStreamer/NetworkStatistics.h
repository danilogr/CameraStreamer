#pragma once

#include <chrono>
#include <string>

struct NetworkStatistics
{

	// number of packets received and sent on the long run

	std::chrono::time_point<std::chrono::system_clock> connected;



	// remote address info
	std::string remoteAddress;
	int remotePort;

	// if true, it means that this structure represents the statistics of 
	// an incoming connection  (and not an outgoing connection)
	bool incomingConnection;
	// number of packets received and sent during this last session
	unsigned int messagesSent;		// messages is protocol dependent (and it doesn't mean network packets.)
	unsigned int messagesDropped;   // messages dropped due to timeouts
	unsigned long long bytesSent;

	unsigned int messagesReceived;
	unsigned long long bytesReceived;


	NetworkStatistics() : connected(std::chrono::system_clock::now()),
		messagesSent(0), messagesDropped(0), bytesSent(0), messagesReceived(0),
		bytesReceived(0), remotePort(0), incomingConnection(false) {}
};