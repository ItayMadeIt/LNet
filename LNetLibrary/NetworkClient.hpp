#ifndef NETWORK_CLIENT_HPP
#define NETWORK_CLIENT_HPP

#include <asio.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include "NetworkMessage.hpp"

namespace lnet
{
	class NetworkClient;

	using ClientMsgCallback = std::function<void(NetworkClient* client, std::shared_ptr<Message>)>;

	class NetworkClient
	{
	private:
		asio::io_context ioContext;
		std::vector<std::unique_ptr<std::thread>> threads;

		std::string serverIp;
		unsigned short port;

		std::shared_ptr<asio::ip::tcp::socket> socket;

		std::atomic<bool> isConnected;

		// Callback functions for each action
		std::function<void(NetworkClient*, const asio::error_code ec)> connectedCallback;
		std::function<void(NetworkClient*, std::shared_ptr<Message>, const asio::error_code ec)> readCallback;
		std::function<void(NetworkClient*, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback;

		// Read msg to callback
		std::unordered_map<LNet4Byte, ClientMsgCallback> msgCallbacks;

		void onConnectSuccesfully()
		{
			isConnected = true;

			asyncReadMessage(socket,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& err) {
					repeatRead(sock, msg, err);
				}
			);
		}

		void repeatRead(std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> readMsg, const asio::error_code& ec)
		{
			// call callback based on type
			if (!ec)
			{
				auto it = msgCallbacks.find(readMsg->getMsgType());

				if (it != msgCallbacks.end())
				{
					it->second(this, readMsg);
				}
			}

			if (readCallback)
			{
				readCallback(this, readMsg, ec);
			}

			asyncReadMessage(socket,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& err) {
					repeatRead(sock, msg, err);
				}
			);
		}

	public:

		NetworkClient(std::string serverIp, unsigned short port, size_t threadsAmount = 2,
			std::function<void(NetworkClient*, const asio::error_code ec)> connectedCallback = nullptr,
			std::function<void(NetworkClient*, std::shared_ptr<Message>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(NetworkClient*, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback = nullptr) :
			serverIp(serverIp), port(port), isConnected(false),
			socket(std::make_shared<asio::ip::tcp::socket>(ioContext)),
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

		void connect()
		{
			socket->async_connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(serverIp), port),
				[this](const asio::error_code& ec)
				{
					if (!ec)
					{
						onConnectSuccesfully();
					}
					if (connectedCallback)
					{
						connectedCallback(this, ec);
					}
				}
			);
		}

		void send(std::shared_ptr<Message> msg)
		{
			asio::error_code ec;

			asyncWriteMessage(socket, msg,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
				{
					if (writeCallback)
					{
						writeCallback(this, msg, ec);
					}
				}
			);
		}

		template<typename... T>
		void send(LNet4Byte type, T... params)
		{
			auto msg = std::make_shared<Message>(type);

			(void(msg->operator<<(params)), ...);

			send(msg);
		}

		void addMsgListener(LNet4Byte type, ClientMsgCallback callback)
		{
			msgCallbacks[type] = callback;
		}

		void disconnect()
		{
			ioContext.stop();

			for (size_t i = 0; i < threads.size(); i++)
			{
				if (threads[i]->joinable()) threads[i]->join();
			}

		}
	};
}
#endif