#ifndef LNET_CLIENT_HPP
#define LNET_CLIENT_HPP

#include <cassert>
#include <enet/enet.h>
#include <unordered_map>
#include "LNetMessage.hpp"

namespace lnet
{
	struct ClientSettings
	{
		// should match the server
		LNetByte channels;
		
		// server address
		ENetAddress address;

		ClientSettings(const LNetByte& channels);
		ClientSettings(const LNetByte& channels, const LNet2Byte& port, const std::string& ip);
		void setAddress(const LNet2Byte& port, const std::string& ip);

	};

	using LNetReadCallback = std::function<void(Message&)>;

	class Client
	{
	public:

		Client(const LNetByte& channels = 16);

		void connect(const LNet2Byte& port, const std::string& ip);

		void tick();

		void terminate();



		void send(const Message& message);

		template<typename... Args>
		void sendReliable(const MessageIdentifier& identifier, const Args&... args);

		template<typename... Args>
		void sendUnreliable(const MessageIdentifier& identifier, const Args&... args);

		void flush();

		bool isConnected();

	private:

		/// <summary>
		/// Handle receiving message of a message
		/// </summary>
		/// <param name="event"></param>
		void handleReceive(const ENetEvent& event);

	private:
		// Connection data
		ClientSettings settings;

		// Enet 
		ENetHost* host;

		ENetPeer* connection;

		// Message callbacks
		std::unordered_map<MessageIdentifier, LNetReadCallback, HashMessageIdentifier> messageCallbacks;
	};

	// template sending functions

	template<typename ...Args>
	void Client::sendReliable(const MessageIdentifier& identifier, const Args & ...args)
	{
		Message message = Message::createByArgs(true, identifier.channel, identifier.type, args...);

		ENetPacket* packet = message.toNetworkPacket();

		enet_peer_send(connection,
			identifier.channel,
			packet
		);
	}
	template<typename ...Args>
	void Client::sendUnreliable(const MessageIdentifier& identifier, const Args & ...args)
	{
		Message message = Message::createByArgs(false, identifier.channel, identifier.type, args...);

		ENetPacket* packet = message.toNetworkPacket();

		enet_peer_send(connection,
			identifier.channel,
			packet
		);

	}
}
#endif