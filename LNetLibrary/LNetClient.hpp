#ifndef LNET_CLIENT_HPP
#define LNET_CLIENT_HPP

#include <asio.hpp>
#include <string>
#include <memory>
#include <unordered_map>
#include "LNetMessage.hpp"
#include "LNetTCP.hpp"
#include "LNetUDP.hpp"
#include "LNetConnection.hpp"

namespace lnet
{
	class Client;

	using ClientMsgCallback = std::function<void(Client* client, std::shared_ptr<Message>)>;

	class Client
	{
	protected:
		asio::io_context ioContext;
		asio::executor_work_guard<asio::io_context::executor_type> workGuard;
		std::vector<std::unique_ptr<std::thread>> threads;

		std::string serverIp;
		unsigned short port;

		std::shared_ptr<Connection> connection;

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

			onConnect();

			repeatReadTCP();
			//repeatReadUDP();
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

		void repeatReadTCP()
		{
			TCP::asyncRead(connection->tcpSocket,
				[this](TCPSocket& sock, std::shared_ptr<Message> msg, const asio::error_code& ec) 
				{
					onRead(true, msg, ec);

					repeatReadTCP();
				}
			);
		}

		void repeatReadUDP()
		{
			UDP::asyncRead(connection->udpSocket, 
				[this](UDPSocket& sock, UDPEndpoint& ep, std::shared_ptr<Message> msg, const asio::error_code& ec)
				{
					// endpoint does nothing, we are a client so the server sent us the message
					onRead(false, msg, ec);

					repeatReadUDP();
				}
			);
		}

	public:

		Client(std::string serverIp, unsigned short port, size_t threadsAmount = 2,
			std::function<void(Client*, const asio::error_code ec)> connectedCallback = nullptr,
			std::function<void(Client*, bool, std::shared_ptr<Message>, const asio::error_code ec)> readCallback = nullptr,
			std::function<void(Client*, bool, std::shared_ptr<Message>, const asio::error_code ec)> writeCallback = nullptr) :
			serverIp(serverIp), port(port), isConnected(false),
			workGuard(asio::make_work_guard(ioContext)),
			connectedCallback(connectedCallback), readCallback(readCallback), writeCallback(writeCallback)
		{
			connection = nullptr;

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
			connection = std::make_shared<Connection>(ioContext);
			
			connection->connect(serverIp, port,
				[this](const asio::error_code& ec)
				{
					if (connectedCallback)
					{
						connectedCallback(this, ec);
					}

					if (!ec)
					{
						handleConnection();
					}
					else
					{
						connection = nullptr;
						throw std::runtime_error("Connection failed");
					}
				}
			);
		}


		void sendTCP(std::shared_ptr<Message> msg)
		{
			TCP::asyncSend(connection->tcpSocket,
				[this](TCPSocket& sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
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
			auto msg = Message::createByArgs(type, params...);

			sendTCP(msg);
		}


		void sendUDP(std::shared_ptr<Message> msg)
		{
			UDP::asyncSend(connection->udpSocket, connection->udpRemoteEndpoint,
				[this](UDPSocket& sock, std::shared_ptr<Message> msg, const asio::error_code& ec)
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
			// create message
			auto msg = Message::createByArgs(type, params);

			sendUDP(msg);
		}

		
		void addMsgListener(LNet4Byte type, ClientMsgCallback callback)
		{
			msgCallbacks[type] = callback;
		}


		void disconnect()
		{
			connection = nullptr;

			isConnected = false;

			workGuard.reset();

			ioContext.stop();

			for (size_t i = 0; i < threads.size(); i++)
			{
				if (threads[i]->joinable()) threads[i]->join();
			}

		}
	};
}
#endif
