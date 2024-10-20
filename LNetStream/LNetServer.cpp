#include "LNetServer.hpp"

namespace lnet
{
	Server::Server(const LNet4Byte& maxConnections, const LNetByte& channels) :
		settings(maxConnections, channels),
		currentClientID(0),
		host(nullptr)
	{
		if (enet_initialize() != 0)
		{
			std::cerr << "An error occurred while initializing ENet (Server).\n";
			return;
		}
	}
	void Server::listen(const LNet2Byte& port)
	{
		settings.address.host = ENET_HOST_ANY;
		settings.address.port = port;

		
		// clear queue
		possibleIDs = std::queue<LNet4Byte>();
		clients.clear();


		// create host
		host = enet_host_create(&settings.address, settings.maxConnections, settings.channels, 0, 0);

		if (!host)
		{
			throw std::runtime_error("Couldn't create server.");
		}
	}
	void Server::tick()
	{
		ENetEvent event;
		while (enet_host_service(host, &event, 0) > 0)
		{
			switch (event.type)
			{
				case ENET_EVENT_TYPE_CONNECT:
				{
					// std::cout << "Server-connection\n";
					handleConnect(event);
					break;
				}
				case ENET_EVENT_TYPE_DISCONNECT:
				{
					// std::cout << "Server-disconnection\n";
					handleDisconnect(event);
					break;
				}
				case ENET_EVENT_TYPE_RECEIVE:
				{
					// std::cout << "Server-receive\n";
					handleReceive(event);
					enet_packet_destroy(event.packet);
					break;
				}
				default:
				{
					std::cout << "Unknown event in the server.\n" <<
						"Peer DATA: " << event.peer->data <<
						"Type: " << event.type <<
						"Channel" << event.channelID <<
						"packet" << event.packet;
				}
			}
		}
	}
	void Server::terminate()
	{
		if (host)
		{
			for (ENetPeer* peer = host->peers; peer < &host->peers[host->peerCount]; ++peer)
			{
				if (peer->state == ENET_PEER_STATE_CONNECTED)
				{
					enet_peer_timeout(peer, 32, 500, 1000);  // Shorten timeout
					enet_peer_disconnect(peer, 0);  // Send disconnect message
				}
			}
			enet_host_flush(host);

			enet_host_destroy(host);

			host = nullptr;
		}

		clients.clear();
		possibleIDs = std::queue<LNet4Byte>();

		enet_deinitialize();
	}

	// MESSAGE CALLBACKS
	
	void Server::setMessageCallback(const MessageIdentifier& identifier, const LNetReadCallback& func)
	{
		messageCallbacks[identifier] = func;
	}
	void Server::removeMessageCallback(const MessageIdentifier& identifier)
	{
		messageCallbacks.erase(identifier);
	}
	

	// SEND FUNCTIONS
	
	void Server::sendClient(const LNet4Byte& clientID, const Message& message)
	{
		ENetPacket* packet = message.toNetworkPacket();

		enet_peer_send(clients[clientID],
			message.getMsgChannel(),
			packet);
	}
	
	void Server::sendBroadcastExcept(const LNet4Byte& clientID, const Message& message)
	{
		for (auto& client : clients)
		{
			if (client.first != clientID)
			{
				ENetPacket* packet = message.toNetworkPacket();
				enet_peer_send(client.second,
					message.getMsgChannel(),
					packet);
			}
		}
	}
	
	void Server::sendBroadcast(const Message& message)
	{
		ENetPacket* packet = message.toNetworkPacket();

		enet_host_broadcast(host, message.getMsgChannel(), packet);
	}
	

	Server::~Server()
	{
		terminate();
	}

	/// <summary>
	/// Handle the connection and saves it in the clients object
	/// </summary>
	/// <param name="event"></param>

	void Server::handleConnect(const ENetEvent& event)
	{
		LNet4Byte id = newClientID();

		clients[id] = event.peer;

		event.peer->data = reinterpret_cast<void*>(id);
	}

	/// <summary>
	/// Handle the disconnection of a peer, and removes it from the clients
	/// </summary>
	/// <param name="event"></param>

	void Server::handleDisconnect(const ENetEvent& event)
	{
		LNet4Byte clientID = reinterpret_cast<LNet4Byte>(event.peer->data);

		possibleIDs.push(clientID);
		clients.erase(clientID);
	}

	/// <summary>
	/// Handle receiving message of a message
	/// </summary>
	/// <param name="event"></param>

	void Server::handleReceive(const ENetEvent& event)
	{
		Message message(event.packet->data, event.packet->dataLength, event.channelID);

		// Call message callback if exists
		auto it = messageCallbacks.find(message.getMsgIdentifier());
		if (it != messageCallbacks.end())
		{
			it->second((LNetByte)event.peer->data, message);
		}
	}

	const LNet4Byte Server::newClientID()
	{
		if (!possibleIDs.empty())
		{
			LNet4Byte reusedID = possibleIDs.front();

			possibleIDs.pop();

			return reusedID;
		}

		return currentClientID++;
	}
}