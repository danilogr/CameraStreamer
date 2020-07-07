#pragma once

#include <chrono>
#include <string>

struct Statistics
{
	std::chrono::time_point<std::chrono::system_clock> connected;
	unsigned int packetsSent;
	unsigned int packetsDropped;
	unsigned long long bytesSent;

	unsigned int packetsReceived;
	unsigned long long bytesReceived;

	std::string remoteAddress;
	int remotePort;

	Statistics() : connected(std::chrono::system_clock::now()), packetsSent(0), packetsDropped(0), bytesSent(0), packetsReceived(0), bytesReceived(0), remotePort(0) {}
};