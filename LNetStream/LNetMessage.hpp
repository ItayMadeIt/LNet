#ifndef LNET_MESSAGE_HPP
#define LNET_MESSAGE_HPP

#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <cstdint>
#include <memory>
#include <iomanip>
#include "LNetEndianHandler.hpp"
#include <functional>

namespace lnet
{
	struct MessageIdentifier
	{
		LNetByte channel;
		LNet2Byte type;

		MessageIdentifier() : channel(0), type(0)
		{ }

		MessageIdentifier(const LNetByte& channel, const LNet2Byte& type) :
			channel(channel), type(type)
		{ }

		bool operator==(const MessageIdentifier& other) const
		{
			return channel == other.channel && type == other.type;
		}
	};

	struct HashMessageIdentifier
	{
		std::size_t operator()(const MessageIdentifier& identifier) const
		{
			return std::hash<LNet2Byte>()(identifier.type) ^ 
				std::hash<LNetByte>()(identifier.channel);
		}
	};


	constexpr size_t LNET_CHANNEL_SIZE = 1;
	constexpr size_t LNET_TYPE_SIZE = 2;

	enum class MessageSizes
	{
		Size1Byte = 1,
		Size2Byte = 2,
		Size4Byte = 4,
	};

	class Message
	{
	public:
		// CONSTRUCTORS
		
		Message() : isReliable(true) {}  // Default constructor

		Message(const LNet2Byte& type, const LNetByte& channel = 0) : isReliable(true), identifier{ channel, type} {}

		Message(const bool& isReliable, const LNetByte& channel, const LNet2Byte& type) : isReliable(isReliable), identifier(channel, type) {}

		Message(const MessageIdentifier& identifier) : isReliable(true), identifier(identifier) {}

		Message(const bool& isReliable, const MessageIdentifier& identifier) : isReliable(isReliable), identifier(identifier) {}

		Message(const LNetByte* arr, const size_t& length) : isReliable(true)
		{
			// copy from memory the first value, type (as size can be calculated)
			std::memcpy(&identifier.type, arr, LNET_TYPE_SIZE);

			identifier.type = LNetEndiannessHandler::fromNetworkEndian(identifier.type);

			size_t payloadSize = length -  LNET_TYPE_SIZE;

			if (payloadSize > 0)
			{
				payload.resize(payloadSize);
				std::memcpy(payload.data(), arr + LNET_TYPE_SIZE, payload.size());
			}
		}

		Message(const LNetByte* arr, const size_t& length, const LNetByte& channel) : isReliable(true)
		{
			identifier.channel = channel;

			// copy from memory the first 2 values, type and size
			std::memcpy(&identifier.type, arr, LNET_TYPE_SIZE);

			identifier.type = LNetEndiannessHandler::fromNetworkEndian(identifier.type);

			size_t payloadSize = length - LNET_TYPE_SIZE;

			if (payloadSize > 0)
			{
				payload.resize(payloadSize);
				std::memcpy(payload.data(), arr + LNET_TYPE_SIZE, payload.size());
			}
		}


		// GETTERS AND SETTERS
		void setMsgChannel(const LNetByte& value)
		{
			identifier.channel = value;
		}
		void setMsgType   (const LNet2Byte& value)
		{
			identifier.type = value;
		}
		void setMsgSize   (const LNet4Byte& value)
		{
			payload.resize(value);
		}
		void setIsReliable(const bool& value)
		{
			isReliable = value;
		}

		const MessageIdentifier& getMsgIdentifier() const
		{
			return identifier;
		}
		const LNetByte& getMsgChannel() const
		{
			return identifier.channel;
		}
		const LNet2Byte& getMsgType() const
		{
			return identifier.type;

		}
		const LNet4Byte& getMsgSize() const
		{
			return payload.size() + LNET_TYPE_SIZE;
		}
		const bool& getIsReliable() const
		{
			return isReliable;
		}
		
		const std::vector<LNetByte>& getPayload() const
		{
			return payload;
		}


		// STATIC

		template<typename... Args>
		static void loadArgs(Message& msg, Args... args)
		{
			(void(msg.operator<<(args)), ...);
		}

		template<typename... Args>
		static void loadArgs(std::shared_ptr<Message> msg, Args... args)
		{
			(void(msg->operator<<(args)), ...);
		}

		template<typename... Args>
		static Message createByArgs(const bool& isReliable, const LNetByte& channel, const LNet2Byte& type, const Args&... args)
		{
			auto msg = Message(isReliable, channel, type);

			(void(msg.operator<<(args)), ...);

			return msg;
		}


		// TO BUFFERS

		// Prepare data for network transmission (converts header to network byte order, and gives a shared ptr to a vector with all the data)
		const std::shared_ptr<std::vector<LNetByte>> toNetworkBuffer() const
		{
			// I hope it resizes it
			std::shared_ptr<std::vector<LNetByte>> buffer = 
				std::make_shared< std::vector<LNetByte>>(LNET_TYPE_SIZE + payload.size());

			// Create a network order header using the EndiannessHandler
			LNet2Byte netType = LNetEndiannessHandler::toNetworkEndian(identifier.type);

			memcpy(buffer->data(), &netType, LNET_TYPE_SIZE);

			if (readPosition < payload.size())
			{
				memcpy(buffer->data() + LNET_TYPE_SIZE, payload.data(), payload.size() - readPosition);
			}

			return buffer;
		}

		ENetPacket* toNetworkPacket() const
		{
			auto buffer = toNetworkBuffer();

			// I hope it resizes it
			ENetPacket* packet = enet_packet_create(
				buffer->data(),
				buffer->size(),
				isReliable ? ENET_PACKET_FLAG_RELIABLE : 0
			);

			return packet;
		}



		// INPUTS
		
		// Input values
		template<typename T>
		Message& operator <<(const T& value)
		{
			// Verify value can be converted
			static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value,
				"Only trivial types can be added to the payload");

			size_t sizeBefore = payload.size();
			payload.resize(sizeBefore + sizeof(T));

			std::memcpy(payload.data() + sizeBefore, &value, sizeof(T));

			return *this;
		}

		// Input string
		Message& operator <<(const std::string& value)
		{
			size_t sizeBefore = payload.size();
			payload.resize(sizeBefore + (value.length() + 1));

			std::memcpy(payload.data() + sizeBefore, value.c_str(), value.length() + 1);

			return *this;
		}

		// Input const char* as string
		Message& operator <<(const char* value)
		{
			size_t sizeBefore = payload.size();
			payload.resize(sizeBefore + (std::strlen(value) + 1));

			std::memcpy(payload.data() + sizeBefore, value, std::strlen(value) + 1);

			return *this;
		}

		// Define input size
		Message& operator <<(const MessageSizes& size)
		{
			inputSize = size;
		}

		// Input list
		template<typename T>
		Message& operator <<(const std::vector<T>& list)
		{
			// put the size as input size:
			switch (inputSize)
			{
				case MessageSizes::Size1Byte:
				{
					LNetByte length = static_cast<LNetByte>(list.size());
					*this << length;
					break;
				}
				case MessageSizes::Size2Byte:
				{
					LNet2Byte length = static_cast<LNet2Byte>(list.size());
					*this << length;
					break;
				}
				case MessageSizes::Size4Byte:
				{
					LNet4Byte length = static_cast<LNet4Byte>(list.size());
					*this << length;
					break;
				}
				default:
				{
					throw std::runtime_error("Undefined Message List Size");
				}
			}

			// input every value in the list
			for (auto& v : list)
			{
				*this << v;
			}

			return *this;
		}

		// Input array
		template<typename T, size_t SIZE>
		Message& operator <<(const std::array<T, SIZE>& arr)
		{
			// Input every value in the list
			for (auto& v : arr)
			{
				*this << v;
			}

			return *this;
		}



		// OUTPUTS

		// Output values
		template<typename T>
		Message& operator >>(T& value)
		{
			// Verify value can be converted
			static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value,
				"Only trivial types can be added to the payload");

			// Verify it can be taken as input
			if (readPosition + sizeof(T) > payload.size())
			{
				throw std::runtime_error("Not enough data in payload to extract type.");
			}

			std::memcpy(reinterpret_cast<void*>(&value), payload.data(), sizeof(T));

			readPosition += sizeof(T);

			return *this;
		}

		// Output string 
		Message& operator >>(std::string& value)
		{
			size_t sizeBefore = payload.size();
			auto nullTermPos = std::find(payload.begin() + readPosition, payload.end(), '\0');

			if (nullTermPos == payload.end()) 
			{
				throw std::runtime_error("No null terminator found, string is incomplete.");
			}

			size_t stringLength = nullTermPos - (payload.begin() + readPosition);
			value.assign(reinterpret_cast<const char*>(payload.data() + readPosition), stringLength);

			readPosition += stringLength + 1;

			return *this;
		}

		// Define output size
		Message& operator >>(const MessageSizes& size)
		{
			outputSize = size;
		}

		// Output list 
		template<typename T>
		Message& operator >>(std::vector<T>& list)
		{
			// value to hold length
			size_t length = 0;

			// gather length
			switch (outputSize)
			{
				case lnet::MessageSizes::Size1Byte:
				{
					*this >> (LNetByte&)length;
					break;
				}
				case lnet::MessageSizes::Size2Byte:
				{
					*this >> (LNet2Byte&)length;
					break;
				}
				case lnet::MessageSizes::Size4Byte:
				{
					*this >> (LNet4Byte&)length;
					break;
				}
				default:
				{
					throw std::runtime_error("Undefined Message List Size");
				}
			}

			// set list length
			list.resize(length);

			// add all values
			for (size_t i = 0; i < length; i++)
			{
				*this >> list[i];
			}
		}

		// Output array
		template<typename T, size_t SIZE>
		Message& operator >>(std::array<T, SIZE>& arr)
		{
			// add all values
			for (size_t i = 0; i < SIZE; i++)
			{
				*this >> arr[i];
			}
		}


		// Print
		friend std::ostream& operator<<(std::ostream& os, const Message& msg)
		{
			os << "---------------------------------------------\n"\
				"Channel: " << msg.getMsgChannel() << '\n' <<
				"Type: " << msg.getMsgType() << '\n' <<
				"Length: " << msg.getMsgSize() << '\n' <<
				"-----------------------------------------------\n"\
				"PAYLOAD: \n";

			for (const auto& byte : msg.payload) {
				os << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
			}

			os << std::dec;
			
			os << "-----------------------------------------------\n";
			
			return os;
		}
		friend std::ostream& operator<<(std::ostream& os, std::shared_ptr<Message>& msg)
		{
			return os << *msg;
		}


		// reset function
		void reset(LNetByte channel=0, LNet2Byte type=0)
		{
			identifier.channel = 0;
			identifier.type = 0;
			payload.clear();
		}

	private:
		
		MessageIdentifier identifier;
		
		bool isReliable;
		
		std::vector<LNetByte> payload;  // Payload follows after the header
		size_t readPosition = 0; // To track the current read position in the payload
		MessageSizes inputSize = MessageSizes::Size4Byte;
		MessageSizes outputSize = MessageSizes::Size4Byte;          
	};


	void addPacketToQueue(ENetPeer* peer, const Message& msg)
	{
		ENetPacket* packet = msg.toNetworkPacket();

		enet_peer_send(peer, msg.getMsgChannel(), packet);
	}

	void addPacketToQueue(ENetPeer* peer, Message& msg, const bool& isReliable)
	{
		msg.setIsReliable(isReliable);

		ENetPacket* packet = msg.toNetworkPacket();

		enet_peer_send(peer, msg.getMsgChannel(), packet);
	}



	template<typename T>
	std::shared_ptr<Message>& operator <<(std::shared_ptr<Message>& msgPtr, const T& value)
	{
		if (!msgPtr)
		{
			throw std::runtime_error("Null message pointer in << operator.");
		}

		*msgPtr << value;

		return msgPtr;
	}
	template<typename T>
	std::shared_ptr<Message>& operator >>(std::shared_ptr<Message>& msgPtr, T& value)
	{
		if (!msgPtr)
		{
			throw std::runtime_error("Null message pointer in >> operator.");
		}

		*msgPtr >> value;

		return msgPtr;
	}

}
#endif