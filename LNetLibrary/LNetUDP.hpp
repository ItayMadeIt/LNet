#ifndef LNET_UDP_HPP
#define LNET_UDP_HPP

#include <asio.hpp>
#include <vector>
#include "LNetEndianHandler.hpp"
#include "LNetMessage.hpp"

namespace lnet
{
	class UDP
	{
	public:
		static void asyncSend(std::shared_ptr<UDPSocket> socket, UDPEndpoint& ep,
			std::function<void(std::shared_ptr<UDPSocket>, std::shared_ptr<Message>, const asio::error_code&)> callback,
			std::shared_ptr<Message> msg)
		{
			// make it into buffers
			std::vector<asio::const_buffer> sendBuffer = msg->toConstBuffers();

			// send
			socket->async_send_to(sendBuffer, ep,
				[callback, socket, msg](const asio::error_code ec, size_t size)
				{
					// try call callback
					if (callback) callback(socket, msg, ec);
				}
			);
		}

		static void asyncSend(UDPSocket& socket, UDPEndpoint& ep,
			std::function<void(UDPSocket&, std::shared_ptr<Message>, const asio::error_code&)> callback,
			std::shared_ptr<Message> msg)
		{
			// make it into buffers
			std::vector<asio::const_buffer> sendBuffer = msg->toConstBuffers();

			// send
			socket.async_send_to(sendBuffer, ep,
				[callback, &socket, msg](const asio::error_code ec, size_t size)
				{
					// try call callback
					if (callback) callback(socket, msg, ec);
				}
			);
		}


		static void asyncRead(std::shared_ptr<UDPSocket> socket, 
			std::function<void(std::shared_ptr<UDPSocket>, UDPEndpoint&, std::shared_ptr<Message>, const asio::error_code&)> callback)
		{
			auto msg = std::make_shared<Message>();

			auto endpoint = std::make_shared<UDPEndpoint>();

			// First catch the header 
			asio::mutable_buffer headerBuffer = asio::buffer(reinterpret_cast<LNetByte*>(&msg->getHeader()), LNET_HEADER_SIZE);

			// First read we use async
			socket->async_receive_from(headerBuffer, *endpoint,
				[socket, msg, endpoint, callback](const asio::error_code& ec, std::size_t size)
				{

					// call callback
					if (ec)
					{
						if (callback) callback(socket, *endpoint, msg, ec);
						return;
					}

					if (size != LNET_HEADER_SIZE)
					{
						throw std::runtime_error("Size of read was not enough to fit the header!\n");
					}

					// Make sure right endian structure
					msg->setMsgType(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgType()));
					msg->setMsgSize(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgSize()));
					
					// get payload buffer
					std::size_t payloadSize = msg->getMsgSize() - LNET_HEADER_SIZE;
					asio::mutable_buffer payloadBuffer = asio::buffer(msg->getPayload().data(), payloadSize);

					// second read uses sync read
					asio::error_code syncEc;
					socket->receive_from(payloadBuffer, *endpoint, 0, syncEc);

					// call callback
					if (callback) callback(socket, *endpoint, msg, ec);
				}
			);
		}

		static void asyncRead(UDPSocket& socket,
			std::function<void(UDPSocket&, UDPEndpoint&, std::shared_ptr<Message>, const asio::error_code&)> callback)
		{
			auto msg = std::make_shared<Message>();

			auto endpoint = std::make_shared<UDPEndpoint>();

			// First catch the header 
			asio::mutable_buffer headerBuffer = asio::buffer(reinterpret_cast<LNetByte*>(&msg->getHeader()), LNET_HEADER_SIZE);

			// First read we use async
			socket.async_receive_from(headerBuffer, *endpoint,
				[&socket, msg, endpoint, callback](const asio::error_code& ec, std::size_t size)
				{
					// call callback
					if (ec)
					{
						if (callback) callback(socket, *endpoint, msg, ec);

						return;
					}
					
					// Make sure right endian structure
					msg->setMsgType(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgType()));
					msg->setMsgSize(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgSize()));

					// get payload buffer
					std::size_t payloadSize = msg->getMsgSize() - LNET_HEADER_SIZE;
					asio::mutable_buffer payloadBuffer = asio::buffer(msg->getPayload().data(), payloadSize);


					// second read uses sync read
					asio::error_code syncEc;
					socket.receive_from(payloadBuffer, *endpoint, 0, syncEc);

					// call callback
					if (callback) callback(socket, *endpoint, msg, ec);

				}
			);
		}


	};
}

#endif
