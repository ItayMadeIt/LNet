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
		Connection(asio::io_context& context) : tcpSocket(context), udpSocket(context)
		{ }

		void connect(std::string serverIp, unsigned short port,
			std::function<void(const asio::error_code& ec)> callback)
		{
			asio::error_code ec;
			tcpSocket.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(serverIp), port), ec);
			
			callback(ec);
			 
			if (ec) return;

			std::string address = "127.0.0.1";
			unsigned short localPort = (unsigned short)tcpSocket.local_endpoint().port();
			asio::error_code _ec;

			//UDPEndpoint ep(asio::ip::address::from_string(address), localPort);

			//udpSocket.bind(ep, _ec);
			//std::cout << "Error: " << _ec.message() << '\n';

			//udpRemoteEndpoint = UDPEndpoint(tcpSocket.remote_endpoint().address(), tcpSocket.remote_endpoint().port());
		}

		~Connection()
		{
			tcpSocket.close();
			udpSocket.close();
		}

		TCPSocket tcpSocket;
		UDPSocket udpSocket;
		UDPEndpoint udpRemoteEndpoint;
	};
}
#endif