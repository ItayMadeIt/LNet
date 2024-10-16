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
	class Server;

	using ServerMsgCallback = std::function<void(Server*, std::shared_ptr<TCPSocket>, std::shared_ptr<lnet::Message>)>;

	class Server
	{

	protected:
		std::vector<std::unique_ptr<std::thread>> threads;

		unsigned short port;
		size_t clientsAmount;
		asio::io_context ioContext;
		asio::executor_work_guard<asio::io_context::executor_type> workGuard;

		std::atomic<bool> isRunning = false;

		asio::ip::tcp::acceptor acceptor;
		std::vector<std::shared_ptr<asio::ip::tcp::socket>> clients;

		std::function<void(Server*, std::shared_ptr<asio::ip::tcp::socket>, const asio::error_code& ec)> acceptCallback;
		std::function<void(Server*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<lnet::Message>, const asio::error_code& ec)> readCallback;
		std::function<void(Server*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<lnet::Message>, const asio::error_code& ec)> writeCallback;

		std::unordered_map<LNet4Byte, ServerMsgCallback> msgCallbacks;

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

		void continueAccept(const asio::error_code& ec, std::shared_ptr<asio::ip::tcp::socket> client)
		{
			onNewConnection(client, ec);

			TCP::asyncRead(client,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<lnet::Message> msg, const asio::error_code& ec)
				{
					repeatRead(sock, msg, ec);
				}
			);

			initAccept();
		}

		void repeatRead(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<Message> readMsg, const asio::error_code ec)
		{
			recievedMessage(client, readMsg, ec);

			TCP::asyncRead(client,
				[this, client](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<lnet::Message> msg, const asio::error_code& ec)
				{
					if (ec != asio::error::eof && ec != asio::error::connection_reset)
					{
						repeatRead(sock, msg, ec);
					}
					else
					{
						handleDisconnection(sock);
					}
				}
			);
		}

		void handleDisconnection(const std::shared_ptr<asio::ip::tcp::socket>& client)
		{
			auto it = std::find(clients.begin(), clients.end(), client);

			if (it == clients.end())
			{
				assert("Trying to disconnect a client that isn't connected.");

				return;
			}

			onDisconnect(client);

			int index = it - clients.begin();

			client->close();

			clients[index] = nullptr;
		}


		virtual void onNewConnection(std::shared_ptr<asio::ip::tcp::socket> client, const asio::error_code& ec)
		{
			if (ec)
			{
				return;
			}
			else
			{
				clients.push_back(client);
			}

			if (acceptCallback)
			{
				acceptCallback(this, client, ec);
			}
		}

		virtual void recievedMessage(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<lnet::Message> message, const asio::error_code& ec)
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

		virtual void onDisconnect(const std::shared_ptr<asio::ip::tcp::socket>& client)
		{
			
		}


	public:
		Server(unsigned short port, size_t threadsAmount,
			std::function<void(Server*, std::shared_ptr<asio::ip::tcp::socket>, const asio::error_code ec)> acceptCallback = nullptr,
			std::function<void(Server*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(Server*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback = nullptr) :
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

			acceptor.listen();

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


		bool sendClient(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<Message> msg)
		{
			if (!client)
			{
				return false;
			}

			asyncWriteMessageTCP(client, msg,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
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
			auto msg = std::make_shared<Message>(type);

			(void(msg->operator<<(params)), ...);

			return sendClient(client, msg);
		}


		void sendAllClients(std::shared_ptr<Message> msg)
		{
			for (auto& client : clients)
			{
				if (!client)
				{
					continue;
				}

				asyncWriteMessageTCP(client, msg,
					[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
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
			auto msg = std::make_shared<Message>(type);

			(void(msg->operator<<(params)), ...);

			sendAllClients(msg);
		}


		void sendAllClientsExcept(std::shared_ptr<asio::ip::tcp::socket> exceptClient, std::shared_ptr<Message> msg)
		{
			for (auto& client : clients)
			{
				if (!client || exceptClient == client)
				{
					continue;
				}

				asyncWriteMessageTCP(client, msg,
					[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
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
			auto msg = std::make_shared<Message>(type);

			(void(msg->operator<<(params)), ...);

			sendAllClientsExcept(exceptClient, msg);
		}
	

		void addMsgListener(LNet4Byte type, ServerMsgCallback callback)
		{
			msgCallbacks[type] = callback;
		}

};
}
#endif 