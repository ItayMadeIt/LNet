#ifndef LNET_TCP_HPP
#define LNET_TCP_HPP

#include <asio.hpp>
#include <vector>
#include "LNetEndianHandler.hpp"
#include "LNetTypes.hpp"
#include "LNetMessage.hpp"

namespace lnet
{
	class TCP
	{
	public:
		static void asyncSend(std::shared_ptr<TCPSocket> socket,
			std::function<void(std::shared_ptr<TCPSocket>, std::shared_ptr<Message>, const asio::error_code&)> callback,
			std::shared_ptr<Message>& msg)
		{
			// make it into buffers
			std::vector<asio::const_buffer> sendBuffer = msg->toConstBuffers();

			// send
			asio::async_write(*socket, sendBuffer,
				[callback, socket, msg](const asio::error_code ec, size_t size)
				{
					// try call callback
					if (callback) callback(socket, msg, ec);
				}
			);
		}

		static void asyncSend(TCPSocket& socket,
			std::function<void(TCPSocket& socket, std::shared_ptr<Message>, const asio::error_code&)> callback,
			std::shared_ptr<Message>& msg)
		{
			// make it into buffers
			std::vector<asio::const_buffer> sendBuffer = msg->toConstBuffers();

			// send
			asio::async_write(socket, sendBuffer,
				[callback, &socket, msg](const asio::error_code ec, size_t size)
				{
					// try call callback
					if (callback) callback(socket, msg, ec);
				}
			);
		}


		static void asyncRead(std::shared_ptr<TCPSocket> socket,
			std::function<void(std::shared_ptr<TCPSocket>, std::shared_ptr<Message>, const asio::error_code&)> callback) 
		{
			auto msg = std::make_shared<Message>();

			// First catch the header 
			asio::mutable_buffer headerBuffer = asio::buffer(reinterpret_cast<LNetByte*>(&msg->getHeader()), LNET_HEADER_SIZE);

			asio::async_read(*socket, headerBuffer,
				[socket, msg, callback](const asio::error_code& ec, std::size_t size)
				{
					if (ec)
					{
						if (callback) callback(socket, msg, ec);

						return;
					}

					// Make sure right endian structure
					msg->setMsgType(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgType()));
					msg->setMsgSize(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgSize()));

					// make buffers
					std::size_t payloadSize = msg->getMsgSize() - LNET_HEADER_SIZE;
					asio::mutable_buffer payloadBuffer = asio::buffer(msg->getPayload().data(), payloadSize);

					// read synchronously
					asio::error_code syncEc;
					asio::read(*socket, payloadBuffer, syncEc);

					if (callback) callback(socket, msg, syncEc);
				}
			);
		}
		
		static void asyncRead(TCPSocket& socket,
			std::function<void(TCPSocket&, std::shared_ptr<Message>, const asio::error_code&)> callback)
		{
			auto msg = std::make_shared<Message>();

			// First catch the header 
			asio::mutable_buffer headerBuffer = asio::buffer(reinterpret_cast<LNetByte*>(&msg->getHeader()), LNET_HEADER_SIZE);

			asio::async_read(socket, headerBuffer,
				[&, msg, callback](const asio::error_code& ec, std::size_t size)
				{
					if (ec)
					{
						if (callback) callback(socket, msg, ec);
						
						return;
					}

					// Make sure right endian structure
					msg->setMsgType(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgType()));
					msg->setMsgSize(LNetEndiannessHandler::fromNetworkEndian(msg->getMsgSize()));

					// make buffers
					std::size_t payloadSize = msg->getMsgSize() - LNET_HEADER_SIZE;
					asio::mutable_buffer payloadBuffer = asio::buffer(msg->getPayload().data(), payloadSize);

					// read synchronously
					asio::error_code syncEc;
					asio::read(socket, payloadBuffer, syncEc);

					if (callback) callback(socket, msg, syncEc);
				}
			);
		}
	};
}

#endif
