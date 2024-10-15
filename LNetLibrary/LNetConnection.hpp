#ifndef LNET_CONNECTION_HEADER
#define LNET_CONNECTION_HEADER

#include <memory>
#include "LNetTypes.hpp"
#include "LNetMessage.hpp"

namespace lnet
{
	struct Connection
	{
	public:
		TCPSocket tcpSocket;
		UDPSocket udpSocket;
		UDPEndpoint udpEndpoint;

		LNet4Byte connectionID;
	};
}
#endif