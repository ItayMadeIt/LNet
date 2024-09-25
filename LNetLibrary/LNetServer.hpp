#ifndef LNET_SERVER_HPP
#define LNET_SERVER_HPP

#include <asio.hpp>
#include <string>
#include <memory>
#include <map>
#include <array>
#include <vector>
#include <atomic>
#include <unordered_map>
#include "LNetMessage.hpp"

namespace lnet
{
	template<size_t maxClients>
	class LNetServer;

	template<size_t maxClients>
	using ServerMsgCallback = std::function<void(LNetServer<maxClients>*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<lnet::LNetMessage>)>;

	template<size_t maxClients>
	class LNetServer
	{

	protected:
		std::vector<std::unique_ptr<std::thread>> threads;

		unsigned short port;
		size_t clientsAmount;
		asio::io_context ioContext;
		asio::executor_work_guard<asio::io_context::executor_type> workGuard;

		std::atomic<bool> isRunning = false;

		asio::ip::tcp::acceptor acceptor;
		std::array<std::shared_ptr<asio::ip::tcp::socket>, maxClients> clients;

		std::function<void(LNetServer*, std::shared_ptr<asio::ip::tcp::socket>, const asio::error_code& ec)> acceptCallback;
		std::function<void(LNetServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<lnet::LNetMessage>, const asio::error_code& ec)> readCallback;
		std::function<void(LNetServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<lnet::LNetMessage>, const asio::error_code& ec)> writeCallback;

		std::map<LNet4Byte, ServerMsgCallback<maxClients>> msgCallbacks;

		void initAccept()
		{
			auto clientSock = std::make_shared<asio::ip::tcp::socket>(ioContext);

			acceptor.async_accept(*clientSock,
				[this, clientSock](const asio::error_code& ec)
				{
					continueAccept(ec, clientSock);
				}
			);
		}

		void continueAccept(const asio::error_code& ec,
			std::shared_ptr<asio::ip::tcp::socket> client)
		{
			onNewConnection(client, ec);

			asyncReadMessage(client,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<lnet::LNetMessage> msg, const asio::error_code& ec)
				{
					repeatRead(sock, msg, ec);
				}
			);

			initAccept();
		}

		void repeatRead(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<LNetMessage> readMsg, const asio::error_code ec)
		{
			recievedMessage(client, readMsg, ec);

			asyncReadMessage(client,
				[this, client](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<lnet::LNetMessage> msg, const asio::error_code& ec)
				{
					if (ec != asio::error::eof && ec != asio::error::connection_reset)
					{
						repeatRead(sock, msg, ec);
					}
					else
					{
						for (size_t i = 0; i < clients.size(); i++) {
							if (clients[i] == client) {
								clients[i] = nullptr;
								clientsAmount--;
								break;
							}
						}
					}
				}
			);
		}
		
		virtual void onNewConnection(std::shared_ptr<asio::ip::tcp::socket> client, const asio::error_code& ec)
		{
			if (ec)
			{
				return;
			}
			else if (clientsAmount < maxClients)
			{
				clients[clientsAmount++] = client;
			}
			else
			{
				std::cout << "Max clients reached. Cannot accept more clients.\n";
			}

			if (acceptCallback)
			{
				acceptCallback(this, client, ec);
			}
		}


		virtual void recievedMessage(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<lnet::LNetMessage> message, const asio::error_code& ec)
		{
			if (!ec)
			{
				if (msgCallbacks[message->getMsgType()])
				{
					msgCallbacks[message->getMsgType()](this, client, message);
				}
			}

			if (readCallback)
			{
				readCallback(this, client, message, ec);
			}
		}

		virtual void onDisconnect(std::shared_ptr<asio::ip::tcp::socket> client)
		{

		}


	public:
		LNetServer(unsigned short port, size_t threadsAmount,
			std::function<void(LNetServer*, std::shared_ptr<asio::ip::tcp::socket>, const asio::error_code ec)> acceptCallback = nullptr,
			std::function<void(LNetServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<LNetMessage>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(LNetServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<LNetMessage>, const asio::error_code ec)> writeCallback = nullptr) :
			port(port),
			acceptCallback(acceptCallback), readCallback(readCallback), writeCallback(writeCallback),
			workGuard(asio::make_work_guard(ioContext)),
			clientsAmount(0),
			acceptor(asio::ip::tcp::acceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)))
		{

			if (threadsAmount < 1)
			{
				throw std::runtime_error("You must at least have one additional thread. ");
			}

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
		void startServer()
		{
			if (isRunning)
			{
				throw std::runtime_error("The server cannot run while starting it. ");
			}

			isRunning = true;

			clientsAmount = 0;
			acceptor.listen(maxClients);

			initAccept();

		}

		void stopServer()
		{
			if (isRunning)
			{
				for (auto& client : clients) {
					if (client) {
						asio::error_code ec;
						client->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
						client->close(ec);
					}
				}

				ioContext.stop();

				for (size_t i = 0; i < threads.size(); i++)
				{
					if (threads[i]->joinable()) threads[i]->join();
				}

				isRunning = false;
			}
		}

		bool sendClient(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<LNetMessage> msg)
		{
			if (!client)
			{
				return false;
			}

			asyncWriteMessage(client, msg,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<LNetMessage> msg, const asio::error_code& ec)
				{
					if (writeCallback)
					{
						writeCallback(this, sock, msg, ec);
					}
				}
			);

			return true;
		}

		template<typename... T>
		bool sendClient(std::shared_ptr<asio::ip::tcp::socket> client, LNet4Byte type, T... params)
		{
			auto msg = std::make_shared<LNetMessage>(type);

			(void(msg->operator<<(params)), ...);

			return sendClient(client, msg);
		}

		void sendAllClients(std::shared_ptr<LNetMessage> msg)
		{
			for (auto& client : clients)
			{
				if (!client)
				{
					continue;
				}

				asyncWriteMessage(client, msg,
					[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<LNetMessage> msg, const asio::error_code& ec)
					{
						if (writeCallback)
						{
							writeCallback(this, sock, msg, ec);
						}
					}
				);
			}
		}

		template<typename... T>
		void sendAllClients(LNet4Byte type, T... params)
		{
			auto msg = std::make_shared<LNetMessage>(type);

			(void(msg->operator<<(params)), ...);

			sendAllClients(msg);
		}

		void sendAllClientsExcept(std::shared_ptr<asio::ip::tcp::socket> exceptClient, std::shared_ptr<LNetMessage> msg)
		{
			for (auto& client : clients)
			{
				if (!client || exceptClient == client)
				{
					continue;
				}

				asyncWriteMessage(client, msg,
					[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<LNetMessage> msg, const asio::error_code& ec)
					{
						if (writeCallback)
						{
							writeCallback(this, sock, msg, ec);
						}
					}
				);
			}
		}

		template<typename... T>
		void sendAllClientsExcept(std::shared_ptr<asio::ip::tcp::socket> exceptClient, LNet4Byte type, T... params)
		{
			auto msg = std::make_shared<LNetMessage>(type);

			(void(msg->operator<<(params)), ...);

			sendAllClientsExcept(exceptClient, msg);
		}
	
		void addMsgListener(LNet4Byte type, ServerMsgCallback<maxClients> callback)
		{
			msgCallbacks[type] = callback;
		}

};
}
#endif 