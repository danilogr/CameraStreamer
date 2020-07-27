//
// Warning: This is WIP. Not currently in use by our application
//

#pragma once


/**
	PacketWriter classes are responsible for creating a network packet
	given a specific protocol implementation
*/
class PacketWriter
{

protected:

	// true if designed for a stream protocol such as TCP
	bool streamProtocol;

	// maximum transmission unit (todo: rtp, udp)
	//int MTU;

public:

	bool isStreamProtocol() { return streamProtocol; }



};