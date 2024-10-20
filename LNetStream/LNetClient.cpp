#include "LNetClient.hpp"

namespace lnet
{

	ClientSettings::ClientSettings(const LNetByte& channels) : channels(channels), address()
	{
		// only if channels has a correct value
		assert(channels < 255);
	}
	ClientSettings::ClientSettings(const LNetByte& channels, const LNet2Byte& port, const std::string& ip) : channels(channels)
	{
		setAddress(port, ip);
	}
	void ClientSettings::setAddress(const LNet2Byte& port, const std::string& ip)
	{
		address.port = port;
		enet_address_set_host_ip(&address, ip.c_str());
	}

	Client::Client(const LNetByte& channels) :
		settings(channels),
		host(nullptr)
	{
		if (enet_initialize() != 0)
		{
			std::cerr << "An error occurred while initializing ENet (Server).\n";
			return;
		}
	}
	void Client::connect(const LNet2Byte& port, const std::string& ip)
	{
		settings.setAddress(port, ip);

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
	void Client::tick()
	{
		ENetEvent event;
		while (enet_host_service(host, &event, 0) > 0)
		{
			switch (event.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
			{
				// nothing for now
				std::cout << "Client-connection\n";
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
			{
				// nothing for now
				std::cout << "Client-disconnection\n";
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE:
			{
				std::cout << "Client-receive\n";
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
	void Client::terminate()
	{
		enet_host_destroy(host);

		host = nullptr;

		enet_deinitialize();
	}
	void Client::send(const Message& message)
	{
		ENetPacket* packet = message.toNetworkPacket();

		enet_peer_send(connection,
			message.getMsgChannel(),
			packet);
	}

	void Client::flush()
	{
		enet_host_flush(host);
	}

	bool Client::isConnected()
	{
		return host;
	}

	/// <summary>
	/// Handle receiving message of a message
	/// </summary>
	/// <param name="event"></param>

	void Client::handleReceive(const ENetEvent& event)
	{
		Message message(event.packet->data, event.packet->dataLength, event.channelID);

		// Call message callback if exists
		auto it = messageCallbacks.find(message.getMsgIdentifier());
		if (it != messageCallbacks.end())
		{
			it->second(message);
		}
	}




}