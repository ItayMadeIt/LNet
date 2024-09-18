#ifndef NETWORK_SERVER_HPP
#define NETWORK_SERVER_HPP

#include <asio.hpp>
#include <string>
#include <memory>
#include <map>
#include <array>
#include <vector>
#include <atomic>
#include <unordered_map>
#include "NetworkMessage.hpp"

namespace lnet
{
	template<size_t maxClients>
	class NetworkServer;

	template<size_t maxClients>
	using ServerMsgCallback = std::function<void(NetworkServer<maxClients>*, std::shared_ptr<lnet::Message>)>;

	template<size_t maxClients>
	class NetworkServer
	{
		std::map<LNet4Byte, ServerMsgCallback<maxClients>> msgCallbacks;

	private:
		std::vector<std::unique_ptr<std::thread>> threads;

		unsigned short port;
		size_t clientsAmount;
		asio::io_context ioContext;
		asio::executor_work_guard<asio::io_context::executor_type> workGuard;

		std::atomic<bool> isRunning = false;

		asio::ip::tcp::acceptor acceptor;
		std::array<std::shared_ptr<asio::ip::tcp::socket>, maxClients> clients;

		std::function<void(NetworkServer*, std::shared_ptr<asio::ip::tcp::socket>, const asio::error_code& ec)> acceptCallback;
		std::function<void(NetworkServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<lnet::Message>, const asio::error_code& ec)> readCallback;
		std::function<void(NetworkServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<lnet::Message>, const asio::error_code& ec)> writeCallback;

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
			if (ec)
			{
				std::cout << "Error accepting new client. \n";
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

			acceptCallback(this, client, ec);
			asyncReadMessage(client,
				[this](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<lnet::Message> msg, const asio::error_code& ec)
				{
					repeatRead(sock, msg, ec);
				}
			);

			initAccept();
		}

		void repeatRead(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<Message> readMsg, const asio::error_code ec)
		{
			readCallback(this, client, readMsg, ec);

			asyncReadMessage(client,
				[this, client](std::shared_ptr<asio::ip::tcp::socket> sock, std::shared_ptr<lnet::Message> msg, const asio::error_code& ec)
				{
					if (ec != asio::error::eof && ec != asio::error::connection_reset)
					{
						repeatRead(sock, msg, ec);
					}
					else
					{
						std::cout << "Client disconnected." << std::endl;

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

	public:
		NetworkServer(unsigned short port, size_t threadsAmount,
			std::function<void(NetworkServer*, std::shared_ptr<asio::ip::tcp::socket>, const asio::error_code ec)> acceptCallback = nullptr,
			std::function<void(NetworkServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(NetworkServer*, std::shared_ptr<asio::ip::tcp::socket>, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback = nullptr) :
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

		bool sendClient(std::shared_ptr<asio::ip::tcp::socket> client, std::shared_ptr<Message> msg)
		{
			if (!client)
			{
				return false;
			}

			asyncWriteMessage(client, msg,
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

		void sendAllClients(std::shared_ptr<Message> msg)
		{
			for (auto& client : clients)
			{
				if (!client)
				{
					continue;
				}

				asyncWriteMessage(client, msg,
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

		void sendAllClientsExcept(std::shared_ptr<asio::ip::tcp::socket> exceptClient, std::shared_ptr<Message> msg)
		{
			for (auto& client : clients)
			{
				if (!client || exceptClient == client)
				{
					continue;
				}

				asyncWriteMessage(client, msg,
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
	};
}
#endif 