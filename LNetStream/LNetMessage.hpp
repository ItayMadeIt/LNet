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

		MessageIdentifier(const LNetByte channel, const LNet2Byte type) :
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

		Message(const LNet2Byte type, const LNetByte channel = 0) : isReliable(true), identifier{ channel, type} {}

		Message(const bool isReliable, const LNetByte channel, const LNet2Byte type) : isReliable(isReliable), identifier(channel, type) {}

		Message(const MessageIdentifier identifier) : isReliable(true), identifier(identifier) {}

		Message(const bool isReliable, const MessageIdentifier identifier) : isReliable(isReliable), identifier(identifier) {}

		Message(const LNetByte* arr, const size_t& length);

		Message(const LNetByte* arr, const size_t& length, const LNetByte& channel);


		// GETTERS AND SETTERS
		void setMsgChannel(const LNetByte value);
		void setMsgType   (const LNet2Byte value);
		void setMsgSize   (const LNet4Byte value);
		void setIsReliable(const bool value);

		MessageIdentifier getMsgIdentifier() const;
		LNetByte getMsgChannel() const;
		LNet2Byte getMsgType() const;
		LNet4Byte getMsgSize() const;
		bool getIsReliable() const;
		
		const std::vector<LNetByte>& getPayload() const;


		// STATIC

		template<typename... Args>
		static void loadArgs(Message& msg, Args... args);

		template<typename... Args>
		static void loadArgs(std::shared_ptr<Message> msg, Args... args);

		template<typename... Args>
		static Message createByArgs(const bool isReliable, const LNetByte channel, const LNet2Byte type, const Args... args);


		// TO BUFFERS

		// Prepare data for network transmission (converts header to network byte order, and gives a shared ptr to a vector with all the data)
		const std::shared_ptr<std::vector<LNetByte>> toNetworkBuffer() const;

		ENetPacket* toNetworkPacket() const;



		// INPUTS
		
		// Input values
		template<typename T>
		Message& operator <<(const T value);

		// Input string
		Message& operator <<(const std::string& value);

		// Input const char* as string
		Message& operator <<(const char* value);

		// Define input size
		inline Message& operator <<(const MessageSizes size);

		// Input list
		template<typename T>
		Message& operator <<(const std::vector<T>& list);

		// Input array
		template<typename T, size_t SIZE>
		Message& operator <<(const std::array<T, SIZE>& arr);



		// OUTPUTS

		// Output values
		template<typename T>
		Message& operator >>(T& value);

		// Output string 
		Message& operator >>(std::string& value);

		// Define output size
		inline Message& operator >>(const MessageSizes size);

		// Output list 
		template<typename T>
		Message& operator >>(std::vector<T>& list);

		// Output array
		template<typename T, size_t SIZE>
		Message& operator >>(std::array<T, SIZE>& arr);


		// Print
		friend std::ostream& operator<<(std::ostream& os, const Message& msg);
		friend std::ostream& operator<<(std::ostream& os, std::shared_ptr<Message>& msg);


		// reset function
		void reset(LNetByte channel=0, LNet2Byte type=0);

	private:
		
		MessageIdentifier identifier;
		
		bool isReliable;
		
		std::vector<LNetByte> payload;  // Payload follows after the header
		size_t readPosition = 0; // To track the current read position in the payload
		MessageSizes inputSize = MessageSizes::Size4Byte;
		MessageSizes outputSize = MessageSizes::Size4Byte;          
	};


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


	// DECIDED TO PUT ALL INPUT & OUTPUT FUNCTIONS IN HEADER AS MOST OF THEM USE TEMPALTES
	
	// Input values

	template<typename T>
	Message& Message::operator<<(const T value)
	{
		// Verify value can be converted
		static_assert(std::is_trivial<T>::value && std::is_standard_layout<T>::value,
			"Only trivial types can be added to the payload");

		size_t sizeBefore = payload.size();
		payload.resize(sizeBefore + sizeof(T));

		std::memcpy(payload.data() + sizeBefore, &value, sizeof(T));

		return *this;
	}

	// Input list

	template<typename T>
	Message& Message::operator<<(const std::vector<T>& list)
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
	Message& Message::operator<<(const std::array<T, SIZE>& arr)
	{
		// Input every value in the list
		for (auto& v : arr)
		{
			*this << v;
		}

		return *this;
	}



	// Output values

	template<typename T>
	Message& Message::operator>>(T& value)
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

	// Output list 

	template<typename T>
	Message& Message::operator>>(std::vector<T>& list)
	{
		// value to hold length
		size_t length = 0;

		// gather length
		switch (outputSize)
		{
		case MessageSizes::Size1Byte:
		{
			*this >> (LNetByte&)length;
			break;
		}
		case MessageSizes::Size2Byte:
		{
			*this >> (LNet2Byte&)length;
			break;
		}
		case MessageSizes::Size4Byte:
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
	Message& Message::operator>>(std::array<T, SIZE>& arr)
	{
		// add all values
		for (size_t i = 0; i < SIZE; i++)
		{
			*this >> arr[i];
		}
	}







	// Create message functions (template functions)

	template<typename ...Args>
	void Message::loadArgs(Message& msg, Args ...args)
	{
		(void(msg.operator<<(args)), ...);
	}

	template<typename ...Args>
	void Message::loadArgs(std::shared_ptr<Message> msg, Args ...args)
	{
		(void(msg->operator<<(args)), ...);
	}

	template<typename ...Args>
	Message Message::createByArgs(const bool isReliable, const LNetByte channel, const LNet2Byte type, const Args...args)
	{
		auto msg = Message(isReliable, channel, type);

		(void(msg.operator<<(args)), ...);

		return msg;
	}


}
#endif