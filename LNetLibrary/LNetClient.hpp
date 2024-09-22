#ifndef LNET_CLIENT_HPP
#define LNET_CLIENT_HPP

#include <asio.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include "LnetMessage.hpp"

namespace lnet
{
	class LNetClient;

	using ClientMsgCallback = std::function<void(LNetClient* client, std::shared_ptr<LNetMessage>)>;

	class LNetClient
	{
	protected:
		asio::io_context ioContext;
		std::vector<std::unique_ptr<std::thread>> threads;

		std::string serverIp;
		unsigned short port;

		std::shared_ptr<asio::ip::tcp::socket> socket;

		std::atomic<bool> isConnected;

		// Callback functions for each action
		std::function<void(LNetClient*, const asio::error_code ec)> connectedCallback;
		std::function<void(LNetClient*, std::shared_ptr<LNetMessage>, const asio::error_code ec)> readCallback;
		std::function<void(LNetClient*, std::shared_ptr<LNetMessage>, const asio::error_code ec)> writeCallback;

		// Read msg to callback
		std::unordered_map<LNet4Byte, ClientMsgCallback> msgCallbacks;

		void onConnectSuccesfully()
		{
			isConnected = true;

			asyncReadMessage(socket,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<LNetMessage> msg, const asio::error_code& err) {
					repeatRead(sock, msg, err);
				}
			);
		}

		void repeatRead(std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<LNetMessage> readMsg, const asio::error_code& ec)
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
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<LNetMessage> msg, const asio::error_code& err) {
					repeatRead(sock, msg, err);
				}
			);
		}

	public:

		LNetClient(std::string serverIp, unsigned short port, size_t threadsAmount = 2,
			std::function<void(LNetClient*, const asio::error_code ec)> connectedCallback = nullptr,
			std::function<void(LNetClient*, std::shared_ptr<LNetMessage>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(LNetClient*, std::shared_ptr<LNetMessage>, const asio::error_code ec)> writeCallback = nullptr) :
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
						onConnectSuccesfully();
					}
					if (connectedCallback)
					{
						connectedCallback(this, ec);
					}
				}
			);
		}

		void send(std::shared_ptr<LNetMessage> msg)
		{
			asio::error_code ec;

			asyncWriteMessage(socket, msg,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<LNetMessage> msg, const asio::error_code& ec)
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
			auto msg = std::make_shared<LNetMessage>(type);

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