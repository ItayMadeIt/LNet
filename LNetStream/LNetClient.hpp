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

		ClientSettings(const LNetByte& channels) : channels(channels)
		{
			// only if channels has a correct value
			assert(channels < 255);
		}
	};

	using LNetReadCallback = std::function<void(Message&)>;

	class Client
	{
	public:

		void tempSend()
		{
			Message msg(true, 88, 88);
			ENetPacket* newPacket = msg.toNetworkPacket();
			
			enet_peer_send(connection, 88, newPacket);
		}

		Client(const LNetByte& channels = 16) : 
			settings(channels),
			host(nullptr)
		{
			if (enet_initialize() != 0)
			{
				std::cerr << "An error occurred while initializing ENet (Server).\n";
				return;
			}
		}

		void connect(const LNet2Byte& port, const std::string& ip)
		{
			enet_address_set_host_ip(&settings.address, ip.c_str());
			settings.address.port = port;

			// create host
			host = enet_host_create(nullptr, 1, settings.channels, 0, 0);

			if (!host)
			{
				throw std::runtime_error("Couldn't create server.");
			}

			// create a connection to the server
			connection = enet_host_connect(host, &settings.address, settings.channels, 0);

			if (!connection)
			{
				std::cerr << "No available peers for initiating an ENet connection.\n";
				exit(EXIT_FAILURE);
			}

			enet_host_flush(host);

			std::cout << "Connected\n";
		}

		void tick()
		{
			ENetEvent event;
			while (enet_host_service(host, &event, 0) > 0)
			{
				switch (event.type)
				{
					case ENET_EVENT_TYPE_CONNECT:
					{
						std::cout << "Client got connection\n";

						break;
					}
					case ENET_EVENT_TYPE_DISCONNECT:
					{
						std::cout << "Client got disconnection\n";
						break;
					}
					case ENET_EVENT_TYPE_RECEIVE:
					{
						std::cout << "Client got packet\n";
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
			enet_host_destroy(host);
			
			host = nullptr;

			enet_deinitialize();
		}

		void send(const Message& message)
		{
			ENetPacket* packet = message.toNetworkPacket();

			enet_peer_send(&host->peers[0],
				message.getMsgChannel(),
				packet);
		}

		template<typename... Args>
		void sendReliable(const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(true, channel, type, args...);

			ENetPacket* packet = message.toNetworkPacket();

			enet_peer_send(&host->peers[0],
				channel,
				packet);
		}

		template<typename... Args>
		void sendUnreliable(const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			Message message = Message::createByArgs(false, channel, type, args...);

			ENetPacket* packet = message.toNetworkPacket();

			enet_peer_send(&host->peers[0],
				channel,
				packet);
			
		}

		void flush()
		{
			enet_host_flush(host);
		}

		bool isConnected()
		{
			return host;
		}

	private:

		// Connection data
		ClientSettings settings;


		// Enet 
		ENetHost* host;

		ENetPeer* connection;


		// Message callbacks
		std::unordered_map<LNet4Byte, LNetReadCallback> messageCallbacks;
	};

}
#endif