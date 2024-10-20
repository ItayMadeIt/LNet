#include "LNetMessage.hpp"

namespace lnet
{
	// GETTERS AND SETTERS

	Message::Message(const LNetByte* arr, const size_t& length) : isReliable(true)
	{
		// copy from memory the first value, type (as size can be calculated)
		std::memcpy(&identifier.type, arr, LNET_TYPE_SIZE);

		identifier.type = LNetEndiannessHandler::fromNetworkEndian(identifier.type);

		size_t payloadSize = length - LNET_TYPE_SIZE;

		if (payloadSize > 0)
		{
			payload.resize(payloadSize);
			std::memcpy(payload.data(), arr + LNET_TYPE_SIZE, payload.size());
		}
	}

	Message::Message(const LNetByte* arr, const size_t& length, const LNetByte& channel) : isReliable(true)
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

	void Message::setMsgChannel(const LNetByte value)
	{
		identifier.channel = value;
	}

	void Message::setMsgType(const LNet2Byte value)
	{
		identifier.type = value;
	}

	void Message::setMsgSize(const LNet4Byte value)
	{
		payload.resize(value);
	}

	void Message::setIsReliable(const bool value)
	{
		isReliable = value;
	}

	MessageIdentifier Message::getMsgIdentifier() const
	{
		return identifier;
	}

	LNetByte Message::getMsgChannel() const
	{
		return identifier.channel;
	}

	LNet2Byte Message::getMsgType() const
	{
		return identifier.type;

	}

	LNet4Byte Message::getMsgSize() const
	{
		return payload.size() + LNET_TYPE_SIZE;
	}

	bool Message::getIsReliable() const
	{
		return isReliable;
	}

	const std::vector<LNetByte>& Message::getPayload() const
	{
		return payload;
	}

	// Prepare data for network transmission (converts header to network byte order, and gives a shared ptr to a vector with all the data)

	const std::shared_ptr<std::vector<LNetByte>> Message::toNetworkBuffer() const
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

	ENetPacket* Message::toNetworkPacket() const
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


	// INPUT
	

	// Input string

	Message& Message::operator<<(const std::string& value)
	{
		size_t sizeBefore = payload.size();
		payload.resize(sizeBefore + (value.length() + 1));

		std::memcpy(payload.data() + sizeBefore, value.c_str(), value.length() + 1);

		return *this;
	}

	// Input const char* as string

	Message& Message::operator<<(const char* value)
	{
		size_t sizeBefore = payload.size();
		payload.resize(sizeBefore + (std::strlen(value) + 1));

		std::memcpy(payload.data() + sizeBefore, value, std::strlen(value) + 1);

		return *this;
	}

	// Define input size

	Message& Message::operator<<(const MessageSizes size)
	{
		inputSize = size;

		return *this;
	}

	
	// OUTPUT

	// Output string 

	Message& Message::operator>>(std::string& value)
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

	Message& Message::operator>>(const MessageSizes size)
	{
		outputSize = size;
	}




	// reset function

	void Message::reset(LNetByte channel, LNet2Byte type)
	{
		identifier.channel = 0;
		identifier.type = 0;
		payload.clear();
	}

	// Print
	std::ostream& operator<<(std::ostream& os, const Message& msg)
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

	std::ostream& operator<<(std::ostream& os, std::shared_ptr<Message>& msg)
	{
		return os << *msg;
	}



}