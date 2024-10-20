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
			const LNetByte& channels = 16);
		
		using LNetReadCallback = std::function<void(const LNet4Byte&, Message&)>;

		void listen(const LNet2Byte& port);

		void tick();
		
		void terminate();

		void setMessageCallback(const MessageIdentifier& identifier, const LNetReadCallback& func);
		void removeMessageCallback(const MessageIdentifier& identifier);

		void sendClient(const LNet4Byte& clientID, const Message& message);
		template<typename... Args>
		void sendReliableClient(const LNet4Byte& clientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args);
		template<typename... Args>
		void sendUnreliableClient(const LNet4Byte& clientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args);
		
		void sendBroadcastExcept(const LNet4Byte& clientID, const Message& message);
		template<typename... Args>
		void sendReliableBroadcastExcept(const LNet4Byte& excludedClientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args);
		template<typename... Args>
		void sendUnreliableBroadcastExcept(const LNet4Byte& excludedClientID, const LNetByte& channel, const LNet2Byte& type, const Args&... args);
		
		void sendBroadcast(const Message& message);
		template<typename... Args>
		void sendReliableBroadcast(const LNetByte& channel, const LNet2Byte& type, const Args&... args);
		template<typename... Args>
		void sendUnreliableBroadcast(const LNetByte& channel, const LNet2Byte& type, const Args&... args);

		~Server();

	private:

		/// <summary>
		/// Handle the connection and saves it in the clients object
		/// </summary>
		/// <param name="event"></param>
		void handleConnect(const ENetEvent& event);

		/// <summary>
		/// Handle the disconnection of a peer, and removes it from the clients
		/// </summary>
		/// <param name="event"></param>
		void handleDisconnect(const ENetEvent& event);

		/// <summary>
		/// Handle receiving message of a message
		/// </summary>
		/// <param name="event"></param>
		void handleReceive(const ENetEvent& event);

		/// <summary>
		/// Uses a binary search to find the lowest value that the clients ID do not use
		/// </summary>
		/// <returns>The ID as LNet4Byte (4 Bytes of possible IDs)</returns>
		const LNet4Byte newClientID();
	

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

	// template sending functions

	template<typename ...Args>
	void Server::sendReliableClient(const LNet4Byte& clientID, const LNetByte& channel, const LNet2Byte& type, const Args & ...args)
	{
		Message message = Message::createByArgs(true, channel, type, args...);
		ENetPacket* packet = message.toNetworkPacket();
		enet_peer_send(clients[clientID], channel, packet);
	}
	template<typename ...Args>
	void Server::sendUnreliableClient(const LNet4Byte& clientID, const LNetByte& channel, const LNet2Byte& type, const Args & ...args)
	{
		Message message = Message::createByArgs(false, channel, type, args...);
		ENetPacket* packet = message.toNetworkPacket();
		enet_peer_send(clients[clientID], channel, packet);
	}

	template<typename ...Args>
	void Server::sendReliableBroadcastExcept(const LNet4Byte& excludedClientID, const LNetByte& channel, const LNet2Byte& type, const Args & ...args)
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
	template<typename ...Args>
	void Server::sendUnreliableBroadcastExcept(const LNet4Byte& excludedClientID, const LNetByte& channel, const LNet2Byte& type, const Args & ...args)
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

	template<typename ...Args>
	void Server::sendReliableBroadcast(const LNetByte& channel, const LNet2Byte& type, const Args & ...args)
	{
		Message message = Message::createByArgs(true, channel, type, args...);  // Create reliable message
		ENetPacket* packet = message.toNetworkPacket();
		enet_host_broadcast(host, channel, packet);
	}
	template<typename ...Args>
	void Server::sendUnreliableBroadcast(const LNetByte& channel, const LNet2Byte& type, const Args & ...args)
	{
		Message message = Message::createByArgs(false, channel, type, args...);  // Create unreliable message
		ENetPacket* packet = message.toNetworkPacket();
		enet_host_broadcast(host, channel, packet);
	}

}

#endif