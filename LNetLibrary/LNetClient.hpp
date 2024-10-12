#ifndef LNET_CLIENT_HPP
#define LNET_CLIENT_HPP

#include <asio.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include "LnetMessage.hpp"

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

		std::shared_ptr<asio::ip::tcp::socket> socket;

		std::atomic<bool> isConnected;

		// Callback functions for each action
		std::function<void(Client*, const asio::error_code ec)> connectedCallback;
		std::function<void(Client*, std::shared_ptr<Message>, const asio::error_code ec)> readCallback;
		std::function<void(Client*, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback;

		// Read msg to callback
		std::unordered_map<LNet4Byte, ClientMsgCallback> msgCallbacks;

		void handleConnection()
		{
			onConnect();

			asyncReadMessageTCP(socket,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& err) {
					repeatRead(sock, msg, err);
				}
			);
		}

		virtual void onConnect()
		{
			isConnected = true;
		}

		virtual void onRead(std::shared_ptr<Message> msg, const asio::error_code& ec)
		{
			// call callback based on type
			if (!ec)
			{
				auto it = msgCallbacks.find(msg->getMsgType());

				if (it != msgCallbacks.end())
				{
					it->second(this, msg);
				}
			}

			if (readCallback)
			{
				readCallback(this, msg, ec);
			}
		}

		void repeatRead(std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> readMsg, const asio::error_code& ec)
		{
			onRead(readMsg, ec);

			asyncReadMessageTCP(socket,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& err) {
					repeatRead(sock, msg, err);
				}
			);
		}

	public:

		Client(std::string serverIp, unsigned short port, size_t threadsAmount = 2,
			std::function<void(Client*, const asio::error_code ec)> connectedCallback = nullptr,
			std::function<void(Client*, std::shared_ptr<Message>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(Client*, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback = nullptr) :
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

		bool getIsConnected()
		{
			return isConnected;
		}

		void connect()
		{
			socket->async_connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(serverIp), port),
				[this](const asio::error_code& ec)
				{
					if (!ec)
					{
						isConnected = true;
						handleConnection();
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

			asyncWriteMessageTCP(socket, msg,
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