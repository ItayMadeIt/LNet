#ifndef LNET_CLIENT_HPP
#define LNET_CLIENT_HPP

#include <asio.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include "LNetMessage.hpp"
#include "LNetTCP.hpp"
#include "LNetUDP.hpp"

namespace lnet
{
	class Client;

	using ClientMsgCallback = std::function<void(Client* client, std::shared_ptr<Message>)>;

	class Client
	{
	protected:
		asio::io_context ioContext;
		std::vector<std::unique_ptr<std::thread>> threads;

		std::string serverIp;
		unsigned short port;

		std::shared_ptr<TCPSocket> tcpSocket;
		std::shared_ptr<UDPSocket> udpSocket;

		std::atomic<bool> isConnected;

		// Callback functions for each action
		std::function<void(Client*, const asio::error_code ec)> connectedCallback;
		// The client instance, is reliable bool, the message pointer, an error code
		std::function<void(Client*, bool, std::shared_ptr<Message>, const asio::error_code ec)> readCallback;
		// The client instance, is reliable bool, the message pointer, an error code
		std::function<void(Client*, bool, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback;

		// Read msg to callback
		std::unordered_map<LNet4Byte, ClientMsgCallback> msgCallbacks;

		void handleConnection()
		{
			isConnected = true;

			// Try bind it to have the same values as the TCP socket
			try
			{
				udpSocket->bind(
					UDPEndpoint(tcpSocket->local_endpoint().address(), tcpSocket->local_endpoint().port())
				);
			}
			catch (const std::exception& e)
			{
				// If failed, bind to any available port
				udpSocket->bind(
					UDPEndpoint(tcpSocket->local_endpoint().address(), 0)
				);
			}

			onConnect();

			repeatReadTCP(tcpSocket);
			repeatReadUDP(udpSocket);
		}

		virtual void onConnect()
		{

		}

		virtual void onRead(bool isReliable, std::shared_ptr<Message> msg, const asio::error_code& ec)
		{
			// call callback based on type
			if (!ec)
			{
				auto it = msgCallbacks.find(msg->getMsgType());

				if (it != msgCallbacks.end()) it->second(this, msg);
			}

			if (readCallback) readCallback(this, isReliable, msg, ec);
		}

		void repeatReadTCP(std::shared_ptr<TCPSocket> sock)
		{
			TCP::asyncRead(sock,
				[this](std::shared_ptr<TCPSocket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec) 
				{
					onRead(true, msg, ec);

					repeatReadTCP(sock);
				}
			);
		}

		void repeatReadUDP(std::shared_ptr<UDPSocket> sock)
		{
			UDP::asyncRead(sock,
				[this](std::shared_ptr<UDPSocket> sock, UDPEndpoint& ep, std::shared_ptr<Message> msg, const asio::error_code& ec)
				{
					// endpoint does nothing, we are a client so the server sent us the message

					onRead(false, msg, ec);

					repeatReadUDP(sock);
				}
			);
		}

	public:

		Client(std::string serverIp, unsigned short port, size_t threadsAmount = 2,
			std::function<void(Client*, const asio::error_code ec)> connectedCallback = nullptr,
			std::function<void(Client*, bool, std::shared_ptr<Message>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(Client*, bool, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback = nullptr) :
			serverIp(serverIp), port(port), isConnected(false),
			tcpSocket(std::make_shared<TCPSocket>(ioContext)),
			connectedCallback(connectedCallback), readCallback(readCallback), writeCallback(writeCallback)
		{
			for (size_t i = 0; i < threadsAmount; i++)
			{
				threads.push_back(
					std::make_unique<std::thread>(
						[this]()
						{
							ioContext.run();
						}
				));
			}
		}

		bool getIsConnected()
		{
			return isConnected;
		}

		void connect()
		{
			tcpSocket->async_connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(serverIp), port),
				[this](const asio::error_code& ec)
				{
					if (!ec)
					{
						handleConnection();


					}
					if (connectedCallback)
					{
						connectedCallback(this, ec);
					}
				}
			);
		}

		void sendTCP(std::shared_ptr<Message> msg)
		{
			TCP::asyncSend(tcpSocket,
				[this](std::shared_ptr<TCPSocket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
				{
					if (writeCallback)
						writeCallback(this, true, msg, ec);
				},
				msg
			);
		}

		template<typename... T>
		void sendTCP(LNet4Byte type, T... params)
		{
			TCP::asyncSend(tcpSocket,
				[this](std::shared_ptr<TCPSocket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
				{
					if (writeCallback)
						writeCallback(this, true, msg, ec);
				},
				type, std::forward<T>(params)...
			);
		}

		/* UDP DOESNT WORK YET
		void sendUDP(std::shared_ptr<Message> msg)
		{
			UDP::asyncSend(udpSocket, UDPEndpoint()
				UDPEndpoint(tcpSocket->remote_endpoint().address(), tcpSocket->remote_endpoint().port()),
				[this](std::shared_ptr<TCPSocket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
				{
					if (writeCallback)
						writeCallback(this, true, msg, ec);
				},
				msg
			);
		}

		template<typename... T>
		void sendUDP(LNet4Byte type, T... params)
		{
			TCP::asyncSend(tcpSocket,
				[this](std::shared_ptr<TCPSocket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
				{
					if (writeCallback)
						writeCallback(this, true, msg, ec);
				},
				type, std::forward<T>(params)...
			);
		}*/

		void addMsgListener(LNet4Byte type, ClientMsgCallback callback)
		{
			msgCallbacks[type] = callback;
		}

		void disconnect()
		{
			isConnected = false;

			ioContext.stop();

			for (size_t i = 0; i < threads.size(); i++)
			{
				if (threads[i]->joinable()) threads[i]->join();
			}

		}
	};
}
#endif