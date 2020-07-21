#include "ReliableCommunicationClientX.h"


// called when done writing to client
void ReliableCommunicationClientX::write_done(std::shared_ptr<ReliableCommunicationClientX> client,
	NetworkBufferPtr buffer, const boost::system::error_code& error, std::size_t bytes_transferred)
{

}

void ReliableCommunicationClientX::write_next_message(std::shared_ptr<ReliableCommunicationClientX> client)
{

}

// start reading for the client
void ReliableCommunicationClientX::read_header_async(std::shared_ptr<ReliableCommunicationClientX> client)
{

}

void ReliableCommunicationClientX::read_message_async(std::shared_ptr<ReliableCommunicationClientX> client,
	const boost::system::error_code& error, std::size_t bytes_transferred)
{

}

void ReliableCommunicationClientX::read_message_done(std::shared_ptr<ReliableCommunicationClientX> client,
	NetworkBufferPtr buffer, const boost::system::error_code& error, std::size_t bytes_transferred)
{

}

void ReliableCommunicationClientX::write_request(std::shared_ptr<ReliableCommunicationClientX> client,
	NetworkBufferPtr message)
{

}