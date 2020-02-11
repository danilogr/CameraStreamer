#pragma once

#include <chrono>

struct Statistics
{
	std::chrono::time_point<std::chrono::system_clock> connected;
	unsigned int packetsSent;
	unsigned int packetsDropped;
	unsigned long long bytesSent;

	Statistics() : connected(std::chrono::system_clock::now()), packetsSent(0), packetsDropped(0), bytesSent(0) {}
};