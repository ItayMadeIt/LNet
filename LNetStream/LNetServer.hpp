#ifndef LNET_SERVER_HPP
#define LNET_SERVER_HPP

#include <cassert>
#include <unordered_map>
#include "LNetEndianHandler.hpp"
#include "LNetMessage.hpp"
#include "LNetTypes.hpp"
#include <queue>

namespace lnet
{
	struct ServerSettings
	{
		// should match the client
		LNetByte channels;

		LNet4Byte maxConnections;
		
		// any address
		ENetAddress address;

		ServerSettings(const LNet4Byte& maxConnections, const LNetByte& channels)
			: maxConnections(maxConnections), channels(channels), address{ENET_HOST_ANY, 0}
		{
			// only if channels has a correct value
			assert(channels < 255);
		}
	};


	class Server
	{
	public:
		Server(const LNet4Byte& maxConnections = 1000,
			const LNetByte& channels = 16) : 
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
		
		using LNetReadCallback = std::function<void(const LNet4Byte&, Message&)>;

		void init(const LNet2Byte& port)
		{
			settings.address.host = ENET_HOST_ANY;
			settings.address.port = port; 

			// create host
			host = enet_host_create(&settings.address, settings.maxConnections, settings.channels, 0, 0);

			if (!host)
			{
				throw std::runtime_error("Couldn't create server.");
			}

			std::cout << "Initialized server\n";
		}

		void tick()
		{
			ENetEvent event;
			while (enet_host_service(host, &event, 0) > 0)
			{
				std::cout << "~got event~\n";

				switch (event.type)
				{
					case ENET_EVENT_TYPE_CONNECT:
					{
						std::cout << "Server-connection\n";
						handleConnect(event);
						break;
					}
					case ENET_EVENT_TYPE_DISCONNECT:
					{
						std::cout << "Server-disconnection\n";
						handleDisconnect(event);
						break;
					}
					case ENET_EVENT_TYPE_RECEIVE:
					{
						std::cout << "Server-receive\n";
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
		
		void terminate()
		{
			std::cout << "Server got terminated\n";

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

			enet_deinitialize();
		}

		void setMessageCallback(const MessageIdentifier& identifier, const LNetReadCallback& func)
		{
			messageCallbacks[identifier] = func;
		}
		void removeMessageCallback(const MessageIdentifier& identifier)
		{
			messageCallbacks.erase(identifier);
		}

		void sendClient(const LNet4Byte& clientID, const Message& message)
		{
			ENetPacket* packet = message.toNetworkPacket();

			enet_peer_send(clients[clientID], 
				message.getMsgChannel(), 
				packet);
		}
		template<typename... Args>
		void sendReliableClient(const LNet4Byte& clientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(true, channel, type, args...);
			ENetPacket* packet = message.toNetworkPacket();
			enet_peer_send(clients[clientID], channel, packet);
		}
		template<typename... Args>
		void sendUnreliableClient(const LNet4Byte& clientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(false, channel, type, args...); 
			ENetPacket* packet = message.toNetworkPacket();
			enet_peer_send(clients[clientID], channel, packet);
		}
		
		void sendBroadcastExcept(const LNet4Byte& clientID, const Message& message)
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
		template<typename... Args>
		void sendReliableBroadcastExcept(const LNet4Byte& excludedClientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(true, channel, type, args...);  

			for (auto& client : clients)
			{
				if (client.first != excludedClientID)
				{
		 			ENetPacket* packet = message.toNetworkPacket();
					enet_peer_send(client.second, channel, packet);
				}
			}
		}
		template<typename... Args>
		void sendUnreliableBroadcastExcept(const LNet4Byte& excludedClientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(false, channel, type, args...); 

			for (auto& client : clients)
			{
				if (client.first != excludedClientID)
				{
					ENetPacket* packet = message.toNetworkPacket();
					enet_peer_send(client.second, channel, packet);
				}
			}
		}
		
		void sendBroadcast(const Message& message)
		{
			ENetPacket* packet = message.toNetworkPacket();

			enet_host_broadcast(host, message.getMsgChannel(), packet);
		}
		template<typename... Args>
		void sendReliableBroadcast(const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(true, channel, type, args...);  // Create reliable message
			ENetPacket* packet = message.toNetworkPacket();
			enet_host_broadcast(host, channel, packet);
		}
		template<typename... Args>
		void sendUnreliableBroadcast(const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(false, channel, type, args...);  // Create unreliable message
			ENetPacket* packet = message.toNetworkPacket();
			enet_host_broadcast(host, channel, packet);
		}

		~Server()
		{
			terminate();
		}

	private:

		/// <summary>
		/// Handle the connection and saves it in the clients object
		/// </summary>
		/// <param name="event"></param>
		void handleConnect(const ENetEvent& event)
		{
			LNet4Byte id = newClientID();

			clients[id] = event.peer;

			event.peer->data = reinterpret_cast<void*>(id);
		}

		/// <summary>
		/// Handle the disconnection of a peer, and removes it from the clients
		/// </summary>
		/// <param name="event"></param>
		void handleDisconnect(const ENetEvent& event)
		{
			LNet4Byte clientID = reinterpret_cast<LNet4Byte>(event.peer->data);

			possibleIDs.push(clientID);
			clients.erase(clientID);
		}

		/// <summary>
		/// Handle receiving message of a message
		/// </summary>
		/// <param name="event"></param>
		void handleReceive(const ENetEvent& event)
		{
			Message message(event.packet->data, event.packet->dataLength, event.channelID);

			// Call message callback if exists
			auto it = messageCallbacks.find(message.getMsgIdentifier());
			if (it != messageCallbacks.end())
			{
				it->second((LNetByte)event.peer->data, message);
			}
		}

		/// <summary>
		/// Uses a binary search to find the lowest value that the clients ID do not use
		/// </summary>
		/// <returns>The ID as LNet4Byte (4 Bytes of possible IDs)</returns>
		const LNet4Byte newClientID()
		{
			if (!possibleIDs.empty())
			{
				LNet4Byte reusedID = possibleIDs.front();

				possibleIDs.pop();

				return reusedID;
			}

			return currentClientID++;
		}
	

	private:
		
		ServerSettings settings;

		ENetHost* host;

		// all connections with ID
		std::unordered_map<LNet4Byte, ENetPeer*> clients;

		std::queue<LNet4Byte> possibleIDs;
		LNet4Byte currentClientID = 0;
		
		// Message callbacks
		std::unordered_map<MessageIdentifier, LNetReadCallback, HashMessageIdentifier> messageCallbacks;

	};
}

#endif